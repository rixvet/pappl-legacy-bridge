/* hbpl_core.c — portable core of the Phaser 6125N HBPL encoder.
 *
 * Everything except the CUPS-filter main(): the sq21/HBPL1 codec, the HBPL
 * container records, the FXM_CC colour calibration (CST + 3D DLUT), the
 * PPM (P6) reader, the PJL header writer, the per-page processor and the
 * job trailer.  Pure C99 + stdio only — no POSIX — so it compiles for
 * Windows (MinGW/MSVC) as well as Linux:
 *
 *   Linux filter : native/foo2sq21hbpl.c   (main: fork gs, pipe the PPM)
 *   Windows tool : win/hbblenc.c           (main: PPM on stdin -> HBPL on stdout)
 *   PAPPL app    : pappl/phaser6125-pappl.c (MIME filter: PDF -> gs -> PPM -> hbpl_page)
 *
 * The pixel pipeline and byte output are shared verbatim between all
 * consumers; verified byte-identical (re/verify-native.py and the
 * binary-identity regression against the installed CUPS filter).
 */
#include "hbpl_core.h"
#include "cst_tables.h"   /* generated: CST_TRC / CST_ITRC / CST_M (FXM_CC) */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* sq21 / HBPL1 ("XRC") codec — tables reverse engineered from FXM_ALC  */
/* ------------------------------------------------------------------ */

typedef struct { unsigned code; int len; } Code;

/* HID[hsel][token]: token 0..4 = run (predictor slot), 5 = diff, 6 = switch row */
static const Code HID[2][7] = {
    { {0,1},{6,3},{28,5},{29,5},{30,5},{2,2},{31,5} },   /* text & graphics */
    { {2,2},{6,3},{28,5},{29,5},{30,5},{0,1},{31,5} },   /* images          */
};
/* HRN0[run] for run 1..25 (short codes) */
static const Code HRN0[26] = {
    {0,0},{0,2},{2,3},{3,3},{8,4},{9,4},{20,5},{21,5},{22,5},{23,5},
    {96,7},{97,7},{98,7},{99,7},{100,7},{101,7},{102,7},{103,7},{104,7},{105,7},
    {106,7},{107,7},{108,7},{109,7},{110,7},{111,7}
};
/* long run lengths: '111' + 3-bit group + extra bits; group g (7..14) */
static const int RL_GROUP16[16]  = {0,1,2,3,4,6,10,26,34,42,50,66,98,162,674,17058};
static const int RL_ADDBITS16[15] = {0,0,0,0,1,2,4,3,3,3,4,5,6,9,14};

/* HPE0[d]: byte-diff codebook, indexed by (cur-left)&0xff (signed value) */
static Code HPE0[256];

