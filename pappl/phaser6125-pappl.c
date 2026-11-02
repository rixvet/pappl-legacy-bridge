/*
 * phaser6125-pappl.c — PAPPL Printer Application for the Xerox Phaser 6125N
 *                      (and the sibling Fuji Xerox DocuPrint C525A).
 *
 * Wraps the reverse-engineered HBPL encoder (native/hbpl_core.c) as an
 * IPP Everywhere printer application using PAPPL (msweet.org/pappl).
 * This makes the printer appear as a driverless IPP printer on Windows 11,
 * macOS, iOS, Android and Linux — no vendor V3/V4 driver needed, so it
 * survives the Windows print-driver deprecation.
 *
 * Architecture (see RE-NOTES.md §12):
 *   Client → IPP (PDF/PS/JPEG/PNG) → PAPPL → MIME filter (this file)
 *          → gs (ppmraw) → PPM → hbpl_core (hbpl_page) → HBPL
 *          → device (socket://192.168.178.10:9100 or usb:)
 *
 * The MIME filter follows the OpenPrinting "non-raster printer" pattern:
 *   driver_data.format = "application/vnd.xerox-hbpl"
 *   papplSystemAddMIMEFilter(system, "application/pdf", HBPL_MIME, filter_cb, NULL)
 *
 * PAPPL 1.4.x: the MIME filter callback receives (job, device, data) and is
 * responsible for the ENTIRE job output (PJL header + pages + trailer).
 * rstartjob_cb / rendjob_cb are only used in the raster path, which we do
 * not use (our driver format is not PWG raster).
 *
 * Build (Linux, PAPPL 1.4.x):
 *   cc -O2 -Wall -Wextra -o phaser6125-pappl pappl/phaser6125-pappl.c \
 *      native/hbpl_core.c -I native $(pkg-config --cflags --libs pappl) -lm
 *
 * Usage:
 *   phaser6125-pappl server                          # run the IPP server
 *   phaser6125-pappl add "Phaser6125" -m xerox_phaser_6125n \
 *       -v socket://192.168.178.10:9100              # add the printer
 *   phaser6125-pappl submit -d Phaser6125 file.pdf   # print a file
 */
#include <pappl/pappl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#ifndef _WIN32
#  include <unistd.h>
#  include <sys/wait.h>
#  include <signal.h>
   typedef pid_t gs_pid_t;
#else
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <io.h>
#  include <fcntl.h>
#  include <process.h>
   /* On Windows we store a PROCESS_INFORMATION-like struct in place of pid_t */
   typedef struct { HANDLE hProcess; HANDLE hThread; } gs_pid_t;
#endif

#include "../native/hbpl_core.h"
#include "../native/mdns_mini.h"
#include "../native/snmp_poll.h"

/* mDNS advertiser handle (started in system_cb when server runs) */
static mdns_handle_t *g_mdns = NULL;

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define HBPL_MIME       "application/vnd.xerox-hbpl"
#define APP_VERSION     "1.0.0"
#define APP_NAME        "Phaser6125"
#define APP_FOOTER      "Copyright &copy; 2026 — reverse-engineered HBPL driver, Apache-2.0"

/* Supported paper sizes (PWG media names) */
static const char * const phaser6125_media[] = {
    "iso_a4_210x297mm",
    "iso_a5_148x210mm",
    "iso_b5_182x257mm",
    "na_letter_8.5x11in",
    "na_legal_8.5x14in",
    "na_folio_8.5x13in",
    "na_executive_7.25x10.5in",
};
#define NUM_MEDIA ((int)(sizeof(phaser6125_media) / sizeof(phaser6125_media[0])))

/* Paper dimensions in points, matched by PWG media name substring */
typedef struct {
    const char *pwg;
    double w, h;
} PaperSize;

static const PaperSize paper_sizes[] = {
    { "a4",        595, 842 },
    { "a5",        421, 595 },
    { "b5",        516, 729 },
    { "letter",    612, 792 },
    { "legal",     612, 1008 },
    { "folio",     612, 936 },
    { "executive", 522, 756 },
};
#define NUM_PAPER ((int)(sizeof(paper_sizes) / sizeof(paper_sizes[0])))

/* Fixed hardware margins: 11.28 pt on all sides (from the PPD) */
#define MARGIN_PT 11.28

/* ------------------------------------------------------------------ */
/* Driver list                                                         */
/* ------------------------------------------------------------------ */

