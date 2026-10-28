/* hbpl_core.h — portable core of the Phaser 6125N HBPL encoder.
 *
 * Everything except the CUPS-filter main(): the sq21/HBPL1 codec, the HBPL
 * container records, the FXM_CC colour calibration (CST + 3D DLUT), the
 * PPM (P6) reader, the PJL header writer, the per-page processor and the
 * job trailer.  Pure C99 + stdio only — no POSIX — so it compiles for
 * Windows (MinGW/MSVC) as well as Linux:
 *
 *   Linux filter : native/foo2sq21hbpl.c   (main: fork gs, pipe the PPM)
 *   Windows tool : win/hbplenc.c           (main: PPM on stdin -> HBPL on stdout)
 *   PAPPL app    : pappl/phaser6125-pappl.c (MIME filter: PDF -> gs -> PPM -> hbpl_page)
 *
 * The pixel pipeline and byte output are shared verbatim between all
 * consumers; verified byte-identical (re/verify-native.py and the
 * binary-identity regression against the installed CUPS filter).
 */
#ifndef HBPL_CORE_H
#define HBPL_CORE_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* MSB-first bit writer with a growable buffer                         */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char *buf; size_t len, cap;
    unsigned long long acc; int nbits;
    unsigned char pad[4];   /* explicit tail padding (keeps -Wpadded quiet) */
} BitW;

void bw_init(BitW *b);

/* ------------------------------------------------------------------ */
/* sq21 lossless encoder                                               */
/* ------------------------------------------------------------------ */

/* Encode the whole image (W x H pixels, 4 bytes each) into b. */
void sq21_encode(BitW *b, int W, int H, const unsigned char *img);

/* ------------------------------------------------------------------ */
/* PPM (P6) stream reader                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    FILE *f;
    unsigned char *row;   /* w*3 bytes */
    int w, h, maxval;
    int pad;              /* explicit tail padding (keeps -Wpadded quiet) */
} Ppm;

/* read the next image header; 1 = ok, 0 = EOF, -1 = bad stream */
int ppm_next(Ppm *p);

/* ------------------------------------------------------------------ */
/* options / geometry / calibration setup (shared by both mains)       */
/* ------------------------------------------------------------------ */

/* Parse a CUPS options string ("key=value key=value ...") into the
 * page size, dpi, gray flag and FXDLUT block code. */
void hbpl_parse_options(const char *opts, char *pagesize, size_t psz,
                        int *dpi, int *gray, int *fxdlut);

/* Per-page processing context. */
typedef struct {
    const char *prog;      /* error message prefix */
    double pd[2];          /* paper dimension, pt */
    double ia[4];          /* imageable area, pt: left bottom right top */
    double rr;             /* dpi / 72 */
    int x0, img_w;         /* crop bounds, px */
    int marg_x, marg_y;    /* HBPL C-record margins */
    int gray, calibrate;
} HbplPageCtx;

/* Resolve the PPD (override arg, else env PPD / CUPS_PPD), look up
 * PaperDimension + ImageableArea and fill the geometry fields of ctx. */
void hbpl_geometry(const char *ppd_override, const char *pagesize,
                   int dpi, HbplPageCtx *ctx);

/* Resolve FOO2SQ_CALIBRATE / FOO2SQ_DLUT, load the .dlut block for fxdlut
 * (colour mode only) and return whether calibration is enabled.  Prints a
 * warning (prog prefix) when the .dlut is unusable. */
int hbpl_calib_setup(const char *prog, int gray, int fxdlut);

/* ------------------------------------------------------------------ */
/* output writers                                                      */
/* ------------------------------------------------------------------ */

/* PJL header + A record. */
void hbpl_pjl_header(FILE *out, const char *title, const char *user,
                     const char *hostname, int copies, int dpi);

/* Process one PPM image (pr) and write the HBPL page (C record + QR
 * record + body + payload + "SD") to out.  Returns 0 on success. */
int hbpl_page(HbplPageCtx *ctx, Ppm *pr, int pageno, FILE *out);

/* Job trailer: 'B' + UEL + "@PJL EOJ\n" (19 bytes, byte-exact like FXM_HBPL). */
void hbpl_trailer(FILE *out);

#endif /* HBPL_CORE_H */