static void init_hpe0(void)
{
    /* groups: base, prefix code, prefix length, extra bits */
    static const struct { int base, prefix, plen, ebits; } g[6] = {
        { 2,  0x3, 3, 1 }, { 4,  0x4, 3, 2 }, { 8,  0x5, 3, 3 },
        { 16, 0x6, 3, 5 }, { 48, 0xe, 4, 5 }, { 80, 0xf, 4, 6 }
    };
    for (int d = 0; d < 256; d++) {
        int val = d < 128 ? d : d - 256;
        int sign = val < 0, a = val < 0 ? -val : val;
        if (val == 0)  { HPE0[d].code = 0; HPE0[d].len = 3; continue; }
        if (val == 1)  { HPE0[d].code = 1; HPE0[d].len = 3; continue; }
        if (val == -1) { HPE0[d].code = 2; HPE0[d].len = 3; continue; }
        for (int i = 0; i < 6; i++) {
            if (a < g[i].base + (1 << g[i].ebits)) {
                HPE0[d].code = ((unsigned)g[i].prefix << (1 + g[i].ebits))
                             | ((unsigned)sign << g[i].ebits)
                             | (unsigned)(a - g[i].base);
                HPE0[d].len = g[i].plen + 1 + g[i].ebits;
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* MSB-first bit writer with a growable buffer                         */
/* ------------------------------------------------------------------ */

void bw_init(BitW *b)
{
    b->cap = 1 << 16; b->len = 0;
    b->buf = (unsigned char *)malloc(b->cap);
    if (!b->buf) { fprintf(stderr, "hbpl_core: out of memory\n"); exit(1); }
    b->acc = 0; b->nbits = 0;
}
static void bw_need(BitW *b, size_t n)
{
    if (b->len + n <= b->cap) return;
    while (b->len + n > b->cap) b->cap *= 2;
    b->buf = (unsigned char *)realloc(b->buf, b->cap);
    if (!b->buf) { fprintf(stderr, "hbpl_core: out of memory\n"); exit(1); }
}
static void bw_bits(BitW *b, unsigned val, int n)
{
    if (n <= 0) return;
    b->acc = (b->acc << n) | (val & ((1ULL << n) - 1));
    b->nbits += n;
    while (b->nbits >= 8) {
        b->nbits -= 8;
        bw_need(b, 1);
        b->buf[b->len++] = (unsigned char)(b->acc >> b->nbits);
    }
    b->acc &= (1ULL << b->nbits) - 1;
}

/* ------------------------------------------------------------------ */
/* sq21 lossless encoder                                               */
/*                                                                     */
/* Image layout: (H+1) rows of (W+2) 4-byte pixel slots.                */
/*   row 0            = virtual "previous row" for y=0, all 0xff        */
/*   rows 1..H        = image rows; slot 0 = left margin (0xff),       */
/*                      slots 1..W = pixels, slot W+1 = right sentinel  */
/*                      (0xff, like FXM_ALC's fire_sentinel).           */
/* Predictors (semantic): 0=left 1=up 2=up-left 3=up-right 4=up+2right  */
/* ------------------------------------------------------------------ */

static const int PRED_DX[4] = { 0, -1, 1, 2 };   /* for pid 1..4 */

static const unsigned char FF4[4] = { 0xff, 0xff, 0xff, 0xff };

static inline const unsigned char *px_at(const unsigned char *img, int W, int x, int y)
{
    return img + ((size_t)(y + 1) * (size_t)(W + 2) + (size_t)(x + 1)) * 4;
}
static inline const unsigned char *pred_at(const unsigned char *img, int W, int x, int y, int pid)
{
    int slot;
    if (pid == 0)   /* left: current row, slot x (x==0 -> margin) */
        return img + ((size_t)(y + 1) * (size_t)(W + 2) + (size_t)x) * 4;
    slot = x + 1 + PRED_DX[pid - 1];
    if (slot < 0 || slot > W + 1) return FF4;   /* beyond the sentinel -> margin */
    return img + ((size_t)y * (size_t)(W + 2) + (size_t)slot) * 4;
}

static void put_token(BitW *b, int hsel, int token)
{
    bw_bits(b, HID[hsel][token].code, HID[hsel][token].len);
}
static void put_runlen(BitW *b, int run)   /* 1..17057 */
{
    int g = 7, n;
    if (run <= 25) { bw_bits(b, HRN0[run].code, HRN0[run].len); return; }
    if (run >= RL_GROUP16[8]) { g = 8; while (run >= RL_GROUP16[g + 1]) g++; }
    bw_bits(b, (unsigned)(g + 49), 6);
    n = RL_ADDBITS16[g];
    if (n) bw_bits(b, (unsigned)(run - RL_GROUP16[g]), n);
}

/* Encode the whole image (W x H pixels, 4 bytes each) into b. */
void sq21_encode(BitW *b, int W, int H, const unsigned char *img)
{
    static int hpe0_ready = 0;
    int rotor[5] = { 0, 1, 2, 3, 4 };   /* move-to-front order of predictor ids */
    int hsel = 0, bit = 0, stat = 0;
    long total = (long)W * H;
    long off = 0;

    if (!hpe0_ready) { init_hpe0(); hpe0_ready = 1; }

    while (off < total) {
        int x = (int)(off % W), y = (int)(off / W);

        /* longest run over the 5 predictors, in MTF order (first max wins) */
        int bdir = 0, brun = 0;
        for (int dir = 0; dir < 5; dir++) {
            int pid = rotor[dir];
            long o = off;
            int run = 0;
            while (run < 17057 && o < total) {
                int xx = (int)(o % W), yy = (int)(o / W);
                if (memcmp(px_at(img, W, xx, yy), pred_at(img, W, xx, yy, pid), 4)) break;
                run++; o++;
            }
            if (run > brun) { bdir = dir; brun = run; }
        }

        if (brun == 0) {
            /* diff against the left pixel (margin 0xff at x==0) */
            const unsigned char *px = px_at(img, W, x, y);
            const unsigned char *lp = (x == 0) ? FF4 : px_at(img, W, x - 1, y);
            put_token(b, hsel, 5);
            for (int j = 0; j < 4; j++)
                bw_bits(b, HPE0[(px[j] - lp[j]) & 0xff].code, HPE0[(px[j] - lp[j]) & 0xff].len);
            bit = 0;
            stat--;
            off++;
            continue;
        }
        if (brun > total - off) brun = (int)(total - off);

        if (bdir) {   /* move-to-front (decoder does this when raw != 0) */
            int j = rotor[bdir];
            for (int i = bdir; i > 0; i--) rotor[i] = rotor[i - 1];
            rotor[0] = j;
        }
        stat += (bdir == bit);
        put_token(b, hsel, bdir - bit);
        put_runlen(b, brun);
        bit = (brun < 17057);
        off += brun;

        /* adaptive switch of the Huffman row when the balance drifts */
        if (stat > 8 || stat < -8) {
            int newhid = (stat < 0) ? 1 : 0;
            if (newhid != hsel) {
                put_token(b, hsel, 6);   /* '11111' */
                bw_bits(b, 0, 1);        /* the extra 0 bit */
                hsel = newhid;
            }
        }
    }

    /* end marker: HID[6] + '111', then pad the last byte with 1-bits */
    put_token(b, hsel, 6);
    bw_bits(b, 7, 3);
    if (b->nbits)
        bw_bits(b, (1u << (8 - b->nbits)) - 1, 8 - b->nbits);
}

/* ------------------------------------------------------------------ */
/* Fuji HBPL container (byte-exact template from the original driver)   */
/* ------------------------------------------------------------------ */

static const unsigned char A_REC[12] = {
    0x41, 0x81, 0xa1, 0x00, 0x82, 0xa2, 0x01, 0x00, 0x83, 0xa2, 0x01, 0x00
};
static const unsigned char BODY[24] = {
    0x18, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x10, 0x32, 0x04, 0x00,
    0xa1, 0x42, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00
};
/* C record (78 bytes).  Patch points:
 *   +24..27  margins (2x uint16 LE)   +33..36 pageno (uint32 LE)
 *   +39..42 w, +43..46 h (uint32 LE) +70..73 w, +74..77 h (uint32 LE)  */
static const unsigned char C_REC_TPL[78] = {
    0x43, 0x91, 0xa1, 0x00, 0x92, 0xa1, 0x00, 0x93, 0xa1, 0x01, 0x94, 0xa1,
    0x02, 0x95, 0xc2, 0x00, 0x00, 0x00, 0x00, 0x96, 0xa1, 0x03, 0x97, 0xc3,
    0x5e, 0x00, 0x5e, 0x00, 0x98, 0xa1, 0x00, 0x99, 0xa4, 0x01, 0x00, 0x00,
    0x00, 0x9a, 0xc4, 0xa8, 0x12, 0x00, 0x00, 0xad, 0x1a, 0x00, 0x00, 0x9b,
    0xa1, 0x00, 0x9c, 0xa1, 0x01, 0x9d, 0xa1, 0x16, 0x9e, 0xa1, 0x00, 0x9f,
    0xa1, 0x01, 0xa0, 0xa1, 0x20, 0xa1, 0xa1, 0x00, 0xa2, 0xc4, 0xa8, 0x12,
    0x00, 0x00, 0xad, 0x1a, 0x00, 0x00
};
/* QR record (12 bytes); +8..11 = total = 24 + payload length (uint32 LE) */
static const unsigned char QR_REC_TPL[12] = {
    0x51, 0x52, 0xa3, 0xa1, 0x00, 0xa4, 0xb1, 0xa4, 0, 0, 0, 0
};

static void put_u16le(unsigned char *p, unsigned v) { p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; }
static void put_u32le(unsigned char *p, unsigned long v)
{
    p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; p[2] = (v >> 16) & 0xff; p[3] = (v >> 24) & 0xff;
}

/* ------------------------------------------------------------------ */
/* PPD parsing (minimal: PaperDimension + ImageableArea)               */
/* ------------------------------------------------------------------ */

/* Look up `*Key <pagesize>/...: "v1 v2 ..."` in the PPD.  Returns count. */
static int ppd_lookup(const char *ppdpath, const char *pagesize, const char *key,
                      double *vals, int nvals)
{
    FILE *f;
    char line[1024];
    char pat[128];
    char *q, *s, *end;
    int n = 0;
    if (!ppdpath) return 0;
    f = fopen(ppdpath, "r");
    if (!f) return 0;
    snprintf(pat, sizeof pat, "*%s %s/", key, pagesize);
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, pat, strlen(pat))) continue;
        q = strchr(line, ':');
        if (!q) continue;
        q++;
        s = strchr(q, '"');
        if (!s) continue;
        s++;
        for (int i = 0; i < nvals; i++) {
            while (*s && isspace((unsigned char)*s)) s++;
            if (!*s || *s == '"') break;
            vals[i] = strtod(s, &end);
            if (end == s) break;
            s = end;
            n = i + 1;
        }
        break;
    }
    fclose(f);
    return n;
}

/* ------------------------------------------------------------------ */
/* FXM_CC colour calibration (byte-exact re-implementation)             */
/*                                                                     */
/* The original FXM_CC stage transforms every pixel as                  */
/*     colour:  out = 3Dlut(CST(in))                                   */
/*     gray:    out = NTSC(CST(in))                                    */
/* where CST is a gamma-2.2 round trip (8->16 bit TRC, 3x3 matrix in    */
/* signed Q16 fixed point, clamp to 12 bit, 4096-entry inverse TRC)    */
/* and 3Dlut is a tetrahedral 17^3 LUT loaded from the .dlut file.      */
/* The whole chain is verified byte-exact against the original binary  */
/* (re/verify_model.c: all 256^3 colours + a real page).               */
/*                                                                     */
/* .dlut file format (all big-endian): "DLUT", u32 block count,        */
/* count x u32 block offsets, u32 metadata count, metadata count x     */
/* u32 values.  The block is selected by metadata[FXDLUT] (1-based);   */
/* its data starts at offsets[block-1] + 8 (skipping the 8-byte        */
/* 'dlt4' header) and is 20420 bytes: 768 bytes of 1D curves (unused   */
/* by FXM_CC's pixel path) followed by 17^3 nodes of 4 bytes           */
/* [pad, out0, out1, out2].                                            */
/* ------------------------------------------------------------------ */

static uint8_t *dlut_nodes = NULL;   /* 17^3 * 4 bytes, file byte order */

static uint32_t rd32be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Load the 3D LUT block for `fxdlut` (the FXDLUT value belonging to the
 * PPD FXDocumentType).  Returns 0 on success, -1 on an unusable file
 * (the caller then falls back to CST-only, like FXM_CC itself). */
static int dlut_load(const char *path, int fxdlut)
{
    FILE *f;
    uint8_t hdr[8];
    uint32_t count;
    uint32_t *offs;
    uint8_t *raw = NULL;
    uint8_t b[4];
    uint32_t metacount;
    uint32_t block;
    int rc = -1;
    f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(hdr, 1, 8, f) != 8 || memcmp(hdr, "DLUT", 4) != 0) { fclose(f); return -1; }
    count = rd32be(hdr + 4);
    if (count == 0 || count > 4096) { fclose(f); return -1; }
    offs = (uint32_t *)malloc(count * sizeof *offs);
    if (!offs) { fclose(f); return -1; }
    for (uint32_t i = 0; i < count; i++) {
        if (fread(b, 1, 4, f) != 4) goto out;
        offs[i] = rd32be(b);
    }
    if (fread(b, 1, 4, f) != 4) goto out;            /* metadata count */
    metacount = rd32be(b);
    if (fxdlut < 0 || (uint32_t)fxdlut >= metacount) goto out;
    if (fseek(f, (long)fxdlut * 4, SEEK_CUR) != 0) goto out;   /* skip to meta[fxdlut] */
    if (fread(b, 1, 4, f) != 4) goto out;
    block = rd32be(b);                      /* 1-based block number */
    if (block < 1 || block > count) goto out;
    /* block data: offsets[block-1] + 8, 20420 bytes (768 curves + 17^3*4 LUT) */
    if (fseek(f, (long)offs[block - 1] + 8, SEEK_SET) != 0) goto out;
    raw = (uint8_t *)malloc(20420);
    if (!raw) goto out;
    if (fread(raw, 1, 20420, f) != 20420) goto out;
    dlut_nodes = raw + 768;    /* 17^3 nodes x 4 bytes, [pad, out0, out1, out2] */
    raw = NULL;                /* the block stays alive via dlut_nodes */
    rc = 0;
out:
    free(raw);
    free(offs);
    fclose(f);
    return rc;
}