static pappl_pr_driver_t phaser6125_drivers[] = {
    { "xerox_phaser_6125n",   "Xerox Printer 6125N",           "MFG:XEROX;MDL:Phaser 6125N;CMD:HBPL;",           NULL },
    { "fuji_xerox_dpc525a",   "Fuji Xerox DocuPrint C525A",   "MFG:FUJI XEROX;MDL:DocuPrint C525A;CMD:HBPL;",   NULL },
};
#define NUM_DRIVERS ((int)(sizeof(phaser6125_drivers) / sizeof(phaser6125_drivers[0])))

/* ------------------------------------------------------------------ */
/* One-time calibration state                                          */
/* ------------------------------------------------------------------ */

static int calib_initialised = 0;
static int calib_enabled     = 1;

static void ensure_calib(void)
{
    if (!calib_initialised) {
        /* gray=0 so the .dlut is loaded for colour jobs */
        calib_enabled = hbpl_calib_setup(APP_NAME, 0, 24 /* FXDLUT Normal */);
        calib_initialised = 1;
    }
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void sanitize(char *s)
{
    for (; *s; s++) if (*s == '\n' || *s == '\r') *s = ' ';
}

/* Copy the contents of a FILE* to a pappl_device_t. */
static int file_to_device(FILE *f, pappl_device_t *device)
{
    char buf[65536];
    size_t n;

    fflush(f);
    rewind(f);
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        if (papplDeviceWrite(device, buf, n) != (ssize_t)n)
            return -1;
    }
    return 0;
}


/* Portable tmpfile: on Windows tmpfile() can fail in service context
 * (TMP/TEMP not set) — use GetTempPath + unique name instead. */
static FILE *make_tempfile(void)
{
#ifndef _WIN32
    return tmpfile();
#else
    char dir[MAX_PATH], path[MAX_PATH];
    FILE *f;
    if (GetTempPathA(sizeof dir, dir) == 0) return NULL;
    if (GetTempFileNameA(dir, "hbl", 0, path) == 0) return NULL;
    f = fopen(path, "w+b");
    if (f) unlink(path);  /* auto-delete on close (POSIX-style) */
    return f;
#endif
}

/* Set up the HBPL page context from PAPPL job options. */
static void setup_page_ctx(pappl_pr_options_t *options, HbplPageCtx *ctx)
{
    const char *media_name = options->media.size_name;
    double t;
    int dpi;
    int i;

    ctx->prog = APP_NAME;

    /* Paper size from PWG media name */
    ctx->pd[0] = 595; ctx->pd[1] = 842;  /* default A4 */
    for (i = 0; i < NUM_PAPER; i++) {
        if (strstr(media_name, paper_sizes[i].pwg)) {
            ctx->pd[0] = paper_sizes[i].w;
            ctx->pd[1] = paper_sizes[i].h;
            break;
        }
    }

    /* Fixed hardware margins */
    ctx->ia[0] = MARGIN_PT;
    ctx->ia[1] = MARGIN_PT;
    ctx->ia[2] = ctx->pd[0] - MARGIN_PT;
    ctx->ia[3] = ctx->pd[1] - MARGIN_PT;

    /* Resolution */
    dpi = options->printer_resolution[0] > 0 ? options->printer_resolution[0] : 600;
    ctx->rr = dpi / 72.0;

    /* Crop bounds (PM2FXR rule) */
    t = floor(ctx->ia[0] * ctx->rr + 0.5);
    ctx->x0 = (int)t;
    t = floor(ctx->ia[2] * ctx->rr + 0.5);
    ctx->img_w = (int)t - ctx->x0;

    /* HBPL C-record margins */
    t = floor(ctx->ia[0] * ctx->rr + 0.5);
    ctx->marg_x = (int)t;
    t = floor((ctx->pd[1] - ctx->ia[3]) * ctx->rr + 0.5);
    ctx->marg_y = (int)t;

    /* Colour mode: anything not explicitly colour is grayscale */
    ctx->gray = !(options->print_color_mode & PAPPL_COLOR_MODE_COLOR);

    /* Calibration (one-time) */
    ctx->calibrate = calib_enabled;
}

/* ------------------------------------------------------------------ */
/* gs pipe: fork Ghostscript, return the PPM stdout pipe               */
/* ------------------------------------------------------------------ */

#ifndef _WIN32

