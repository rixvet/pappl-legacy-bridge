/*
 * foo2sq21hbpl.c — native 64-bit CUPS filter for the Xerox Phaser 6125N
 *                  (and the sibling Fuji Xerox DocuPrint C525A, same engine).
 *
 * Re-implementation of the original 32-bit Xerox/Fuji FXM filter chain
 * (FXM_PS2PM -> FXM_PM2FXR -> FXM_SBP -> FXM_PR -> FXM_CC -> FXM_ALC -> FXM_HBPL),
 * reverse engineered from the shipped binaries.  Everything below is verified
 * against the original driver's output (see ANALYSIS.md / RE-NOTES.md):
 *
 *   gs (ppmraw) -> crop to the PPD ImageableArea -> 4-byte pixels [ff,R,G,B]
 *   -> pad width to a multiple of 8 -> sq21 (Fuji "ARL" == foo2zjs HBPL1 "XRC")
 *   lossless run/diff encoder -> Fuji HBPL container (@DRDM=RASTER, single
 *   interleaved bitstream) -> stdout.
 *
 * Deliberately NOT replicated (v1):
 *   - FXM_CC's dlut colour calibration (the print is slightly uncalibrated;
 *     structurally the raster is identical: black->black, white->white).
 *   - the near-lossless quantisation of FXM_ALC (we encode losslessly, which
 *     is a valid subset of the same bitstream — the printer's decoder only
 *     knows run/diff/switch tokens).
 *
 * CUPS filter protocol:  foo2sq21hbpl job-id user title copies options [file]
 * Environment:            PPD=<path to the PPD>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

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
        if (val == 0)  { HPE0[d].code = 0; HPE0[d].len = 3; continue; }
        if (val == 1)  { HPE0[d].code = 1; HPE0[d].len = 3; continue; }
        if (val == -1) { HPE0[d].code = 2; HPE0[d].len = 3; continue; }
        int sign = val < 0, a = val < 0 ? -val : val;
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

/* MSB-first bit writer with a growable buffer */
typedef struct {
    unsigned char *buf; size_t len, cap;
    unsigned long long acc; int nbits;
} BitW;

static void bw_init(BitW *b)
{
    b->cap = 1 << 16; b->len = 0;
    b->buf = malloc(b->cap);
    if (!b->buf) { fprintf(stderr, "foo2sq21hbpl: out of memory\n"); exit(1); }
    b->acc = 0; b->nbits = 0;
}
static void bw_need(BitW *b, size_t n)
{
    if (b->len + n <= b->cap) return;
    while (b->len + n > b->cap) b->cap *= 2;
    b->buf = realloc(b->buf, b->cap);
    if (!b->buf) { fprintf(stderr, "foo2sq21hbpl: out of memory\n"); exit(1); }
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
    if (pid == 0)   /* left: current row, slot x (x==0 -> margin) */
        return img + ((size_t)(y + 1) * (size_t)(W + 2) + (size_t)x) * 4;
    int slot = x + 1 + PRED_DX[pid - 1];
    if (slot < 0 || slot > W + 1) return FF4;   /* beyond the sentinel -> margin */
    return img + ((size_t)y * (size_t)(W + 2) + (size_t)slot) * 4;
}

static void put_token(BitW *b, int hsel, int token)
{
    bw_bits(b, HID[hsel][token].code, HID[hsel][token].len);
}
static void put_runlen(BitW *b, int run)   /* 1..17057 */
{
    if (run <= 25) { bw_bits(b, HRN0[run].code, HRN0[run].len); return; }
    int g = 7;
    if (run >= RL_GROUP16[8]) { g = 8; while (run >= RL_GROUP16[g + 1]) g++; }
    bw_bits(b, (unsigned)(g + 49), 6);
    int n = RL_ADDBITS16[g];
    if (n) bw_bits(b, (unsigned)(run - RL_GROUP16[g]), n);
}