/* CST stage: gamma-2.2 round trip, 8-bit in -> 8-bit out (lossy 8->16->12->8). */
static void calib_cst(const uint8_t in[3], uint8_t out[3])
{
    int32_t v0 = CST_TRC[in[0]];
    int32_t v1 = CST_TRC[in[1]];
    int32_t v2 = CST_TRC[in[2]];
    /* 3x3 matrix, signed Q16: (M>>2)*v >> 18 == m*v/16 (16 -> 12 bit) */
    int32_t o0 = ((CST_M[0] >> 2) * v0 + (CST_M[1] >> 2) * v1 + (CST_M[2] >> 2) * v2) >> 18;
    int32_t o1 = ((CST_M[3] >> 2) * v0 + (CST_M[4] >> 2) * v1 + (CST_M[5] >> 2) * v2) >> 18;
    int32_t o2 = ((CST_M[6] >> 2) * v0 + (CST_M[7] >> 2) * v1 + (CST_M[8] >> 2) * v2) >> 18;
    if (o0 < 0) o0 = 0; else if (o0 > 0xfff) o0 = 0xfff;
    if (o1 < 0) o1 = 0; else if (o1 > 0xfff) o1 = 0xfff;
    if (o2 < 0) o2 = 0; else if (o2 > 0xfff) o2 = 0xfff;
    out[0] = CST_ITRC[o0];
    out[1] = CST_ITRC[o1];
    out[2] = CST_ITRC[o2];
}