static FILE *start_gs(const char *input, int dpi, double pd_w, double pd_h, pid_t *pid_out)
{
    int pfd[2];
    pid_t pid;

    if (pipe(pfd)) return NULL;
    pid = fork();
    if (pid < 0) { close(pfd[0]); close(pfd[1]); return NULL; }
    if (pid == 0) {
        /* child: exec gs */
        char r[16], wpt[40], hpt[40];
        char *in;
        static char GS[] = "gs", QUIET[] = "-dQUIET", PSAFER[] = "-dPARANOIDSAFER";
        static char NOPAUSE[] = "-dNOPAUSE", BATCH[] = "-dBATCH", NOMEDIA[] = "-dNOMEDIAATTRS";
        static char DEVP[] = "-sDEVICE=ppmraw", STDO[] = "-sstdout=%stderr";
        static char OUTF[] = "-sOUTPUTFILE=%stdout";

        close(pfd[0]);
        if (pfd[1] != 1) { dup2(pfd[1], 1); close(pfd[1]); }
        snprintf(r, sizeof r, "-r%d", dpi);
        snprintf(wpt, sizeof wpt, "-dDEVICEWIDTHPOINTS=%d", (int)pd_w);
        snprintf(hpt, sizeof hpt, "-dDEVICEHEIGHTPOINTS=%d", (int)pd_h);
        in = strdup(input);
        if (!in) _exit(127);
        execvp("gs", (char *[]){ GS, QUIET, PSAFER, NOPAUSE, BATCH, NOMEDIA,
                                 DEVP, STDO, OUTF, r, wpt, hpt, in, NULL });
        _exit(127);
    }
    close(pfd[1]);
    *pid_out = pid;
    return fdopen(pfd[0], "rb");
}

static int wait_gs(pid_t pid)
{
    int status = 0;
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return -1;
    return 0;
}

#else /* _WIN32 — CreateProcess-based gs invocation via gswin64c.exe */

static FILE *start_gs(const char *input, int dpi, double pd_w, double pd_h, gs_pid_t *pid_out)
{
    HANDLE read_pipe, write_pipe;
    SECURITY_ATTRIBUTES sa;
    PROCESS_INFORMATION pi;
    STARTUPINFOA si;
    char cmdline[1024];
    char gs_exe[MAX_PATH];
    gs_pid_t *proc;
    FILE *fp;
    DWORD written;

    /* Locate gswin64c.exe: in "gs/" subdir next to our .exe, next to our
     * .exe directly, or on PATH */
    if (GetModuleFileNameA(NULL, gs_exe, MAX_PATH) > 0) {
        char *slash = strrchr(gs_exe, '\\');
        if (slash) {
            size_t remain = sizeof(gs_exe) - (size_t)(slash + 1 - gs_exe);
            snprintf(slash + 1, remain, "gs\\bin\\gswin64c.exe");
            if (GetFileAttributesA(gs_exe) == INVALID_FILE_ATTRIBUTES) {
                snprintf(slash + 1, remain, "gs\\gswin64c.exe");
                snprintf(slash + 1, remain, "gswin64c.exe");
                if (GetFileAttributesA(gs_exe) == INVALID_FILE_ATTRIBUTES)
                    snprintf(gs_exe, sizeof gs_exe, "gswin64c.exe"); /* PATH fallback */
            }
        }
    } else {
        snprintf(gs_exe, sizeof gs_exe, "gswin64c.exe");
    }

    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0))
        return NULL;
    if (!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(read_pipe); CloseHandle(write_pipe);
        return NULL;
    }

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_pipe;
    si.hStdError  = write_pipe;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    snprintf(cmdline, sizeof cmdline,
             "\"%s\" -dQUIET -dPARANOIDSAFER -dNOPAUSE -dBATCH -dNOMEDIAATTRS "
             "-sDEVICE=ppmraw -sstdout=%%stderr -sOUTPUTFILE=%%stdout "
             "-r%d -dDEVICEWIDTHPOINTS=%d -dDEVICEHEIGHTPOINTS=%d \"%s\"",
             gs_exe, dpi, (int)pd_w, (int)pd_h, input);

    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(read_pipe); CloseHandle(write_pipe);
        return NULL;
    }

    CloseHandle(write_pipe);
    CloseHandle(pi.hThread);

    /* Store process handle where pid_out points (reinterpret as HANDLE) */
    proc = pid_out;
    proc->hProcess = pi.hProcess;
    proc->hThread  = NULL;

    /* Wrap the read end of the pipe as a FILE* */
    {
        int fd = _open_osfhandle((intptr_t)read_pipe, 0);
        if (fd < 0) {
            CloseHandle(read_pipe);
            CloseHandle(pi.hProcess);
            return NULL;
        }
        fp = _fdopen(fd, "rb");
        if (!fp) {
            _close(fd);
            CloseHandle(pi.hProcess);
            return NULL;
        }
    }
    (void)written;
    return fp;
}

