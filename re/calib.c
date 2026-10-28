/* calib.c — oracle voor de FXM_CC kleurcalibratie (CST + 3D DLUT), om de
 * native driver (native/foo2sq21hbpl.c) end-to-end te verifiëren tegen de
 * oorspronkelijke 32-bit FXM_CC binary.
 *
 * Onafhankelijke implementatie van het byte-exact model uit
 * re/verify_model.c (bewezen tegen ground truth: synth.fxr -> synth-cc.fxr,
 * alle 256^3 kleuren, 0 mismatches), maar lezend uit het .dlut bestand
 * (zoals de driver) in plaats van uit de gdb memory dump.
 *
 * File vs memory (bewezen, zie RE-NOTES.md §9): FXM_CC bewaart elke 4-byte
 * node reversed in het geheugen:  dump node = [out2, out1, out0, pad],
 * file node = [pad, out0, out1, out2].  De model-mapping in dump-termen
 * (out0 <- dump[2], out1 <- dump[1], out2 <- dump[0]) is in file-termen:
 * out0 <- file[1], out1 <- file[2], out2 <- file[3].
 *
 * Pipeline (uit de Ghidra decompilatie van FXM_CC main):
 *   color: out = 3Dlut(CST(in))    (CST eerst, dáárna de tetrahedral LUT)
 *   gray : out = NTSC(CST(in))     (0.30/0.59/0.11 == integer 77/151/28 >> 8)
 *   CST  : gamma-2.2 TRC (8->16 bit) -> 3x3 matrix signed Q16
 *          (((M[i]>>2)*v ...) >> 18 == m*v/16, 16->12 bit) -> clamp [0,0xfff]
 *          -> inverse TRC (12->8 bit).  Tabellen: native/cst_tables.h
 *          (gegenereerd uit de FXM_CC ELF, geverifieerd tegen de dump).
 *   3Dlut: tetrahedral 17^3.  Node index (c2>>4)*289 + (c1>>4)*17 + (c0>>4);
 *          fracties f = c&0xf, descending sort; weights w0=16-f[0],
 *          w1=f[0]-f[1], w2=f[1]-f[2], w3=f[2] (sommen tot 16); nodes
 *          base / base+d[0] / base+d[0]+d[1] / base+307 (c111); resultaat
 *          (sum w*node)>>4.  White shortcut: (c0&c1&c2)==0xff -> 255,255,255.
 *
 * .dlut block-selectie: FXDocumentType -> FXDLUT (PPD setpagedevice) ->
 * meta[FXDLUT] = 1-based blocknummer; block data op offsets[block-1]+8,
 * 20420 bytes = 768 ongebruikte 1D curves + 17^3 nodes x 4 bytes.
 * Uit meta[] van driver/Phaser_6125N.dlut: Normal 24 -> block 3,
 * Photo 22 -> block 1, Presentation 26 -> block 5, Web 23 -> block 2,
 * POP 32 -> block 11.
 *
 * Export (shared lib, voor re/verify-native.py):
 *   int  calib_load(const char *path, int fxdlut);  0 = ok, -1 = onbruikbaar
 *   void calib_color(const uint8_t *in, uint8_t *out, size_t n);  3n -> 3n
 *   void calib_gray (const uint8_t *in, uint8_t *out, size_t n);  3n -> n
 *   Zonder calib_load: CST-only (zoals de driver als het .dlut ontbreekt).
 *
 * Standalone (ground-truth cross-check, vanaf de repo root):
 *   calib in.fxr out.fxr [--gray] [--fxdlut N] [--dlut path]
 *   FXR = 44-byte header (11 x u32le) + W*H*3 pixels (24bpp in).
 *   Color out: 24bpp, header gekopieerd van de input.  Gray out: 8bpp,
 *   1 byte per pixel (bpp=8 in de header, zoals gray-out.fxr).
 *   Env: FXDLUT (default 24 = Normal), FOO2SQ_DLUT
 *        (default driver/Phaser_6125N.dlut).
 *
 * Bouw:  gcc -O2 -Wall -Wextra -o re/build/calib re/calib.c
 *        gcc -O2 -Wall -Wextra -shared -fPIC -DCALIB_NO_MAIN \
 *            -o re/libcalib.so re/calib.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../native/cst_tables.h"

static uint8_t *calib_block = NULL;   /* malloc'd 20420 bytes */
static uint8_t *calib_nodes = NULL;   /* = calib_block + 768, 17^3 x 4 bytes */

