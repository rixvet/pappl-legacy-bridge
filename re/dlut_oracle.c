/*
 * dlut_oracle.c — determine which DLUT block, grid mapping and interpolation
 * reproduce the original Windows driver's FXM_CC ("fxcolorconv") output.
 *
 * Oracle data (captured from the 32-bit FXM chain, see RE-NOTES.md §9):
 *   re/build/s3-pr.fxr  input raster  (44-byte FXR header + 595x842 RGB 24bpp)
 *   re/build/s4-cc.fxr  output raster (same layout, after fxcolorconv)
 *
 * For every block x gridmap x interpolation x channel-swap we compute the
 * diff between DLUT(s3) and s4 and report max/mean abs diff.
 *
 * Build:  gcc -O2 -Wall -Wextra -o re/dlut_oracle re/dlut_oracle.c -lm
 * Usage:  re/dlut_oracle [dlut] [s3.fxr] [s4.fxr]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static unsigned char *dlut;
static unsigned blkoff[64];
static int nblocks;

static void die(const char *m) { fprintf(stderr, "dlut_oracle: %s\n", m); exit(1); }

static void load_dlut(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) die("cannot open dlut");
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    dlut = (unsigned char *)malloc((size_t)n);
    if (!dlut || fread(dlut, 1, (size_t)n, f) != (size_t)n) die("read dlut");
    fclose(f);
    if (memcmp(dlut, "DLUT", 4)) die("bad DLUT magic");
    nblocks = (int)((dlut[4] << 24) | (dlut[5] << 16) | (dlut[6] << 8) | dlut[7]);
    if (nblocks < 1 || nblocks > 64) die("bad block count");
    for (int i = 0; i < nblocks; i++)
        blkoff[i] = ((unsigned)dlut[8 + 4 * i] << 24) | ((unsigned)dlut[9 + 4 * i] << 16) |
                    ((unsigned)dlut[10 + 4 * i] << 8) | dlut[11 + 4 * i];
}

/* FXR raster: 44-byte header, then W*H*3 bytes */
typedef struct { int w, h; unsigned char *px; } Raster;

static void load_fxr(const char *path, Raster *r)
{
    FILE *f = fopen(path, "rb");
    if (!f) die("cannot open fxr");
    unsigned char hdr[44];
    if (fread(hdr, 1, 44, f) != 44) die("short fxr header");
    r->w = hdr[0] | (hdr[1] << 8) | (hdr[2] << 16) | ((unsigned)hdr[3] << 24);
    r->h = hdr[4] | (hdr[5] << 8) | (hdr[6] << 16) | ((unsigned)hdr[7] << 24);
    long n = (long)r->w * r->h * 3;
    r->px = (unsigned char *)malloc((size_t)n);
    if (!r->px || fread(r->px, 1, (size_t)n, f) != (size_t)n) die("short fxr raster");
    fclose(f);
}

static double gridpos(int v, int map)
{
    if (map == 0) return (v >> 4) + (v & 15) / 16.0;   /* node c covers [16c,16c+15] */
    return v * 16.0 / 255.0;                            /* nodes at c*255/16 */
}

/* Apply block b to (r,g,b); result in out[0..2] = R,G,B.
 * Node layout (from re/analyze_dlut.py): [pad, B, G, R]; axes (i,j,k)=(R,G,B). */
static void apply_block(int b, int r, int g, int bl, int map, int interp,
                        unsigned char *out)
{
    const unsigned char *lut = dlut + blkoff[b] + 8 + 3 * 256;
    double pr = gridpos(r, map), pg = gridpos(g, map), pb = gridpos(bl, map);
    int R, G, B;

    if (interp == 2) {   /* nearest node */
        int i = (int)floor(pr + 0.5); if (i < 0) i = 0; if (i > 16) i = 16;
        int j = (int)floor(pg + 0.5); if (j < 0) j = 0; if (j > 16) j = 16;
        int k = (int)floor(pb + 0.5); if (k < 0) k = 0; if (k > 16) k = 16;
        const unsigned char *nd = lut + (((size_t)i * 17 + j) * 17 + k) * 4;
        out[0] = nd[3]; out[1] = nd[2]; out[2] = nd[1];
        return;
    }

    int i0 = (int)floor(pr); if (i0 > 16) i0 = 16;
    int i1 = i0 < 16 ? i0 + 1 : 16;
    int j0 = (int)floor(pg); if (j0 > 16) j0 = 16;
    int j1 = j0 < 16 ? j0 + 1 : 16;
    int k0 = (int)floor(pb); if (k0 > 16) k0 = 16;
    int k1 = k0 < 16 ? k0 + 1 : 16;
    double fi = pr - i0, fj = pg - j0, fk = pb - k0;

    double oR = 0, oG = 0, oB = 0;
    for (int ci = 0; ci < 2; ci++)
        for (int cj = 0; cj < 2; cj++)
            for (int ck = 0; ck < 2; ck++) {
                const unsigned char *nd =
                    lut + (((size_t)(ci ? i1 : i0) * 17 + (cj ? j1 : j0)) * 17 +
                           (ck ? k1 : k0)) * 4;
                double w = (ci ? fi : 1 - fi) * (cj ? fj : 1 - fj) * (ck ? fk : 1 - fk);
                oR += w * nd[3];
                oG += w * nd[2];
                oB += w * nd[1];
            }
    if (interp == 0) {          /* round half up */
        R = (int)floor(oR + 0.5); G = (int)floor(oG + 0.5); B = (int)floor(oB + 0.5);
    } else {                    /* truncate */
        R = (int)oR; G = (int)oG; B = (int)oB;
    }
    out[0] = (unsigned char)(R < 0 ? 0 : R > 255 ? 255 : R);
    out[1] = (unsigned char)(G < 0 ? 0 : G > 255 ? 255 : G);
    out[2] = (unsigned char)(B < 0 ? 0 : B > 255 ? 255 : B);
}