static int wait_gs(gs_pid_t pid_out)
{
    gs_pid_t *proc = &pid_out;
    DWORD exit_code = 1;

    if (WaitForSingleObject(proc->hProcess, INFINITE) == WAIT_OBJECT_0)
        GetExitCodeProcess(proc->hProcess, &exit_code);
    CloseHandle(proc->hProcess);
    return exit_code == 0 ? 0 : -1;
}

#endif /* !_WIN32 */

/* ------------------------------------------------------------------ */
/* MIME filter callback: PDF/PS/JPEG/PNG → gs → PPM → HBPL → device    */
/*                                                                     */
/* PAPPL 1.4.x: the filter is responsible for the ENTIRE job output    */
/* (PJL header + pages + trailer).  It receives only (job, device).    */
/* ------------------------------------------------------------------ */

static bool hbpl_filter_cb(pappl_job_t *job, pappl_device_t *device, void *data)
{
    const char *input;
    const char *format;
    pappl_pr_options_t *options;
    HbplPageCtx ctx;
    Ppm pr;
    FILE *gsout;
    FILE *tmp;
    gs_pid_t gs_pid;
    int pageno = 0;
    int rc;
    int dpi;
    int copies;
    char title[256], user[128], hostname[256];
    const char *job_name, *job_user;

    memset(&gs_pid, 0, sizeof gs_pid);

    (void)data;

    input  = papplJobGetFilename(job);
    format = papplJobGetFormat(job);
    if (!input || !*input) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "No input file for job.");
        return false;
    }

    /* Get the print options (media, resolution, colour mode) */
    options = papplJobCreatePrintOptions(job, 0, true);
    if (!options) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Cannot create print options.");
        return false;
    }

    ensure_calib();
    setup_page_ctx(options, &ctx);
    dpi = (int)(ctx.rr * 72.0 + 0.5);

    /* If the client did not send an explicit printer-resolution, PAPPL's
     * options default to 300 dpi.  Override with our driver default (600)
     * which is also what we advertise as printer-resolution-default. */
    {
        ipp_attribute_t *res_attr = papplJobGetAttribute(job, "printer-resolution");
        if (!res_attr) {
            dpi = 600;
            ctx.rr = dpi / 72.0;
            /* Recompute geometry for the corrected dpi */
            {
                double t;
                t = floor(ctx.ia[0] * ctx.rr + 0.5);
                ctx.x0 = (int)t;
                t = floor(ctx.ia[2] * ctx.rr + 0.5);
                ctx.img_w = (int)t - ctx.x0;
                t = floor(ctx.ia[0] * ctx.rr + 0.5);
                ctx.marg_x = (int)t;
                t = floor((ctx.pd[1] - ctx.ia[3]) * ctx.rr + 0.5);
                ctx.marg_y = (int)t;
            }
        }
    }

    copies = options->copies > 0 ? options->copies : 1;

    papplLogJob(job, PAPPL_LOGLEVEL_INFO,
                "Job: format=%s, %gx%g pt, %d dpi, %s, calibrate=%d, copies=%d",
                format ? format : "?",
                ctx.pd[0], ctx.pd[1], dpi,
                ctx.gray ? "gray" : "color", ctx.calibrate, copies);

    /* Sanitise strings for the PJL header */
    job_name = papplJobGetName(job);
    job_user = papplJobGetUsername(job);
    snprintf(title, sizeof title, "%s", job_name ? job_name : "Print Job");
    sanitize(title);
    snprintf(user, sizeof user, "%s", job_user ? job_user : "unknown");
    sanitize(user);
#ifdef _WIN32
    snprintf(hostname, sizeof hostname, "localhost");
#else
    if (gethostname(hostname, sizeof hostname - 1) != 0)
        snprintf(hostname, sizeof hostname, "localhost");