static uint32_t rd32be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Laad de 3D LUT block voor `fxdlut` uit het .dlut bestand.
 * Retourneert 0 bij succes, -1 bij een onbruikbaar bestand. */
int calib_load(const char *path, int fxdlut)
{
    FILE *f = fopen(path, "rb");
    uint8_t hdr[8], b[4];
    uint32_t count, metacount, block, *offs = NULL;
    uint8_t *raw = NULL;
    if (!f) return -1;
    if (fread(hdr, 1, 8, f) != 8 || memcmp(hdr, "DLUT", 4) != 0) goto fail;
    count = rd32be(hdr + 4);
    if (count == 0 || count > 4096) goto fail;
    offs = (uint32_t *)malloc(count * sizeof *offs);
    if (!offs) goto fail;
    for (uint32_t i = 0; i < count; i++) {
        if (fread(b, 1, 4, f) != 4) goto fail;
        offs[i] = rd32be(b);
    }
    if (fread(b, 1, 4, f) != 4) goto fail;          /* metadata count */
    metacount = rd32be(b);
    if (fxdlut < 0 || (uint32_t)fxdlut >= metacount) goto fail;
    if (fseek(f, (long)fxdlut * 4, SEEK_CUR) != 0) goto fail;  /* naar meta[fxdlut] */
    if (fread(b, 1, 4, f) != 4) goto fail;
    block = rd32be(b);                             /* 1-based block nummer */
    if (block < 1 || block > count) goto fail;
    /* block data: offsets[block-1] + 8, 20420 bytes (768 curves + 17^3*4 LUT) */
    if (fseek(f, (long)offs[block - 1] + 8, SEEK_SET) != 0) goto fail;
    raw = (uint8_t *)malloc(20420);
    if (!raw) goto fail;
    if (fread(raw, 1, 20420, f) != 20420) goto fail;
    free(calib_block);
    calib_block = raw;
    calib_nodes = raw + 768;    /* 17^3 nodes x 4 bytes: [pad, out0, out1, out2] */
    free(offs);
    fclose(f);
    return 0;
fail:
    free(raw);
    free(offs);
    fclose(f);
    return -1;
}

/* CST stage: gamma-2.2 round trip, 8-bit in -> 8-bit out (lossy 8->16->12->8). */
static void cst(const uint8_t in[3], uint8_t out[3])
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

/* Tetrahedral interpolatie in de 17^3 LUT (FXM_CC 3Dlut, file byte order:
 * node bytes [pad, out0, out1, out2]).  De 4 gewichten zijn de verschillen
 * van de descending gesorteerde 4-bit fracties; ze sommen tot 16. */
static void tetra(const uint8_t c[3], uint8_t out[3])
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
    for (int a = 1; a < 3; a++) {           /* sorteer descending op fractie */
        int fv = f[a], dv = d[a], b = a - 1;
        while (b >= 0 && f[b] < fv) { f[b + 1] = f[b]; d[b + 1] = d[b]; b--; }
        f[b + 1] = fv; d[b + 1] = dv;
    }
    w0 = 16 - f[0]; w1 = f[0] - f[1]; w2 = f[1] - f[2]; w3 = f[2];
    n0 = calib_nodes + (size_t)base * 4;
    nA = calib_nodes + (size_t)(base + d[0]) * 4;
    nAB = calib_nodes + (size_t)(base + d[0] + d[1]) * 4;
    nABC = calib_nodes + (size_t)(base + 307) * 4;    /* c111 */
    /* node bytes: [pad, out0, out1, out2] */
    s0 = w0 * n0[1] + w1 * nA[1] + w2 * nAB[1] + w3 * nABC[1];
    s1 = w0 * n0[2] + w1 * nA[2] + w2 * nAB[2] + w3 * nABC[2];
    s2 = w0 * n0[3] + w1 * nA[3] + w2 * nAB[3] + w3 * nABC[3];
    out[0] = (uint8_t)(s0 >> 4);    /* gewichten sommen tot 16, dus s>>4 <= 255 */
    out[1] = (uint8_t)(s1 >> 4);
    out[2] = (uint8_t)(s2 >> 4);
}

/* Volledige FXM_CC kleur transform: out = 3Dlut(CST(in)).
 * Zonder .dlut: alleen CST (zoals het origineel als het bestand ontbreekt). */