/* Encode the whole image (W x H pixels, 4 bytes each) into b. */
static void sq21_encode(BitW *b, int W, int H, const unsigned char *img)
{
    int rotor[5] = { 0, 1, 2, 3, 4 };   /* move-to-front order of predictor ids */
    int hsel = 0, bit = 0, stat = 0;
    long total = (long)W * H;
    long off = 0;

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
            put_token(b, hsel, 5);
            const unsigned char *px = px_at(img, W, x, y);
            const unsigned char *lp = (x == 0) ? FF4 : px_at(img, W, x - 1, y);
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
static unsigned char C_REC[78] = {
    0x43, 0x91, 0xa1, 0x00, 0x92, 0xa1, 0x00, 0x93, 0xa1, 0x01, 0x94, 0xa1,
    0x02, 0x95, 0xc2, 0x00, 0x00, 0x00, 0x00, 0x96, 0xa1, 0x03, 0x97, 0xc3,
    0x5e, 0x00, 0x5e, 0x00, 0x98, 0xa1, 0x00, 0x99, 0xa4, 0x01, 0x00, 0x00,
    0x00, 0x9a, 0xc4, 0xa8, 0x12, 0x00, 0x00, 0xad, 0x1a, 0x00, 0x00, 0x9b,
    0xa1, 0x00, 0x9c, 0xa1, 0x01, 0x9d, 0xa1, 0x16, 0x9e, 0xa1, 0x00, 0x9f,
    0xa1, 0x01, 0xa0, 0xa1, 0x20, 0xa1, 0xa1, 0x00, 0xa2, 0xc4, 0xa8, 0x12,
    0x00, 0x00, 0xad, 0x1a, 0x00, 0x00
};
/* QR record (12 bytes); +8..11 = total = 24 + payload length (uint32 LE) */
static unsigned char QR_REC[12] = {
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
    if (!ppdpath) return 0;
    FILE *f = fopen(ppdpath, "r");
    if (!f) return 0;
    char line[1024];
    char pat[128];
    snprintf(pat, sizeof pat, "*%s %s/", key, pagesize);
    int n = 0;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, pat, strlen(pat))) continue;
        char *q = strchr(line, ':');
        if (!q) continue;
        q++;
        char *s = strchr(q, '"');
        if (!s) continue;
        s++;
        for (int i = 0; i < nvals; i++) {
            while (*s && isspace((unsigned char)*s)) s++;
            if (!*s || *s == '"') break;
            char *end;
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
/* PPM (P6) stream reader                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    FILE *f;
    int w, h, maxval;
    unsigned char *row;   /* w*3 bytes */
} Ppm;