#endif
    hostname[sizeof hostname - 1] = '\0';
    sanitize(hostname);

    /* ---- PJL header → device ---- */
    tmp = make_tempfile();
    if (!tmp) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Cannot create temp file for PJL header.");
        papplJobDeletePrintOptions(options);
        return false;
    }
    hbpl_pjl_header(tmp, title, user, hostname, copies, dpi);
    if (file_to_device(tmp, device) != 0) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Cannot write PJL header to device.");
        fclose(tmp);
        papplJobDeletePrintOptions(options);
        return false;
    }
    fclose(tmp);

    /* ---- Fork gs to rasterise the input to PPM ---- */
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    gsout = start_gs(input, dpi, ctx.pd[0], ctx.pd[1], &gs_pid);
    if (!gsout) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Cannot start gs.");
        papplJobDeletePrintOptions(options);
        return false;
    }

    /* ---- Process each PPM page → device ---- */
    memset(&pr, 0, sizeof pr);
    pr.f = gsout;
    while ((rc = ppm_next(&pr)) == 1) {
        pageno++;

        tmp = make_tempfile();
        if (!tmp) {
            papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Cannot create temp file for page %d.", pageno);
            break;
        }

        if (hbpl_page(&ctx, &pr, pageno, tmp) != 0) {
            fclose(tmp);
            break;
        }

        if (file_to_device(tmp, device) != 0) {
            papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Cannot write page %d to device.", pageno);
            fclose(tmp);
            break;
        }
        fclose(tmp);
    }

    fclose(gsout);
    wait_gs(gs_pid);
    papplJobDeletePrintOptions(options);

    if (rc < 0) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Bad PPM stream from gs.");
        return false;
    }
    if (pageno == 0) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "gs produced no pages.");
        return false;
    }

    /* ---- Job trailer → device ---- */
    tmp = make_tempfile();
    if (!tmp) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Cannot create temp file for trailer.");
        return false;
    }
    hbpl_trailer(tmp);
    if (file_to_device(tmp, device) != 0) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Cannot write trailer to device.");
        fclose(tmp);
        return false;
    }
    fclose(tmp);

    papplLogJob(job, PAPPL_LOGLEVEL_INFO, "Job complete: %d page(s) sent to device.", pageno);
    return true;
}

/* ------------------------------------------------------------------ */
/* Auto-add callback: match by IEEE-1284 device ID                     */
/* ------------------------------------------------------------------ */

static const char *autoadd_cb(const char *device_info, const char *device_uri,
                              const char *device_id, void *data)
{
    (void)device_info; (void)device_uri; (void)data;

    if (!device_id) return NULL;

    if (strstr(device_id, "Phaser 6125N") || strstr(device_id, "Phaser6125N"))
        return "xerox_phaser_6125n";

    if (strstr(device_id, "DocuPrint C525A") || strstr(device_id, "DPC525A"))
        return "fuji_xerox_dpc525a";

    return NULL;
}

/* ------------------------------------------------------------------ */
/* Raw passthrough: client sends pre-formatted HBPL                    */
/* ------------------------------------------------------------------ */

static bool printfile_cb(pappl_job_t *job, pappl_pr_options_t *options,
                         pappl_device_t *device)
{
    const char *filename;
    FILE *in;
    char buf[65536];
    size_t n;
    (void)options;

    filename = papplJobGetFilename(job);
    if (!filename) return false;
    in = fopen(filename, "rb");
    if (!in) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Cannot open input file for raw passthrough.");
        return false;
    }
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
        if (papplDeviceWrite(device, buf, n) != (ssize_t)n) {
            fclose(in);
            return false;
        }
    }
    fclose(in);
    return true;
}

/* ------------------------------------------------------------------ */
/* Dummy raster callbacks (PAPPL 1.4.x requires them for validation).  */
/* They are never called: our MIME filter handles PDF/PS/JPEG/PNG.     */
/* PWG raster input is not yet supported (TODO).                       */
/* ------------------------------------------------------------------ */

static bool dummy_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device)
{ (void)job; (void)options; (void)device; return true; }

static bool dummy_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page)
{ (void)job; (void)options; (void)device; (void)page; return true; }

static bool dummy_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line)
{ (void)job; (void)options; (void)device; (void)y; (void)line; return true; }

static bool dummy_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page)
{ (void)job; (void)options; (void)device; (void)page; return true; }

static bool dummy_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device)
{ (void)job; (void)options; (void)device; return true; }

/* ------------------------------------------------------------------ */
/* SNMP → IPP status bridge                                            */
/*                                                                     */
/* PAPPL calls status_cb when it needs fresh printer status.  We poll  */
/* the physical printer via SNMPv1 (Printer-MIB) and translate to      */
/* pappl_supply_t[] + printer-state-reasons.                           */
/* ------------------------------------------------------------------ */