static void dlut_tetra(const uint8_t c[3], uint8_t out[3]);

/* Full FXM_CC colour transform: out = 3Dlut(CST(in)).
 * Without .dlut: CST only (like the original when the file is missing). */
static void calib_pixel(const uint8_t in[3], uint8_t out[3])
{
    uint8_t c[3];
    calib_cst(in, c);
    if (!dlut_nodes) {
        out[0] = c[0]; out[1] = c[1]; out[2] = c[2];
        return;
    }
    dlut_tetra(c, out);
}

/* Tetrahedral interpolation in the 17^3 LUT (FXM_CC cil_3Dlut_Conv_sBGR).
 * Node index: i = c[2]>>4 (x289), j = c[1]>>4 (x17), k = c[0]>>4 (x1);
 * node bytes: [pad, out0, out1, out2].  The 4 weights are the differences
 * of the descending-sorted 4-bit fractions; they sum to 16. */
static void dlut_tetra(const uint8_t c[3], uint8_t out[3])
{
    int base, w0, w1, w2, w3, s0, s1, s2;
    int f[3], d[3];
    const uint8_t *n0, *nA, *nAB, *nABC;
    if ((c[0] & c[1] & c[2]) == 0xff) {    /* white shortcut */
        out[0] = out[1] = out[2] = 0xff;
        return;
    }
    base = (c[2] >> 4) * 289 + (c[1] >> 4) * 17 + (c[0] >> 4);
    f[0] = c[2] & 0xf; f[1] = c[1] & 0xf; f[2] = c[0] & 0xf;
    d[0] = 289; d[1] = 17; d[2] = 1;
    for (int a = 1; a < 3; a++) {           /* sort descending by fraction */
        int fv = f[a], dv = d[a], b = a - 1;
        while (b >= 0 && f[b] < fv) { f[b + 1] = f[b]; d[b + 1] = d[b]; b--; }
        f[b + 1] = fv; d[b + 1] = dv;
    }
    w0 = 16 - f[0]; w1 = f[0] - f[1]; w2 = f[1] - f[2]; w3 = f[2];
    n0 = dlut_nodes + (size_t)base * 4;
    nA = dlut_nodes + (size_t)(base + d[0]) * 4;
    nAB = dlut_nodes + (size_t)(base + d[0] + d[1]) * 4;
    nABC = dlut_nodes + (size_t)(base + 307) * 4;    /* c111 */
    /* node bytes: [pad, out0, out1, out2] */
    s0 = w0 * n0[1] + w1 * nA[1] + w2 * nAB[1] + w3 * nABC[1];
    s1 = w0 * n0[2] + w1 * nA[2] + w2 * nAB[2] + w3 * nABC[2];
    s2 = w0 * n0[3] + w1 * nA[3] + w2 * nAB[3] + w3 * nABC[3];
    out[0] = (uint8_t)(s0 >> 4);    /* weights sum to 16, so s>>4 <= 255 */
    out[1] = (uint8_t)(s1 >> 4);
    out[2] = (uint8_t)(s2 >> 4);
}

