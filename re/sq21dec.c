/* sq21 bitstream decoder + raster helpers — C port of re/sq21dec.py.
 *
 * The sq21/XRC raster codec (single interleaved MSB-first bitstream), as
 * reverse-engineered from Fuji's FXM_ALC (Xerox Phaser 6125N / DocuPrint
 * C525-A) and cross-checked against Dave Coffin's hbpldecode.c:
 *   tokens (HID[hsel], canonical Huffman, 2 rows):
 *     k=0..4 : run of `run` pixels copied from predictor order[k] (move-to-front)
 *     k=5    : diff — 4 byte-diffs (HPE0) added to the left pixel
 *     k=6    : switch hsel row ('11111' + one extra 0 bit)
 *   predictors: 0=left 1=up 2=up-left 3=up-right 4=up+2right; out-of-image = 0xff
 *   bit bookkeeping: token index = bdir - bit, bit = (run % 17057 != 0),
 *   bit = 0 after a diff and after each 17057-chunk.
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -Wpedantic -Werror -std=c99 -o sq21dec sq21dec.c
 *   gcc -O2 -Wall -Wextra -Wpedantic -Werror -std=c99 -shared -fPIC \
 *       -o libsq21dec.so sq21dec.c
 *
 * CLI (same as sq21dec.py):
 *   sq21dec <fxr-or-raw-sq21> <W> <H> [offset] [out.raw]
 *   A 44-byte FXR header (11x uint32 LE, f9==2) is auto-detected and skipped.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* exported library API (also used via ctypes from verify-native.py) */
int sq21_decode(const uint8_t *payload, size_t plen, int W, int H,
                uint8_t *out, size_t out_cap, size_t *bits_used, long *ntok_out);
int sq21_expand(const uint8_t *gpix, int gw, int gh, int cx, int cy,
                int cw, int ch, int W4, int gray, uint8_t *out, size_t out_cap);

/* ------------------------------------------------------------------ */
/* codebooks                                                           */
/* ------------------------------------------------------------------ */

/* HID[hsel][k] = { code, length } */
static const int HID[2][7][2] = {
    { {0,1},{6,3},{28,5},{29,5},{30,5},{2,2},{31,5} },
    { {2,2},{6,3},{28,5},{29,5},{30,5},{0,1},{31,5} },
};
static int hid_inv[2][6][32];          /* [hsel][len 1..5][code] -> k, -1 if none */

static int hpe0_code[256], hpe0_len[256];
static int hpe0_inv[12][2048];         /* [len 1..11][code] -> d (0..255), -1 */

static const int RL_GROUP[8]  = { 26, 34, 42, 50, 66, 98, 162, 674 };
static const int RL_ADDBITS[8] = { 3, 3, 3, 4, 5, 6, 9, 14 };

static const int PRED[5][2] = { {-1,0},{0,-1},{-1,-1},{1,-1},{2,-1} };

static int g_init = 0;

static void sq21_init(void)
{
    if (g_init) return;
    memset(hid_inv, -1, sizeof hid_inv);
    for (int h = 0; h < 2; h++)
        for (int k = 0; k < 7; k++)
            hid_inv[h][HID[h][k][1]][HID[h][k][0]] = k;

    /* HPE0[d] = (code, length) for byte-diff d = (cur-left)&0xff — same
     * codebook as Dave Coffin's put_diff()/get_diff(). */
    static const int base[6]   = { 2, 4, 8, 16, 48, 80 };
    static const int prefix[6] = { 0x3, 0x4, 0x5, 0x6, 0xe, 0xf }; /* 011 100 101 110 1110 1111 */
    static const int plen[6]   = { 3, 3, 3, 3, 4, 4 };
    static const int ebits[6]  = { 1, 2, 3, 5, 5, 6 };
    memset(hpe0_inv, -1, sizeof hpe0_inv);
    for (int d = 0; d < 256; d++) {
        int val = d < 128 ? d : d - 256;
        if (val == 0)       { hpe0_code[d] = 0; hpe0_len[d] = 3; }
        else if (val == 1)  { hpe0_code[d] = 1; hpe0_len[d] = 3; }
        else if (val == -1) { hpe0_code[d] = 2; hpe0_len[d] = 3; }
        else {
            int sign = val < 0 ? 1 : 0;
            int a = val < 0 ? -val : val;
            for (int g = 0; g < 6; g++) {
                if (a < base[g] + (1 << ebits[g])) {
                    hpe0_code[d] = (prefix[g] << (1 + ebits[g]))
                                 | (sign << ebits[g]) | (a - base[g]);
                    hpe0_len[d] = plen[g] + 1 + ebits[g];
                    break;
                }
            }
        }
        hpe0_inv[hpe0_len[d]][hpe0_code[d]] = d;
    }
    g_init = 1;
}

/* ------------------------------------------------------------------ */
/* MSB-first bit reader                                                */
/* ------------------------------------------------------------------ */