static const char *snmp_host_from_device_uri(const char *uri, char *hostbuf, size_t hostlen)
{
    /* socket://192.168.178.10:9100 → "192.168.178.10" */
    const char *p = uri;
    const char *colon, *slash;
    size_t n;
    if (!strncmp(p, "socket://", 9)) p += 9;
    else if (!strncmp(p, "usb://", 6)) return NULL;
    else if (!strncmp(p, "file:", 5)) return NULL;
    colon = strchr(p, ':');
    slash = strchr(p, '/');
    n = (colon && (!slash || colon < slash)) ? (size_t)(colon - p)
        : (slash ? (size_t)(slash - p) : strlen(p));
    if (n >= hostlen) n = hostlen - 1;
    memcpy(hostbuf, p, n);
    hostbuf[n] = 0;
    return hostbuf;
}

static bool snmp_status_cb(pappl_printer_t *printer)
{
    const char *uri = papplPrinterGetDeviceURI(printer);
    char host[64];
    snmp_status_t st;
    pappl_supply_t supplies[PAPPL_MAX_SUPPLY];
    pappl_preason_t add_reasons = PAPPL_PREASON_NONE;
    int nsup = 0;
    bool blocking = false;

    if (!uri || !snmp_host_from_device_uri(uri, host, sizeof host)) {
        /* file:/usb: device — no SNMP */
        papplPrinterSetReasons(printer, PAPPL_PREASON_NONE, PAPPL_PREASON_OFFLINE);
        return true;
    }

    if (snmp_poll_printer(host, "public", &st) != 0) {
        /* printer unreachable */
        papplPrinterSetReasons(printer, PAPPL_PREASON_OFFLINE, PAPPL_PREASON_NONE);
        return true;
    }

    /* Map supplies */
    for (int i = 0; i < st.num_supplies && nsup < PAPPL_MAX_SUPPLY; i++) {
        snmp_supply_t *s = &st.supplies[i];
        pappl_supply_t *ps = &supplies[nsup++];
        memset(ps, 0, sizeof *ps);
        snprintf(ps->description, sizeof ps->description, "%s", s->description);
        ps->level = s->level_pct;  /* 0-100 or -1 */
        ps->is_consumed = true;
        /* color: Printer-MIB colorIndex 1=cyan 2=magenta 3=yellow 4=black */
        switch (s->color) {
        case 1: ps->color = PAPPL_SUPPLY_COLOR_CYAN; break;
        case 2: ps->color = PAPPL_SUPPLY_COLOR_MAGENTA; break;
        case 3: ps->color = PAPPL_SUPPLY_COLOR_YELLOW; break;
        case 4: ps->color = PAPPL_SUPPLY_COLOR_BLACK; break;
        default: ps->color = PAPPL_SUPPLY_COLOR_NO_COLOR; break;
        }
        /* type from description */
        if (strstr(s->description, "Toner")) ps->type = PAPPL_SUPPLY_TYPE_TONER_CARTRIDGE;
        else if (strstr(s->description, "Imaging")) ps->type = PAPPL_SUPPLY_TYPE_OPC;
        else if (strstr(s->description, "Fuser")) ps->type = PAPPL_SUPPLY_TYPE_FUSER;
        else ps->type = PAPPL_SUPPLY_TYPE_OTHER;
    }
    papplPrinterSetSupplies(printer, nsup, supplies);

    /* Map alerts → reasons (match on text — this printer uses group=3 for
     * everything, so group-based mapping is unreliable) */
    for (int i = 0; i < st.num_alerts; i++) {
        snmp_alert_t *a = &st.alerts[i];
        const char *t;
        if (a->severity <= 1) continue;  /* skip informational (power saver etc.) */
        t = a->text;
        if (!t[0]) continue;
        if (strstr(t, "paper") || strstr(t, "Paper") || strstr(t, "Tray")) {
            add_reasons |= PAPPL_PREASON_MEDIA_EMPTY;
            /* "Printer is able to print" in the text → warning only, not blocking */
            if (!strstr(t, "able to print")) blocking = true;
        } else if (strstr(t, "jam") || strstr(t, "Jam")) {
            add_reasons |= PAPPL_PREASON_MEDIA_JAM;
            blocking = true;
        } else if (strstr(t, "Toner") || strstr(t, "toner") || strstr(t, "ink")) {
            add_reasons |= PAPPL_PREASON_TONER_LOW;
            if (strstr(t, "empty") || strstr(t, "Empty")) blocking = true;
        } else if (strstr(t, "over") && (strstr(t, "Cover") || strstr(t, "cover") || strstr(t, "door") || strstr(t, "Door"))) {
            add_reasons |= PAPPL_PREASON_COVER_OPEN;
            blocking = true;
        } else if (a->severity >= 4) {
            add_reasons |= PAPPL_PREASON_OTHER;
        }
    }
    if (st.device_status == 6) {
        add_reasons |= PAPPL_PREASON_OFFLINE;
        blocking = true;
    }
    papplPrinterSetReasons(printer, add_reasons, PAPPL_PREASON_NONE);

    /* Pause the queue on blocking conditions so Windows shows
     * "Stopped"/"Attention required" instead of just "Idle" */
    if (blocking)
        papplPrinterPause(printer);
    else
        papplPrinterResume(printer);

    papplLogPrinter(printer, PAPPL_LOGLEVEL_INFO,
                    "SNMP status: %d supplies, %d alerts, device_status=%d, reasons=0x%x",
                    nsup, st.num_alerts, st.device_status, (unsigned)add_reasons);
    return true;
}