/* 1-pixel cache, like FXM_CC's main loop (the transform is pure). */
static void calib_pixel_cached(const uint8_t in[3], uint8_t out[3])
{
    static uint32_t c_in = 0xffffffffu;
    static uint8_t c_out[3];
    uint32_t k = (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16);
    if (k == c_in) {
        out[0] = c_out[0]; out[1] = c_out[1]; out[2] = c_out[2];
        return;
    }
    calib_pixel(in, out);
    c_in = k;
    c_out[0] = out[0]; c_out[1] = out[1]; c_out[2] = out[2];
}

/* ------------------------------------------------------------------ */
/* PPM (P6) stream reader                                              */
/* ------------------------------------------------------------------ */

static int ppm_getc(FILE *f)   /* skips whitespace and # comments */
{
    int c;
    for (;;) {
        c = fgetc(f);
        if (c == '#') { while (c != '\n' && c != EOF) c = fgetc(f); continue; }
        if (c == EOF || !isspace(c)) return c;
    }
}
int ppm_next(Ppm *p)    /* read the next image header; 0 at EOF */
{
    int c = ppm_getc(p->f);
    long v[3];
    int nv = 0;
    if (c == EOF) return 0;
    if (c != 'P') return -1;
    c = ppm_getc(p->f);
    if (c != '6') return -1;
    while (nv < 3) {
        long x = 0;
        c = ppm_getc(p->f);
        if (c == EOF) return -1;
        if (!isdigit(c)) return -1;
        while (isdigit(c)) { x = x * 10 + (c - '0'); c = fgetc(p->f); }
        if (c == '#') { while (c != '\n' && c != EOF) c = fgetc(p->f); }
        else if (c != EOF) ungetc(c, p->f);
        v[nv++] = x;
    }
    /* exactly one whitespace character separates header and data */
    c = fgetc(p->f);
    if (c == EOF || !isspace(c)) return -1;
    p->w = (int)v[0]; p->h = (int)v[1]; p->maxval = (int)v[2];
    if (p->w <= 0 || p->h <= 0 || p->maxval != 255) return -1;
    free(p->row);
    p->row = (unsigned char *)malloc((size_t)p->w * 3);
    if (!p->row) return -1;
    return 1;
}