int main(int argc, char **argv)
{
    const char *dlut_path = argc > 1 ? argv[1] : "driver/Phaser_6125N.dlut";
    const char *s3_path   = argc > 2 ? argv[2] : "re/build/s3-pr.fxr";
    const char *s4_path   = argc > 3 ? argv[3] : "re/build/s4-cc.fxr";

    load_dlut(dlut_path);
    Raster s3, s4;
    load_fxr(s3_path, &s3);
    load_fxr(s4_path, &s4);
    if (s3.w != s4.w || s3.h != s4.h) die("raster size mismatch");
    long npix = (long)s3.w * s3.h;
    printf("oracle: %dx%d (%ld pixels), %d blocks\n", s3.w, s3.h, npix, nblocks);

    const char *mapname[2] = { "v>>4", "v*16/255" };
    const char *interpname[3] = { "trilinear+round", "trilinear+floor", "nearest" };

    double best_overall = 1e30;
    int best_b = -1, best_m = -1, best_i = -1, best_s = -1;

    for (int b = 0; b < nblocks; b++) {
        double best_block = 1e30;
        int bb_m = -1, bb_i = -1, bb_s = -1;
        for (int map = 0; map < 2; map++)
            for (int interp = 0; interp < 3; interp++)
                for (int swap = 0; swap < 2; swap++) {
                    long maxd = 0, nexact = 0, nwithin1 = 0;
                    double sumd = 0;
                    for (long p = 0; p < npix; p++) {
                        unsigned char o[3];
                        apply_block(b, s3.px[p * 3], s3.px[p * 3 + 1],
                                    s3.px[p * 3 + 2], map, interp, o);
                        const unsigned char *q = s4.px + p * 3;
                        int dr = o[0] - (int)(swap ? q[2] : q[0]);
                        int dg = o[1] - (int)q[1];
                        int db = o[2] - (int)(swap ? q[0] : q[2]);
                        if (dr < 0) dr = -dr;
                        if (dg < 0) dg = -dg;
                        if (db < 0) db = -db;
                        int md = dr > dg ? dr : dg;
                        if (db > md) md = db;
                        sumd += (double)md;
                        if (md > maxd) maxd = md;
                        if (md == 0) nexact++;
                        if (md <= 1) nwithin1++;
                    }
                    double mean = sumd / npix;
                    if (mean < best_block) {
                        best_block = mean;
                        bb_m = map; bb_i = interp; bb_s = swap;
                    }
                    if (mean < 30 || maxd <= 2)
                        printf("  block %2d  map=%-9s interp=%-16s swap=%d  "
                               "max=%ld mean=%.4f exact=%ld (%.1f%%) within1=%ld (%.1f%%)\n",
                               b, mapname[map], interpname[interp], swap, maxd,
                               mean, nexact, 100.0 * nexact / npix, nwithin1,
                               100.0 * nwithin1 / npix);
                    if (mean < best_overall) {
                        best_overall = mean;
                        best_b = b; best_m = map; best_i = interp; best_s = swap;
                    }
                }
        printf("block %2d best: map=%s interp=%s swap=%d mean=%.4f\n",
               b, mapname[bb_m], interpname[bb_i], bb_s, best_block);
    }
    printf("\nOVERALL BEST: block %d  map=%s interp=%s swap=%d  mean=%.6f\n",
           best_b, mapname[best_m], interpname[best_i], best_s, best_overall);
    return 0;
}