/* ------------------------------------------------------------------ */
/* Driver callback: set capabilities                                   */
/* ------------------------------------------------------------------ */

static bool driver_cb(pappl_system_t *system, const char *driver_name,
                      const char *device_uri, const char *device_id,
                      pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata)
{
    int i;
    (void)system; (void)device_uri; (void)device_id; (void)attrs; (void)cbdata;

    /* Identify the driver */
    for (i = 0; i < NUM_DRIVERS; i++) {
        if (!strcmp(driver_name, phaser6125_drivers[i].name)) {
            snprintf(data->make_and_model, sizeof(data->make_and_model), "%s", phaser6125_drivers[i].description);
            break;
        }
    }

    /* Printer-specific format (our HBPL stream) */
    data->format = HBPL_MIME;

    /* Raw passthrough callback (for clients sending pre-formatted HBPL) */
    data->printfile_cb = printfile_cb;

    /* Raster callbacks: required by PAPPL 1.4.x validation, but never
     * called — our MIME filter handles PDF/PS/JPEG/PNG conversion. */
    data->rstartjob_cb  = dummy_rstartjob;
    data->rstartpage_cb = dummy_rstartpage;
    data->rwriteline_cb = dummy_rwriteline;
    data->rendpage_cb   = dummy_rendpage;
    data->rendjob_cb    = dummy_rendjob;

    /* Raster types: required by validation.  We advertise RGB 8-bit so
     * PAPPL accepts the driver; PWG raster input is not yet implemented
     * (the MIME filter path is used for all supported input formats). */
    data->raster_types = PAPPL_PWG_RASTER_TYPE_RGB_8;

    /* Colour: the 6125N is a colour LED printer */
    data->color_supported = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_COLOR |
                            PAPPL_COLOR_MODE_MONOCHROME | PAPPL_COLOR_MODE_BI_LEVEL;
    data->color_default   = PAPPL_COLOR_MODE_AUTO;

    /* Resolutions: 600x600 (native), 300x300 (fast draft) */
    data->num_resolution   = 2;
    data->x_resolution[0]  = 600;
    data->y_resolution[0]  = 600;
    data->x_resolution[1]  = 300;
    data->y_resolution[1]  = 300;
    data->x_default        = 600;
    data->y_default        = 600;

    /* Media */
    data->num_media = NUM_MEDIA;
    for (i = 0; i < NUM_MEDIA; i++)
        data->media[i] = phaser6125_media[i];

    /* Default media: A4 with the hardware margins (398 = 11.28pt in 1/100 mm) */
    memset(&data->media_default, 0, sizeof(data->media_default));
    data->media_default.size_width    = 21000;
    data->media_default.size_length   = 29700;
    data->media_default.left_margin   = 398;
    data->media_default.right_margin  = 398;
    data->media_default.top_margin    = 398;
    data->media_default.bottom_margin = 398;
    snprintf(data->media_default.size_name, sizeof(data->media_default.size_name), "%s", "iso_a4_210x297mm");
    snprintf(data->media_default.source, sizeof(data->media_default.source), "%s", "tray-1");

    /* Media source */
    data->num_source = 1;
    data->source[0]  = "tray-1";

    /* Sides: the 6125N base model has no duplex */
    data->sides_supported = PAPPL_SIDES_ONE_SIDED;
    data->sides_default   = PAPPL_SIDES_ONE_SIDED;

    /* Printer kind */
    data->kind = PAPPL_KIND_DOCUMENT;

    /* Speed (approximate: 16 ppm colour, 20 ppm mono) */
    data->ppm       = 20;
    data->ppm_color = 16;

    /* Output orientation */
    data->output_face_up  = true;
    data->input_face_up   = false;

    /* Content / quality defaults */
    data->content_default = PAPPL_CONTENT_AUTO;
    data->quality_default = IPP_QUALITY_NORMAL;

    /* Supplies + status via SNMP bridge to the physical printer */
    data->has_supplies = true;
    data->status_cb    = snmp_status_cb;

    return true;
}