/* ------------------------------------------------------------------ */
/* portable case-insensitive compare (replaces POSIX strcasecmp)       */
/* ------------------------------------------------------------------ */

static int hbpl_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* ------------------------------------------------------------------ */
/* options parsing (CUPS "key=value key=value ..." string)             */
/* ------------------------------------------------------------------ */

void hbpl_parse_options(const char *opts, char *pagesize, size_t psz,
                        int *dpi, int *gray, int *fxdlut)
{
    const char *p = opts;
    char key[128], val[256];

    while (*p) {
        int ki, vi;
        /* skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        /* read key */
        ki = 0;
        while (*p && *p != '=' && *p != ' ' && *p != '\t' && ki < (int)sizeof(key) - 1)
            key[ki++] = *p++;
        key[ki] = 0;
        if (*p != '=') continue;   /* no value, skip */
        p++;                       /* skip '=' */
        /* read value */
        vi = 0;
        while (*p && *p != ' ' && *p != '\t' && vi < (int)sizeof(val) - 1)
            val[vi++] = *p++;
        val[vi] = 0;

        if (!strcmp(key, "PageSize") || !strcmp(key, "PageRegion"))
            snprintf(pagesize, psz, "%s", val);
        else if (!strcmp(key, "FXOutputMode")) {
            int d = atoi(val);
            if (d > 0) *dpi = d;
        } else if (!strcmp(key, "FXColorMode"))
            *gray = (strcmp(val, "Color") != 0);
        else if (!strcmp(key, "FXDocumentType")) {
            /* FXDLUT codes from the PPD setpagedevice rules */
            if (!strcmp(val, "Photo")) *fxdlut = 22;
            else if (!strcmp(val, "Presentation")) *fxdlut = 26;
            else if (!strcmp(val, "Web")) *fxdlut = 23;
            else if (!strcmp(val, "POP")) *fxdlut = 32;
            /* Normal (and default) = 24 */
        }
    }
}

/* ------------------------------------------------------------------ */
/* geometry: PPD PaperDimension + ImageableArea -> crop bounds         */
/* ------------------------------------------------------------------ */

void hbpl_geometry(const char *ppd_override, const char *pagesize,
                   int dpi, HbplPageCtx *ctx)
{
    const char *ppd = ppd_override;
    double v[4];
    double t;

    /* defaults (A4, pt) */
    ctx->pd[0] = 595; ctx->pd[1] = 842;
    ctx->ia[0] = 11.28; ctx->ia[1] = 11.28;
    ctx->ia[2] = 583.72; ctx->ia[3] = 830.72;

    if (!ppd || !*ppd) ppd = getenv("PPD");
    if (!ppd || !*ppd) ppd = getenv("CUPS_PPD");
    if (ppd) {
        if (ppd_lookup(ppd, pagesize, "PaperDimension", v, 2) == 2) {
            ctx->pd[0] = v[0]; ctx->pd[1] = v[1];
        }
        if (ppd_lookup(ppd, pagesize, "ImageableArea", v, 4) == 4) {
            ctx->ia[0] = v[0]; ctx->ia[1] = v[1];
            ctx->ia[2] = v[2]; ctx->ia[3] = v[3];
        }
    }

    ctx->rr = dpi / 72.0;
    t = floor(ctx->ia[0] * ctx->rr + 0.5);
    ctx->x0 = (int)t;                                  /* left   */
    t = floor(ctx->ia[2] * ctx->rr + 0.5);
    ctx->img_w = (int)t - ctx->x0;                     /* width  */
    /* HBPL C-record margins: from the PPD paper height (like FXM_HBPL) */
    t = floor(ctx->ia[0] * ctx->rr + 0.5);
    ctx->marg_x = (int)t;
    t = floor((ctx->pd[1] - ctx->ia[3]) * ctx->rr + 0.5);
    ctx->marg_y = (int)t;
}

/* ------------------------------------------------------------------ */
/* calibration setup (FOO2SQ_CALIBRATE / FOO2SQ_DLUT env vars)         */
/* ------------------------------------------------------------------ */