typedef struct { const uint8_t *d; size_t len; size_t pos; } BitR;

static int br_bit(BitR *br)
{
    size_t byte = br->pos >> 3;
    int b = 0;
    if (byte < br->len)
        b = (br->d[byte] >> (7 - (br->pos & 7))) & 1;
    br->pos++;
    return b;
}

static unsigned br_bits(BitR *br, int n)
{
    unsigned v = 0;
    while (n-- > 0)
        v = (v << 1) | (unsigned)br_bit(br);
    return v;
}

/* ------------------------------------------------------------------ */
/* token readers                                                       */
/* ------------------------------------------------------------------ */

static int read_token(BitR *br, int hid)
{
    unsigned code = 0;
    int len = 0;
    for (;;) {
        code = (code << 1) | (unsigned)br_bit(br);
        if (++len > 5)
            return -1;
        int k = hid_inv[hid][len][code];
        if (k >= 0)
            return k;
    }
}

static int read_run(BitR *br)
{
    if (!br_bit(br)) {
        if (!br_bit(br)) return 1;
        return 2 + br_bit(br);
    }
    if (!br_bit(br)) {
        if (!br_bit(br)) return 4 + br_bit(br);
        return 6 + (int)br_bits(br, 2);
    }
    if (!br_bit(br))
        return 10 + (int)br_bits(br, 4);
    int g = (int)br_bits(br, 3);
    int n = RL_ADDBITS[g];
    return RL_GROUP[g] + (n ? (int)br_bits(br, n) : 0);
}

/* returns signed diff (-128..127), or 1000 on a bad code */
static int read_diff(BitR *br)
{
    unsigned code = 0;
    int len = 0;
    for (;;) {
        code = (code << 1) | (unsigned)br_bit(br);
        if (++len > 11)
            return 1000;
        int d = hpe0_inv[len][code];
        if (d >= 0)
            return d < 128 ? d : d - 256;
    }
}

/* ------------------------------------------------------------------ */
/* decoder (exported)                                                  */
/* ------------------------------------------------------------------ */

int sq21_decode(const uint8_t *payload, size_t plen, int W, int H,
                uint8_t *out, size_t out_cap, size_t *bits_used, long *ntok_out)
{
    sq21_init();
    if (W <= 0 || H <= 0)
        return -2;
    size_t need = (size_t)W * (size_t)H * 4;
    if (out_cap < need)
        return -3;

    static const uint8_t FF4[4] = { 0xff, 0xff, 0xff, 0xff };
    BitR br = { payload, plen, 0 };
    size_t slots = (size_t)W + 4;
    uint8_t *prev = (uint8_t *)malloc(slots * 4);
    uint8_t *cur  = (uint8_t *)malloc(slots * 4);
    if (!prev || !cur) {
        free(prev);
        free(cur);
        return -4;
    }
    memset(prev, 0xff, slots * 4);
    memset(cur, 0xff, slots * 4);

    int hid = 0, bit = 0;
    int order[5] = { 0, 1, 2, 3, 4 };
    long x = 0, y = 0;
    size_t npix = 0, ntokens = 0;
    size_t total = (size_t)W * (size_t)H;
    int rc = 0;

    while (npix < total) {
        int k = read_token(&br, hid);
        if (k < 0) { rc = -1; break; }
        ntokens++;
        if (k == 6) {
            hid ^= 1;
            br_bit(&br);            /* the extra 0 bit */
            continue;
        }
        if (k == 5) {
            const uint8_t *left = x > 0 ? cur + (size_t)x * 4 : FF4;
            uint8_t px[4];
            for (int j = 0; j < 4; j++) {
                int d = read_diff(&br);
                if (d > 127) { rc = -1; break; }
                px[j] = (uint8_t)((left[j] + d) & 0xff);
            }
            if (rc) break;
            memcpy(cur + (size_t)(x + 1) * 4, px, 4);
            memcpy(out + ((size_t)y * (size_t)W + (size_t)x) * 4, px, 4);
            x++;
            npix++;
            bit = 0;
        } else {
            int run = read_run(&br);
            int bdir = k + bit;
            bit = run < 17057 ? 1 : 0;
            int pid = order[bdir];
            if (bdir) {             /* move-to-front */
                memmove(&order[1], &order[0], (size_t)bdir * sizeof(int));
                order[0] = pid;
            }
            int dx = PRED[pid][0], dy = PRED[pid][1];
            for (int r = 0; r < run && npix < total; r++) {
                const uint8_t *p;
                if (dy == 0)
                    p = x > 0 ? cur + (size_t)x * 4 : FF4;
                else {
                    long pxi = x + dx;
                    p = (pxi >= 0 && pxi < W) ? prev + (size_t)(pxi + 1) * 4 : FF4;
                }
                memcpy(cur + (size_t)(x + 1) * 4, p, 4);
                memcpy(out + ((size_t)y * (size_t)W + (size_t)x) * 4, p, 4);
                x++;
                npix++;
                if (x == W) {
                    uint8_t *t = prev;
                    prev = cur;
                    cur = t;
                    memset(cur, 0xff, slots * 4);
                    x = 0;
                    y++;
                }
            }
        }
    }
    free(prev);
    free(cur);
    if (bits_used)
        *bits_used = br.pos;
    if (ntok_out)
        *ntok_out = (long)ntokens;
    return rc;
}