void calib_color(const uint8_t *in, uint8_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++, in += 3, out += 3) {
        uint8_t c[3];
        cst(in, c);
        if (!calib_nodes) {
            out[0] = c[0]; out[1] = c[1]; out[2] = c[2];
            continue;
        }
        tetra(c, out);
    }
}

/* Gray transform: out = NTSC(CST(in)), 1 byte per pixel
 * (0.30/0.59/0.11 == integer 77/151/28, >> 8 — geverifieerd tegen
 * gray-test.fxr -> gray-out.fxr). */
void calib_gray(const uint8_t *in, uint8_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++, in += 3, out++) {
        uint8_t c[3];
        cst(in, c);
        *out = (uint8_t)(((unsigned)c[0] * 77u + (unsigned)c[1] * 151u +
                          (unsigned)c[2] * 28u) >> 8);
    }
}

#ifndef CALIB_NO_MAIN
static uint32_t rd32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

int main(int argc, char **argv)
{
    const char *fin = NULL, *fout = NULL, *dlut = NULL;
    int gray = 0, fxdlut = -1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--gray")) gray = 1;
        else if (!strcmp(argv[i], "--fxdlut") && i + 1 < argc) fxdlut = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dlut") && i + 1 < argc) dlut = argv[++i];
        else if (!fin) fin = argv[i];
        else if (!fout) fout = argv[i];
        else { fprintf(stderr, "calib: onverwacht argument %s\n", argv[i]); return 1; }
    }
    if (!fin || !fout) {
        fprintf(stderr, "usage: calib in.fxr out.fxr [--gray] [--fxdlut N] [--dlut path]\n");
        return 1;
    }
    if (!dlut) dlut = getenv("FOO2SQ_DLUT");
    if (!dlut || !*dlut) dlut = "driver/Phaser_6125N.dlut";
    if (fxdlut < 0) {
        const char *e = getenv("FXDLUT");
        fxdlut = e ? atoi(e) : 24;   /* Normal */
    }
    if (calib_load(dlut, fxdlut) != 0 && !gray)
        fprintf(stderr, "calib: dlut %s niet bruikbaar, alleen CST calibratie\n", dlut);

    /* FXR in: 44-byte header + W*H*3 pixels (24bpp) */
    FILE *f = fopen(fin, "rb");
    uint8_t hdr[44];
    if (!f) { perror(fin); return 1; }
    if (fread(hdr, 1, 44, f) != 44) { fprintf(stderr, "calib: %s te kort\n", fin); return 1; }
    uint32_t w = rd32le(hdr), h = rd32le(hdr + 4), bpp = rd32le(hdr + 8);
    if (bpp != 24) { fprintf(stderr, "calib: %s bpp=%u != 24\n", fin, bpp); return 1; }
    size_t npix = (size_t)w * h;
    uint8_t *px = (uint8_t *)malloc(npix * 3);
    if (!px || fread(px, 1, npix * 3, f) != npix * 3) {
        fprintf(stderr, "calib: %s pixel data te kort\n", fin);
        return 1;
    }
    fclose(f);
    printf("calib: %ux%u (%zu pixels), %s, fxdlut=%d, dlut=%s\n",
           w, h, npix, gray ? "gray (NTSC over CST)" : "color (3Dlut over CST)",
           fxdlut, dlut);

    FILE *o = fopen(fout, "wb");
    if (!o) { perror(fout); return 1; }
    if (gray) {
        uint8_t *g = (uint8_t *)malloc(npix);
        if (!g) { fprintf(stderr, "calib: out of memory\n"); return 1; }
        calib_gray(px, g, npix);
        wr32le(hdr + 8, 8);        /* 8bpp output */
        wr32le(hdr + 12, (uint32_t)npix);
        fwrite(hdr, 1, 44, o);
        fwrite(g, 1, npix, o);
        free(g);
    } else {
        uint8_t *cp = (uint8_t *)malloc(npix * 3);
        if (!cp) { fprintf(stderr, "calib: out of memory\n"); return 1; }
        calib_color(px, cp, npix);
        fwrite(hdr, 1, 44, o);
        fwrite(cp, 1, npix * 3, o);
        free(cp);
    }
    fclose(o);
    free(px);
    printf("calib: %s geschreven\n", fout);
    return 0;
}
#endif /* CALIB_NO_MAIN */