int hbpl_calib_setup(const char *prog, int gray, int fxdlut)
{
    const char *ce = getenv("FOO2SQ_CALIBRATE");
    const char *dlut_path;
    int calibrate = 1;

    if (ce && (!strcmp(ce, "0") || hbpl_ieq(ce, "no") || hbpl_ieq(ce, "off")))
        calibrate = 0;
    dlut_path = getenv("FOO2SQ_DLUT");
    if (!dlut_path || !*dlut_path)
        dlut_path = "/usr/share/phaser6125/Phaser_6125N.dlut";
    if (calibrate && !gray && dlut_load(dlut_path, fxdlut) != 0)
        fprintf(stderr, "%s: dlut %s unusable, CST calibration only\n",
                prog, dlut_path);

    return calibrate;
}

/* ------------------------------------------------------------------ */
/* PJL header + A record                                               */
/* ------------------------------------------------------------------ */

void hbpl_pjl_header(FILE *out, const char *title, const char *user,
                     const char *hostname, int copies, int dpi)
{
    char date[16], tim[16];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    strftime(date, sizeof date, "%m/%d/%Y", tm);
    strftime(tim, sizeof tim, "%H:%M:%S", tm);
    fprintf(out, "\x1b%%-12345X@PJL COMMENT DATE=%s\n", date);
    fprintf(out, "@PJL COMMENT TIME=%s\n", tim);
    fprintf(out, "@PJL COMMENT DNAME=%s\n", title);
    fprintf(out, "@PJL JOB MODE=PRINTER\n");
    fprintf(out, "@PJL SET JOBATTR=\"@LUNA=%s\"\n", user);
    fprintf(out, "@PJL SET JOBATTR=\"@JOAU=%s\"\n", user);
    fprintf(out, "@PJL SET JOBATTR=\"@CNAM=%s\"\n", hostname);
    fprintf(out, "@PJL SET JOBATTR=\"@HOAD=%s\"\n", hostname);
    fprintf(out, "@PJL SET JOBATTR=\"@NLPP=1\"\n");
    fprintf(out, "@PJL SET COPIES=%d\n", copies);
    fprintf(out, "@PJL SET RENDERMODE=COLOR\n");
    fprintf(out, "@PJL SET DUPLEX=OFF\n");
    fprintf(out, "@PJL SET OUTBIN=FACEDOWN\n");
    fprintf(out, "@PJL SET PAPERDIRECTION=SEF\n");
    fprintf(out, "@PJL SET JOBATTR=\"@MSIP=NORMAL\"\n");
    fprintf(out, "@PJL SET RESOLUTION=%d\n", dpi);
    fprintf(out, "@PJL SET BITSPERPIXEL=8\n");
    fprintf(out, "@PJL SET JOBATTR=\"@TRCH=ON\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@APSP=OFF\"\n");
    fprintf(out, "@PJL SET ECONOMODE=OFF\n");
    fprintf(out, "@PJL SET SLIPSHEET=OFF\n");
    fprintf(out, "@PJL SET JOBATTR=\"@SPSE=AUTO\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@PODR=NORMAL\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@DRDM=RASTER\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@BANR=DEVICE\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@IREC=OFF\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@TCPR=24\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@GCPR=24\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@ICPR=24\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@TUCR=24\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@GUCR=24\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@IUCR=24\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@TTRC=9\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@GTRC=9\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@ITRC=9\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@TSCR=9\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@GSCR=9\"\n");
    fprintf(out, "@PJL SET JOBATTR=\"@ISCR=9\"\n");
    fprintf(out, "@PJL ENTER LANGUAGE=HBPL\n");
    fwrite(A_REC, 1, sizeof A_REC, out);
}

/* ------------------------------------------------------------------ */
/* per-page processor                                                  */
/* ------------------------------------------------------------------ */