static int ppm_getc(FILE *f)   /* skips whitespace and # comments */
{
    int c;
    for (;;) {
        c = fgetc(f);
        if (c == '#') { while (c != '\n' && c != EOF) c = fgetc(f); continue; }
        if (c == EOF || !isspace(c)) return c;
    }
}
static int ppm_next(Ppm *p)    /* read the next image header; 0 at EOF */
{
    int c = ppm_getc(p->f);
    if (c == EOF) return 0;
    if (c != 'P') return -1;
    c = ppm_getc(p->f);
    if (c != '6') return -1;
    long v[3]; int nv = 0;
    while (nv < 3) {
        c = ppm_getc(p->f);
        if (c == EOF) return -1;
        if (!isdigit(c)) return -1;
        long x = 0;
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
    p->row = malloc((size_t)p->w * 3);
    if (!p->row) return -1;
    return 1;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

static void sanitize(char *s)
{
    for (; *s; s++) if (*s == '\n' || *s == '\r') *s = ' ';
}

int main(int argc, char **argv)
{
    if (argc < 6) {
        fprintf(stderr, "Usage: foo2sq21hbpl job-id user title copies options [file]\n");
        return 1;
    }
    const char *user  = argv[2];
    const char *title = argv[3];
    int copies = atoi(argv[4]); if (copies < 1) copies = 1;
    const char *opts  = argv[5];
    const char *input = (argc > 6) ? argv[6] : "-";

    init_hpe0();
    signal(SIGPIPE, SIG_IGN);

    /* ---- options (argv[5]: "key=value key=value ...") ---- */
    char pagesize[64] = "A4";
    int dpi = 600;
    int gray = 0;
    {
        char o[1024];
        snprintf(o, sizeof o, "%s", opts);
        for (char *tok = strtok(o, " \t"); tok; tok = strtok(NULL, " \t")) {
            char *eq = strchr(tok, '=');
            if (!eq) continue;
            *eq = 0;
            const char *val = eq + 1;
            if (!strcmp(tok, "PageSize") || !strcmp(tok, "PageRegion"))
                snprintf(pagesize, sizeof pagesize, "%s", val);
            else if (!strcmp(tok, "FXOutputMode")) {
                int d = atoi(val);
                if (d > 0) dpi = d;
            } else if (!strcmp(tok, "FXColorMode"))
                gray = (strcmp(val, "Color") != 0);
        }
    }

    /* ---- PPD: PaperDimension + ImageableArea for the page size ---- */
    const char *ppd = getenv("PPD");
    if (!ppd || !*ppd) ppd = getenv("CUPS_PPD");
    double pd[2] = { 595, 842 };                       /* A4 default */
    double ia[4] = { 11.28, 11.28, 583.72, 830.72 };   /* left bottom right top, pt */
    if (ppd) {
        double v[4];
        if (ppd_lookup(ppd, pagesize, "PaperDimension", v, 2) == 2) { pd[0] = v[0]; pd[1] = v[1]; }
        if (ppd_lookup(ppd, pagesize, "ImageableArea", v, 4) == 4)
            { ia[0] = v[0]; ia[1] = v[1]; ia[2] = v[2]; ia[3] = v[3]; }
    }
    double rr = dpi / 72.0;
    double t;
    t = floor(ia[0] * rr + 0.5);
    int x0 = (int)t;                                  /* left   */
    t = floor(ia[2] * rr + 0.5);
    int x1 = (int)t;                                  /* right  */
    int img_w = x1 - x0;
    /* HBPL C-record margins: from the PPD paper height (like FXM_HBPL) */
    t = floor(ia[0] * rr + 0.5);
    int marg_x = (int)t;
    t = floor((pd[1] - ia[3]) * rr + 0.5);
    int marg_y = (int)t;

    /* ---- fork gs (the PS2PM stage) ---- */
    int pfd[2];
    if (pipe(pfd)) { fprintf(stderr, "foo2sq21hbpl: pipe: %s\n", strerror(errno)); return 1; }
    pid_t pid = fork();
    if (pid < 0) { fprintf(stderr, "foo2sq21hbpl: fork: %s\n", strerror(errno)); return 1; }
    if (pid == 0) {
        close(pfd[0]);
        if (pfd[1] != 1) { dup2(pfd[1], 1); close(pfd[1]); }
        char r[16], wpt[40], hpt[40];
        snprintf(r, sizeof r, "-r%d", dpi);
        snprintf(wpt, sizeof wpt, "-dDEVICEWIDTHPOINTS=%d", (int)pd[0]);
        snprintf(hpt, sizeof hpt, "-dDEVICEHEIGHTPOINTS=%d", (int)pd[1]);
        /* execvp wants char *const[]; use mutable buffers so no casts are
         * needed (keeps -Wcast-qual / -Wwrite-strings happy) */
        static char GS[] = "gs", QUIET[] = "-dQUIET", PSAFER[] = "-dPARANOIDSAFER";
        static char NOPAUSE[] = "-dNOPAUSE", BATCH[] = "-dBATCH", NOMEDIA[] = "-dNOMEDIAATTRS";
        static char DEVP[] = "-sDEVICE=ppmraw", STDO[] = "-sstdout=%stderr";
        static char OUTF[] = "-sOUTPUTFILE=%stdout";
        char *in = strdup(input);
        if (!in) _exit(127);
        char *g[] = { GS, QUIET, PSAFER, NOPAUSE, BATCH, NOMEDIA,
                      DEVP, STDO, OUTF, r, wpt, hpt, in, NULL };
        execvp("gs", g);
        fprintf(stderr, "foo2sq21hbpl: cannot exec gs: %s\n", strerror(errno));
        _exit(127);
    }
    close(pfd[1]);
    FILE *gsout = fdopen(pfd[0], "rb");
    if (!gsout) { fprintf(stderr, "foo2sq21hbpl: fdopen: %s\n", strerror(errno)); return 1; }

    /* ---- PJL header + A record ---- */
    char hostname[256] = "localhost";
    gethostname(hostname, sizeof hostname - 1);
    char ttl[256];
    snprintf(ttl, sizeof ttl, "%s", title);
    sanitize(ttl);
    char usr[128];
    snprintf(usr, sizeof usr, "%s", user);
    sanitize(usr);
    char hn[256];
    snprintf(hn, sizeof hn, "%s", hostname);
    sanitize(hn);
    {
        char date[16], tim[16];
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        strftime(date, sizeof date, "%m/%d/%Y", tm);
        strftime(tim, sizeof tim, "%H:%M:%S", tm);
        FILE *out = stdout;
        fprintf(out, "\x1b%%-12345X@PJL COMMENT DATE=%s\n", date);
        fprintf(out, "@PJL COMMENT TIME=%s\n", tim);
        fprintf(out, "@PJL COMMENT DNAME=%s\n", ttl);
        fprintf(out, "@PJL JOB MODE=PRINTER\n");
        fprintf(out, "@PJL SET JOBATTR=\"@LUNA=%s\"\n", usr);
        fprintf(out, "@PJL SET JOBATTR=\"@JOAU=%s\"\n", usr);
        fprintf(out, "@PJL SET JOBATTR=\"@CNAM=%s\"\n", hn);
        fprintf(out, "@PJL SET JOBATTR=\"@HOAD=%s\"\n", hn);
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

    /* ---- pages ---- */
    Ppm pr; memset(&pr, 0, sizeof pr); pr.f = gsout;
    int pageno = 0;
    int rc;
    while ((rc = ppm_next(&pr)) == 1) {
        pageno++;
        int in_w = pr.w, in_h = pr.h;

        /* PM2FXR crop rule (verified against the original filter):
         *   x0=round(left*r)  x1=round(right*r)
         *   y0=in_h-round(top*r)  y1=in_h-round(bottom*r)
         * pass through untouched when the input fits inside the imageable area */
        double ty0 = floor(ia[3] * rr + 0.5);
        double ty1 = floor(ia[1] * rr + 0.5);
        int y0 = in_h - (int)ty0;
        int y1 = in_h - (int)ty1;
        int img_h = y1 - y0;
        int cx = 0, cy = 0, cw = in_w, ch = in_h;
        if (!(in_w <= img_w && in_h <= img_h)) {
            cx = x0 < 0 ? 0 : (x0 > in_w ? in_w : x0);
            cy = y0 < 0 ? 0 : (y0 > in_h ? in_h : y0);
            cw = img_w; if (cx + cw > in_w) cw = in_w - cx;
            ch = img_h; if (cy + ch > in_h) ch = in_h - cy;
            if (cw < 0) cw = 0;
            if (ch < 0) ch = 0;
        }
        int W4 = (cw + 7) & ~7;   /* FXM_ALC pads the width to a multiple of 8 */

        /* image: (ch+1) rows x (W4+2) slots x 4 bytes; row 0 = all 0xff */
        size_t stride = (size_t)(W4 + 2) * 4;
        unsigned char *img = malloc(stride * (size_t)(ch + 1));
        if (!img) { fprintf(stderr, "foo2sq21hbpl: out of memory (%dx%d)\n", W4, ch); return 1; }
        memset(img, 0xff, stride);
        /* skip the PPM rows above the crop region */
        for (int y = 0; y < cy; y++)
            if (fread(pr.row, 3, (size_t)in_w, gsout) != (size_t)in_w) {
                fprintf(stderr, "foo2sq21hbpl: short read from gs (skip row %d)\n", y);
                return 1;
            }
        for (int y = 0; y < ch; y++) {
            if (fread(pr.row, 3, (size_t)in_w, gsout) != (size_t)in_w) {
                fprintf(stderr, "foo2sq21hbpl: short read from gs (row %d)\n", y);
                return 1;
            }
            unsigned char *row = img + stride * (size_t)(y + 1);
            memset(row, 0xff, 4);                       /* left margin   */
            memset(row + (size_t)(W4 + 1) * 4, 0xff, 4); /* right sentinel */
            for (int x = 0; x < cw; x++) {
                const unsigned char *s = pr.row + (size_t)(cx + x) * 3;
                unsigned char *d = row + (size_t)(x + 1) * 4;
                d[0] = 0xff;
                if (gray) {
                    unsigned g = ((unsigned)s[0] + s[1] + s[2]) / 3;
                    d[1] = d[2] = d[3] = (unsigned char)g;
                } else {
                    d[1] = s[0]; d[2] = s[1]; d[3] = s[2];
                }
            }
            for (int x = cw; x < W4; x++)               /* pad columns */
                memset(row + (size_t)(x + 1) * 4, 0xff, 4);
        }
        /* skip the PPM rows below the crop region (keeps the stream aligned) */
        for (int y = ch; y < in_h - cy; y++)
            if (fread(pr.row, 3, (size_t)in_w, gsout) != (size_t)in_w) {
                fprintf(stderr, "foo2sq21hbpl: short read from gs (tail row %d)\n", y);
                return 1;
            }

        /* sq21 encode */
        BitW payload;
        bw_init(&payload);
        sq21_encode(&payload, W4, ch, img);
        free(img);

        /* HBPL page: C record + QR record + body + payload + "SD" */
        FILE *out = stdout;
        put_u16le(C_REC + 24, (unsigned)marg_x);
        put_u16le(C_REC + 26, (unsigned)marg_y);
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
    }
    if (rc < 0) {
        fprintf(stderr, "foo2sq21hbpl: bad PPM stream from gs\n");
        return 1;
    }
    if (pageno == 0) {
        fprintf(stderr, "foo2sq21hbpl: gs produced no pages\n");
        return 1;
    }

    /* job trailer: 'B' + UEL + "@PJL EOJ\n" (19 bytes, byte-exact like FXM_HBPL) */
    static const char trailer[] = "B\x1b%-12345X@PJL EOJ\n";
    fwrite(trailer, 1, sizeof trailer - 1, stdout);
    fflush(stdout);

    int status = 0;
    fclose(gsout);
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "foo2sq21hbpl: gs failed (status %d)\n", status);
        return 1;
    }
    return 0;
}