/* ------------------------------------------------------------------ */
/* expected-image builder (exported): crop + expand RGB24 -> ffRGB + pad */
/* ------------------------------------------------------------------ */

int sq21_expand(const uint8_t *gpix, int gw, int gh, int cx, int cy,
                int cw, int ch, int W4, int gray, uint8_t *out, size_t out_cap)
{
    if (cw < 0 || ch < 0 || cx < 0 || cy < 0 || W4 < cw)
        return -2;
    if ((long)cx + cw > gw || (long)cy + ch > gh)
        return -2;
    size_t need = (size_t)W4 * (size_t)ch * 4;
    if (out_cap < need)
        return -3;
    for (int y = 0; y < ch; y++) {
        const uint8_t *srow = gpix + ((size_t)(cy + y) * (size_t)gw + (size_t)cx) * 3;
        uint8_t *drow = out + (size_t)y * (size_t)W4 * 4;
        for (int x = 0; x < cw; x++) {
            unsigned r = srow[x * 3], g = srow[x * 3 + 1], b = srow[x * 3 + 2];
            if (gray) {
                unsigned v = (r + g + b) / 3;
                r = g = b = v;
            }
            uint8_t *d = drow + (size_t)x * 4;
            d[0] = 0xff;
            d[1] = (uint8_t)r;
            d[2] = (uint8_t)g;
            d[3] = (uint8_t)b;
        }
        for (int x = cw; x < W4; x++) {
            uint8_t *d = drow + (size_t)x * 4;
            d[0] = d[1] = d[2] = d[3] = 0xff;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* CLI (same interface as sq21dec.py)                                  */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: sq21dec <fxr-or-raw-sq21> <W> <H> [offset] [out.raw]\n");
        fprintf(stderr, "  if the file starts with a 44-byte FXR header (f9==2), it is skipped\n");
        return 1;
    }
    const char *fn = argv[1];
    long W = atol(argv[2]);
    long H = atol(argv[3]);
    size_t off = argc > 4 ? (size_t)atol(argv[4]) : 0;
    const char *outfn = argc > 5 ? argv[5] : NULL;

    FILE *f = fopen(fn, "rb");
    if (!f) {
        fprintf(stderr, "sq21dec: cannot open %s\n", fn);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = (uint8_t *)malloc((size_t)flen);
    if (!d || fread(d, 1, (size_t)flen, f) != (size_t)flen) {
        fprintf(stderr, "sq21dec: cannot read %s\n", fn);
        return 1;
    }
    fclose(f);

    if (flen > 44 && memcmp(d, "\0\0\0\0", 4) != 0) {
        /* FXR header = 11x uint32 LE (little-endian host assumed) */
        uint32_t hdr[11];
        memcpy(hdr, d, 44);
        if (hdr[9] == 2) {
            off = 44;
            if (W == 0) W = (long)hdr[0];
            if (H == 0) H = (long)hdr[1];
            printf("FXR header: w=%u h=%u bpp=%u f9=%u f10=%u -> payload at 44\n",
                   hdr[0], hdr[1], hdr[2], hdr[9], hdr[10]);
        }
    }
    if (W <= 0 || H <= 0 || off >= (size_t)flen) {
        fprintf(stderr, "sq21dec: bad dimensions or offset\n");
        return 1;
    }
    size_t plen = (size_t)flen - off;
    uint8_t *img = (uint8_t *)malloc((size_t)W * (size_t)H * 4);
    if (!img) {
        fprintf(stderr, "sq21dec: out of memory\n");
        return 1;
    }
    size_t bits = 0;
    long ntok = 0;
    int rc = sq21_decode(d + off, plen, (int)W, (int)H, img,
                         (size_t)W * (size_t)H * 4, &bits, &ntok);
    if (rc != 0) {
        fprintf(stderr, "sq21dec: decode failed (rc=%d)\n", rc);
        return 1;
    }
    printf("decoded %ld pixels (%ld bytes) from %zu payload bytes, %ld tokens, %zu bits consumed\n",
           W * H, W * H * 4, plen, ntok, bits);
    if (outfn) {
        FILE *o = fopen(outfn, "wb");
        if (!o || fwrite(img, 1, (size_t)W * (size_t)H * 4, o) != (size_t)W * (size_t)H * 4) {
            fprintf(stderr, "sq21dec: cannot write %s\n", outfn);
            return 1;
        }
        fclose(o);
        printf("wrote %s\n", outfn);
    }
    free(img);
    free(d);
    return 0;
}