int hbpl_page(HbplPageCtx *ctx, Ppm *pr, int pageno, FILE *out)
{
    unsigned char C_REC[78], QR_REC[12];
    int in_w = pr->w, in_h = pr->h;
    double ty0, ty1;
    int y0, y1, img_h, cx, cy, cw, ch, W4;
    size_t stride;
    unsigned char *img;
    BitW payload;

    /* PM2FXR crop rule (verified against the original filter):
     *   x0=round(left*r)  x1=round(right*r)
     *   y0=in_h-round(top*r)  y1=in_h-round(bottom*r)
     * pass through untouched when the input fits inside the imageable area */
    ty0 = floor(ctx->ia[3] * ctx->rr + 0.5);
    ty1 = floor(ctx->ia[1] * ctx->rr + 0.5);
    y0 = in_h - (int)ty0;
    y1 = in_h - (int)ty1;
    img_h = y1 - y0;
    cx = 0; cy = 0; cw = in_w; ch = in_h;
    if (!(in_w <= ctx->img_w && in_h <= img_h)) {
        cx = ctx->x0 < 0 ? 0 : (ctx->x0 > in_w ? in_w : ctx->x0);
        cy = y0 < 0 ? 0 : (y0 > in_h ? in_h : y0);
        cw = ctx->img_w; if (cx + cw > in_w) cw = in_w - cx;
        ch = img_h; if (cy + ch > in_h) ch = in_h - cy;
        if (cw < 0) cw = 0;
        if (ch < 0) ch = 0;
    }
    W4 = (cw + 7) & ~7;   /* FXM_ALC pads the width to a multiple of 8 */

    /* image: (ch+1) rows x (W4+2) slots x 4 bytes; row 0 = all 0xff */
    stride = (size_t)(W4 + 2) * 4;
    img = (unsigned char *)malloc(stride * (size_t)(ch + 1));
    if (!img) {
        fprintf(stderr, "%s: out of memory (%dx%d)\n", ctx->prog, W4, ch);
        return -1;
    }
    memset(img, 0xff, stride);
    /* skip the PPM rows above the crop region */
    for (int y = 0; y < cy; y++)
        if (fread(pr->row, 3, (size_t)in_w, pr->f) != (size_t)in_w) {
            fprintf(stderr, "%s: short read from PPM stream (skip row %d)\n", ctx->prog, y);
            free(img);
            return -1;
        }
    for (int y = 0; y < ch; y++) {
        unsigned char *row;
        if (fread(pr->row, 3, (size_t)in_w, pr->f) != (size_t)in_w) {
            fprintf(stderr, "%s: short read from PPM stream (row %d)\n", ctx->prog, y);
            free(img);
            return -1;
        }
        row = img + stride * (size_t)(y + 1);
        memset(row, 0xff, 4);                       /* left margin   */
        memset(row + (size_t)(W4 + 1) * 4, 0xff, 4); /* right sentinel */
        for (int x = 0; x < cw; x++) {
            const unsigned char *s = pr->row + (size_t)(cx + x) * 3;
            unsigned char *d = row + (size_t)(x + 1) * 4;
            d[0] = 0xff;
            if (ctx->gray) {
                if (ctx->calibrate) {
                    /* FXM_CC gray: NTSC over the CST output */
                    unsigned char c[3];
                    unsigned g;
                    calib_cst(s, c);
                    g = ((unsigned)c[0] * 77u + (unsigned)c[1] * 151u +
                         (unsigned)c[2] * 28u) >> 8;
                    d[1] = d[2] = d[3] = (unsigned char)g;
                } else {
                    unsigned g = ((unsigned)s[0] + s[1] + s[2]) / 3;
                    d[1] = d[2] = d[3] = (unsigned char)g;
                }
            } else if (ctx->calibrate) {
                calib_pixel_cached(s, d + 1);
            } else {
                d[1] = s[0]; d[2] = s[1]; d[3] = s[2];
            }
        }
        for (int x = cw; x < W4; x++)               /* pad columns */
            memset(row + (size_t)(x + 1) * 4, 0xff, 4);
    }
    /* skip the PPM rows below the crop region (keeps the stream aligned) */
    for (int y = ch; y < in_h - cy; y++)
        if (fread(pr->row, 3, (size_t)in_w, pr->f) != (size_t)in_w) {
            fprintf(stderr, "%s: short read from PPM stream (tail row %d)\n", ctx->prog, y);
            free(img);
            return -1;
        }

    /* sq21 encode */
    bw_init(&payload);
    sq21_encode(&payload, W4, ch, img);
    free(img);

    /* HBPL page: C record + QR record + body + payload + "SD" */
    memcpy(C_REC, C_REC_TPL, sizeof C_REC);
    memcpy(QR_REC, QR_REC_TPL, sizeof QR_REC);
    put_u16le(C_REC + 24, (unsigned)ctx->marg_x);
    put_u16le(C_REC + 26, (unsigned)ctx->marg_y);
    put_u32le(C_REC + 33, (unsigned)pageno);
    put_u32le(C_REC + 39, (unsigned)W4);
    put_u32le(C_REC + 43, (unsigned)ch);
    put_u32le(C_REC + 70, (unsigned)W4);
    put_u32le(C_REC + 74, (unsigned)ch);
    put_u32le(QR_REC + 8, 24 + (unsigned long)payload.len);
    fwrite(C_REC, 1, sizeof C_REC, out);
    fwrite(QR_REC, 1, sizeof QR_REC, out);
    fwrite(BODY, 1, sizeof BODY, out);
    if (payload.len) fwrite(payload.buf, 1, payload.len, out);
    fwrite("SD", 1, 2, out);
    free(payload.buf);

    return 0;
}

/* ------------------------------------------------------------------ */
/* job trailer                                                         */
/* ------------------------------------------------------------------ */

void hbpl_trailer(FILE *out)
{
    /* 'B' + UEL + "@PJL EOJ\n" (19 bytes, byte-exact like FXM_HBPL) */
    static const char trailer[] = "B\x1b%-12345X@PJL EOJ\n";
    fwrite(trailer, 1, sizeof trailer - 1, out);
}