/* ------------------------------------------------------------------ */
/* System callback: create system, register MIME filters, set drivers  */
/* ------------------------------------------------------------------ */

static pappl_system_t *system_cb(int num_options, cups_option_t *options,
                                 void *data)
{
    pappl_system_t *system;
    pappl_soptions_t soptions;
    const char *val;
    int port = 0;
    pappl_loglevel_t loglevel = PAPPL_LOGLEVEL_WARN;
    const char *spooldir = NULL;
    const char *logfile  = NULL;
    (void)data;

    /* Parse options */
    if ((val = cupsGetOption("log-level", num_options, options)) != NULL) {
        if (!strcmp(val, "fatal")) loglevel = PAPPL_LOGLEVEL_FATAL;
        else if (!strcmp(val, "error")) loglevel = PAPPL_LOGLEVEL_ERROR;
        else if (!strcmp(val, "warn")) loglevel = PAPPL_LOGLEVEL_WARN;
        else if (!strcmp(val, "info")) loglevel = PAPPL_LOGLEVEL_INFO;
        else if (!strcmp(val, "debug")) loglevel = PAPPL_LOGLEVEL_DEBUG;
    }
    if ((val = cupsGetOption("server-port", num_options, options)) != NULL)
        port = atoi(val);
    spooldir = cupsGetOption("spool-directory", num_options, options);
    logfile  = cupsGetOption("log-file", num_options, options);

    soptions = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_WEB_INTERFACE |
               PAPPL_SOPTIONS_WEB_LOG | PAPPL_SOPTIONS_WEB_NETWORK |
               PAPPL_SOPTIONS_WEB_REMOTE | PAPPL_SOPTIONS_RAW_SOCKET;

    system = papplSystemCreate(soptions, APP_NAME, port, "_print,_universal",
                               spooldir, logfile, loglevel, NULL, false);
    if (!system) return NULL;

    /* Add TCP listeners ourselves: when a custom system_cb is used, PAPPL's
     * _papplMainloopRunServer only adds the Unix domain socket (non-Windows)
     * — the TCP listener code lives in default_system_cb which is never
     * called. "0.0.0.0" binds all IPv4 interfaces on the configured port.
     * Works on both Linux and Windows (papplSystemAddListeners handles
     * numeric hostnames as IPv4 on all platforms). */
    {
        const char *listen_host = cupsGetOption("listen-hostname", num_options, options);
        if (!listen_host) listen_host = "0.0.0.0";
        papplSystemAddListeners(system, listen_host);
    }

    papplSystemSetFooterHTML(system, APP_FOOTER);

    /* Start minimal mDNS advertiser so Windows/macOS "Add printer" finds
     * us automatically — no Bonjour/dnssd.dll needed (RFC 6762 multicast).
     * Advertise on the configured port; rp points at the default queue. */
    if (port <= 0) port = 8631;
    if (!g_mdns)
        g_mdns = mdns_start("Xerox Printer 6125N", (uint16_t)port, "ipp/print/Phaser6125");

    /* Register MIME filters: common input formats → our HBPL format.
     * The filter callback runs gs → PPM → hbpl_core for each job. */
    papplSystemAddMIMEFilter(system, "application/pdf",        HBPL_MIME, hbpl_filter_cb, NULL);
    papplSystemAddMIMEFilter(system, "application/postscript", HBPL_MIME, hbpl_filter_cb, NULL);
    papplSystemAddMIMEFilter(system, "image/jpeg",             HBPL_MIME, hbpl_filter_cb, NULL);
    papplSystemAddMIMEFilter(system, "image/png",              HBPL_MIME, hbpl_filter_cb, NULL);
    papplSystemAddMIMEFilter(system, "text/plain",             HBPL_MIME, hbpl_filter_cb, NULL);

    /* Register printer drivers */
    papplSystemSetPrinterDrivers(system, NUM_DRIVERS, phaser6125_drivers,
                                 autoadd_cb, NULL, driver_cb, NULL);

    return system;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    return papplMainloop(argc, argv, APP_VERSION, APP_FOOTER,
                         0, NULL, NULL, NULL,   /* drivers set in system_cb */
                         NULL, NULL,            /* no custom sub-command */
                         system_cb,             /* custom system callback */
                         NULL, NULL);           /* default usage, no data */
}
