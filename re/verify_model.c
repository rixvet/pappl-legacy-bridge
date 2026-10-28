/* verify_model.c — byte-exact model of the FXM_CC color-conversion stage
 * (CST gamma-2.2 roundtrip + tetrahedral 3D LUT), verified against the
 * ground truth synth.fxr -> synth-cc.fxr (all 256^3 colours).
 *
 * Params come from live dumps of the original 32-bit binary (gdb):
 *   re/build/cst-param.bin   1684 B  (3x TRC u16[256] @ +0/+0x200/+0x400,
 *                                      3x3 matrix int32 @ +0x600, status +0x62c,
 *                                      flag +0x690)
 *   re/build/dlut-param.bin  20420 B (curves 768 B @ +0, 17^3 LUT u32 @ +0x300)
 *   re/build/xyz2srgbtrc.bin 4096 B  (inverse TRC, 12-bit index -> 8-bit)
 *
 * Build:  gcc -O2 -Wall -Wextra -o re/build/verify_model re/verify_model.c
 * Run:   ./re/build/verify_model [in.fxr out.fxr]   (from the repo root;
 *          default: synth.fxr / synth-cc.fxr)
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static uint8_t *readfile(const char *p, size_t *n)
{
    FILE *f = fopen(p, "rb");
    long s;
    uint8_t *b;
    if (!f) { perror(p); exit(1); }
    fseek(f, 0, SEEK_END); s = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)s);
    if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) { perror("read"); exit(1); }
    fclose(f);
    *n = (size_t)s;
    return b;
}

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

int main(int argc, char **argv)
{
    const char *in_path  = (argc > 1) ? argv[1] : "re/build/synth.fxr";
    const char *out_path = (argc > 2) ? argv[2] : "re/build/synth-cc.fxr";
    size_t n;
    uint8_t *cst  = readfile("re/build/cst-param.bin", &n);
    uint8_t *dlu  = readfile("re/build/dlut-param.bin", &n);
    uint8_t *itrc = readfile("re/build/xyz2srgbtrc.bin", &n);
    uint8_t *sin  = readfile(in_path, &n);
    uint8_t *sout = readfile(out_path, &n);

    const uint16_t *trc0 = (const uint16_t *)(cst + 0x000);
    const uint16_t *trc1 = (const uint16_t *)(cst + 0x200);
    const uint16_t *trc2 = (const uint16_t *)(cst + 0x400);
    const int32_t  *M    = (const int32_t  *)(cst + 0x600);
    const uint32_t *lut  = (const uint32_t *)(dlu + 0x300);   /* 4913 nodes */

    /* FXR header = 11 x u32le = 44 bytes */
    uint32_t iw = rd32(sin + 0), ih = rd32(sin + 4), ibpp = rd32(sin + 8);
    uint32_t ow = rd32(sout + 0), oh = rd32(sout + 4), obpp = rd32(sout + 8);
    printf("FXR in : %ux%u bpp=%u\nFXR out: %ux%u bpp=%u\n", iw, ih, ibpp, ow, oh, obpp);
    if (iw != ow || ih != oh || ibpp != obpp || ibpp != 24) {
        fprintf(stderr, "header mismatch\n");
        return 1;
    }
    size_t npix = (size_t)iw * ih;
    const uint8_t *ip = sin + 44, *op = sout + 44;

    long mism = 0, shown = 0;
    int maxdiff = 0;
    for (size_t p = 0; p < npix; p++) {
        uint8_t b0 = ip[p*3+0], b1 = ip[p*3+1], b2 = ip[p*3+2];

        /* ---- CST: TRC (8->16 bit) ---- */
        int32_t v0 = trc0[b0], v1 = trc1[b1], v2 = trc2[b2];
        /* ---- CST: 3x3 matrix, signed fixed point: (M>>2)*v >> 18 ---- */
        int32_t o0 = ((M[0]>>2)*v0 + (M[1]>>2)*v1 + (M[2]>>2)*v2) >> 18;
        int32_t o1 = ((M[3]>>2)*v0 + (M[4]>>2)*v1 + (M[5]>>2)*v2) >> 18;
        int32_t o2 = ((M[6]>>2)*v0 + (M[7]>>2)*v1 + (M[8]>>2)*v2) >> 18;
        /* ---- clamp [0, 0xfff] ---- */
        if (o0 < 0) o0 = 0; else if (o0 > 0xfff) o0 = 0xfff;
        if (o1 < 0) o1 = 0; else if (o1 > 0xfff) o1 = 0xfff;
        if (o2 < 0) o2 = 0; else if (o2 > 0xfff) o2 = 0xfff;
        /* ---- CST: inverse TRC (12->8 bit) ---- */
        uint8_t c0 = itrc[o0], c1 = itrc[o1], c2 = itrc[o2];

        /* ---- 3D LUT: tetrahedral ---- */
        uint8_t r0, r1, r2;
        if ((c0 & c1 & c2) == 0xff) {
            r0 = r1 = r2 = 0xff;                    /* white shortcut */
        } else {
            int base = (c2 >> 4) * 289 + (c1 >> 4) * 17 + (c0 >> 4);
            int f[3] = { c2 & 0xf, c1 & 0xf, c0 & 0xf };   /* fi, fj, fk */
            int d[3] = { 289, 17, 1 };                    /* i, j, k deltas */
            /* sort descending by fraction (ties allowed, weight becomes 0) */
            for (int a = 1; a < 3; a++) {
                int fv = f[a], dv = d[a], b = a - 1;
                while (b >= 0 && f[b] < fv) { f[b+1] = f[b]; d[b+1] = d[b]; b--; }
                f[b+1] = fv; d[b+1] = dv;
            }
            int w0 = 16 - f[0], w1 = f[0] - f[1], w2 = f[1] - f[2], w3 = f[2];
            uint32_t n0   = lut[base];
            uint32_t nA   = lut[base + d[0]];
            uint32_t nAB  = lut[base + d[0] + d[1]];
            uint32_t nABC = lut[base + 307];            /* c111 */
            /* node u32 (LE) = f3 | f2<<8 | f1<<16 | pad<<24
             * out0 <- byte2 (f1), out1 <- byte1 (f2), out2 <- byte0 (f3) */
            int s0 = w0*((n0>>16)&0xff) + w1*((nA>>16)&0xff) + w2*((nAB>>16)&0xff) + w3*((nABC>>16)&0xff);
            int s1 = w0*((n0>>8)&0xff)  + w1*((nA>>8)&0xff)  + w2*((nAB>>8)&0xff)  + w3*((nABC>>8)&0xff);
            int s2 = w0*(n0&0xff)       + w1*(nA&0xff)       + w2*(nAB&0xff)       + w3*(nABC&0xff);
            r0 = (uint8_t)(s0 >> 4);
            r1 = (uint8_t)(s1 >> 4);
            r2 = (uint8_t)(s2 >> 4);
            /* s <= 16*255 so s>>4 <= 255 — no clamp needed (uint8_t) */
        }

        uint8_t e0 = op[p*3+0], e1 = op[p*3+1], e2 = op[p*3+2];
        if (r0 != e0 || r1 != e1 || r2 != e2) {
            if (shown < 10) {
                printf("MISMATCH pix=%zu in=(%u,%u,%u) cst=(%u,%u,%u) model=(%u,%u,%u) want=(%u,%u,%u)\n",
                       p, b0, b1, b2, c0, c1, c2, r0, r1, r2, e0, e1, e2);
                shown++;
            }
            mism++;
            int dd = abs(r0-e0) + abs(r1-e1) + abs(r2-e2);
            if (dd > maxdiff) maxdiff = dd;
        }
    }
    printf("pixels=%zu  mismatches=%ld  maxdiff=%d  -> %s\n",
           npix, mism, maxdiff, mism ? "NOT BYTE-EXACT" : "BYTE-EXACT ✓");
    return mism ? 1 : 0;
}
