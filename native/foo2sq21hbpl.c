/*
 * foo2sq21hbpl.c — native 64-bit CUPS filter for the Xerox Phaser 6125N
 *                  (and the sibling Fuji Xerox DocuPrint C525A, same engine).
 *
 * Thin main() around hbpl_core.c: parses the CUPS filter arguments, forks
 * Ghostscript (ppmraw) to rasterise the PostScript input, and pipes the PPM
 * stream through the shared HBPL encoder core.
 *
 * The entire pixel pipeline (crop -> 4-byte pixels -> pad -> sq21 lossless
 * encoder -> Fuji HBPL container) lives in hbpl_core.c and is shared with
 * the Windows tools and the PAPPL printer application — see hbpl_core.h.
 *
 * CUPS filter protocol:  foo2sq21hbpl job-id user title copies options [file]
 * Environment:            PPD=<path to the PPD>
 *                         FOO2SQ_CALIBRATE=0  — disable the FXM_CC transform
 *                         FOO2SQ_DLUT=<path>  — override the .dlut file path
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

#include "hbpl_core.h"

static void sanitize(char *s)
{
    for (; *s; s++) if (*s == '\n' || *s == '\r') *s = ' ';
}

int main(int argc, char **argv)
{
    const char *user, *title, *opts, *input;
    char pagesize[64] = "A4";
    int copies, dpi = 600, gray = 0;
    int fxdlut = 24;   /* FXDocumentType Normal (FXDLUT code from the PPD) */
    HbplPageCtx ctx;
    char hostname[256] = "localhost";
    char ttl[256], usr[128], hn[256];
    Ppm pr;
    int pageno, rc;
    int pfd[2];
    pid_t pid;
    FILE *gsout;
    int status;

    if (argc < 6) {
        fprintf(stderr, "Usage: foo2sq21hbpl job-id user title copies options [file]\n");
        return 1;
    }
    user   = argv[2];
    title  = argv[3];
    copies = atoi(argv[4]); if (copies < 1) copies = 1;
    opts   = argv[5];
    input  = (argc > 6) ? argv[6] : "-";

    signal(SIGPIPE, SIG_IGN);

    /* ---- options (argv[5]: "key=value key=value ...") ---- */
    hbpl_parse_options(opts, pagesize, sizeof pagesize, &dpi, &gray, &fxdlut);

    /* ---- page context: calibration + geometry ---- */
    ctx.prog  = "foo2sq21hbpl";
    ctx.gray  = gray;
    ctx.calibrate = hbpl_calib_setup(ctx.prog, gray, fxdlut);
    hbpl_geometry(NULL, pagesize, dpi, &ctx);

    /* ---- fork gs (the PS2PM stage) ---- */
    if (pipe(pfd)) { fprintf(stderr, "foo2sq21hbpl: pipe: %s\n", strerror(errno)); return 1; }
    pid = fork();
    if (pid < 0) { fprintf(stderr, "foo2sq21hbpl: fork: %s\n", strerror(errno)); return 1; }
    if (pid == 0) {
        char r[16], wpt[40], hpt[40];
        char *in;
        /* execvp wants char *const[]; use mutable buffers so no casts are
         * needed (keeps -Wcast-qual / -Wwrite-strings happy) */
        static char GS[] = "gs", QUIET[] = "-dQUIET", PSAFER[] = "-dPARANOIDSAFER";
        static char NOPAUSE[] = "-dNOPAUSE", BATCH[] = "-dBATCH", NOMEDIA[] = "-dNOMEDIAATTRS";
        static char DEVP[] = "-sDEVICE=ppmraw", STDO[] = "-sstdout=%stderr";
        static char OUTF[] = "-sOUTPUTFILE=%stdout";
        close(pfd[0]);
        if (pfd[1] != 1) { dup2(pfd[1], 1); close(pfd[1]); }
        snprintf(r, sizeof r, "-r%d", dpi);
        snprintf(wpt, sizeof wpt, "-dDEVICEWIDTHPOINTS=%d", (int)ctx.pd[0]);
        snprintf(hpt, sizeof hpt, "-dDEVICEHEIGHTPOINTS=%d", (int)ctx.pd[1]);
        in = strdup(input);
        if (!in) _exit(127);
        execvp("gs", (char *[]){ GS, QUIET, PSAFER, NOPAUSE, BATCH, NOMEDIA,
                                 DEVP, STDO, OUTF, r, wpt, hpt, in, NULL });
        fprintf(stderr, "foo2sq21hbpl: cannot exec gs: %s\n", strerror(errno));
        _exit(127);
    }
    close(pfd[1]);
    gsout = fdopen(pfd[0], "rb");
    if (!gsout) { fprintf(stderr, "foo2sq21hbpl: fdopen: %s\n", strerror(errno)); return 1; }

    /* ---- PJL header + A record ---- */
    gethostname(hostname, sizeof hostname - 1);
    snprintf(ttl, sizeof ttl, "%s", title);
    sanitize(ttl);
    snprintf(usr, sizeof usr, "%s", user);
    sanitize(usr);
    snprintf(hn, sizeof hn, "%s", hostname);
    sanitize(hn);
    hbpl_pjl_header(stdout, ttl, usr, hn, copies, dpi);

    /* ---- pages ---- */
    memset(&pr, 0, sizeof pr); pr.f = gsout;
    pageno = 0;
    while ((rc = ppm_next(&pr)) == 1) {
        pageno++;
        if (hbpl_page(&ctx, &pr, pageno, stdout) != 0)
            return 1;
    }
    if (rc < 0) {
        fprintf(stderr, "foo2sq21hbpl: bad PPM stream from gs\n");
        return 1;
    }
    if (pageno == 0) {
        fprintf(stderr, "foo2sq21hbpl: gs produced no pages\n");
        return 1;
    }

    /* job trailer */
    hbpl_trailer(stdout);
    fflush(stdout);

    status = 0;
    fclose(gsout);
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "foo2sq21hbpl: gs failed (status %d)\n", status);
        return 1;
    }
    return 0;
}
