# RE-NOTES — sq21/HBPL codec & native 64-bit driver

Reverse-engineering results for the Xerox Phaser 6125N (Fuji Xerox HBPL,
@DRDM=RASTER). See `ANALYSIS.md` for the analysis of the original 32-bit
filter chain. Everything below is verified against the real printer
(192.168.178.10, raw JetDirect port 9100).

## 1. The sq21 raster codec (identical to "XRC" in foo2zjs)

Single interleaved bitstream, MSB-first.

- **Tokens** `HID[hsel][k]`, canonical Huffman, 2 rows:
  - `k=0..4`: run of `run` pixels copied from predictor `order[k]` (move-to-front)
  - `k=5`: diff — 4 byte-diffs (HPE0) added to the left pixel
  - `k=6`: switch hsel row (code `11111` + one extra 0 bit)
- **Run lengths** 1..17057: HRN0 short codes / `111` + 3-bit group + extra bits.
  17057 = 674 + 14 bits (maximum encodable run; encoder chunks longer runs).
- **Byte diffs**: HPE0 codebook — identical to Dave Coffin's `put_diff()`/`get_diff()`
  (proven: tables in Fuji's libsq21 == foo2hbpl1's XRC; see foo2zjs PR #24 /
  foomatic-db PR #59 — same codec, different container).
- **Predictors** (semantic, per pixel): 0=left 1=up 2=up-left 3=up-right 4=up+2right.
  Outside the image read as `ff ff ff ff` (sq21 pixbuf margins; right edge up-right /
  up+2right hit the 0xff sentinel slot).
- **Bit bookkeeping**: token index = `bdir - bit`; `bit = (run < 17057)` after runs,
  `bit = 0` after a diff and after each 17057 chunk. The MTF rotor (`order[]`) shifts
  along when `bdir != 0`. Encoder switches hsel row when `|stat| > 8` (emits HID[6] + one 0 bit).
- **End marker**: HID[6] + `111` + 1 bit padding (byte-aligned).
- **Image layout**: (H+1) rows × (W+2) slots × 4 bytes `[ff,R,G,B]`; row 0 = virtual
  all-ff; slot 0 = left margin ff; slot W+1 = right sentinel ff.

## 2. HBPL container (this printer: @DRDM=RASTER, single interleaved stream)

- **PJL header**: fixed template, byte-exact from capture `tests/testpage-600dpi.hbpl`
  (1079 bytes; only DATE/TIME/DNAME/user/host/copies/resolution vary).
  Contains e.g. `@PJL SET RENDERMODE=COLOR`, `@PJL SET JOBATTR=\"@DRDM=RASTER\"`,
  `@PJL ENTER LANGUAGE=HBPL`.
- **A-record** 12 bytes (once, right after ENTER LANGUAGE).
- **Per page**:
  - C-record 78 bytes: margins u16le @24-27, pageno u32le @33-36 (1-based),
    w u32le @39-42, h u32le @43-46, w/h again @70-77
  - QR-record 12 bytes: total u32le @8-11 = 24 + payload length
  - body 24 bytes (fixed): `18 00 00 00 00 01 00 00 10 32 04 00 a1 42 00 00 00 00 ff 00 00 00 00 00`
  - payload (sq21 stream)
  - `"SD"` 2 bytes
- The head (C+QR+body) is a fixed template with 3 patch points:
  `9a c4` w/h @51-58, `a2 c4` w/h @82-89, QR total @98-101.
- **Job end**: `'B'` + `ESC \"%-12345X\"` + `\"@PJL EOJ\n\"` (19 bytes).
- **Copies**: `@PJL SET COPIES=n`; pages are NOT replicated in the stream.
- NB: foo2hbpl1's XRC framing (@DRDM=XRC, 5-stream layout) is **rejected** by this
  printer — it wants RASTER + single stream.

## 3. FXR format (intermediate stages of the original chain)

- 44-byte header = 11× uint32 LE `[w,h,bpp,rowbytes,datasize,resX,resY,0,0,f9,f10]`
- `f9`: 1 = raw RGB 24bpp, 2 = sq21 (f10=3)
- FXM_ALC output = FXR header + sq21 payload (without 16-byte magic header);
  FXM_HBPL stripped the FXR header (w/h are in the HBPL 9a/a2 records).
- sq21 magic = `0xc2c0cfba` (bytes `ba cf c0 c2`).

## 4. PM2FXR crop rule (derived from the PPD itself)

- PPD `*ImageableArea <PageSize>/...: "left bottom right top"` in pt; `r = dpi/72`
- `x0=round(left·r)`, `x1=round(right·r)`, `y0=in_h−round(top·r)`, `y1=in_h−round(bottom·r)`
- pass-through (no crop) when the input fits inside the imageable area
- A4@600: crop @(94,94) → 4770×6829; Letter@600: @(94,94) → 4912×6412; A4@300: @(47,47) → 2385×3414
- gs page sizes: A4@600 → 4958×7017 (gs rounds 842×600/72=7016.67 → 7017) for all
  invocation variants; only explicit `-g4958x7016` gives 7016
- FXM_ALC pads the width to a multiple of 8 (4770→4776); pad pixels = `ff ff ff ff`
- 6125N imageable margins: 11.28pt (~4mm) on all sides

## 5. Windows driver (DPC525A DISM export, 64-bit)

- `fxhl2zim.dll` = "Fuji Xerox HBPL Printer Driver" (64-bit PJL/HBPL output + image processing)
- `fxhzdczd.dll` (1.5 MB) contains the sq21 magic `ba cf c0 c2`
- `fxzarl32.dll` = CentreWare Address Resolution Library (NOT the codec)
- `fxhl2z.xrs` = resources (page sizes/margins), `fxhl2z.cdd` = color data, `fxhl2zp1.icm` = ICC
- Conclusion: usable as 64-bit reference, not blocking (crop offsets came from the PPD).

## 6. Native driver (`native/foo2sq21hbpl.c` + `native/hbpl_core.c`)

CUPS filter, libc only, native 64-bit (no multilib). Interface as CUPS expects
from filters: `foo2sq21hbpl job-id user title copies options [file]`,
environment `PPD`.

Pipeline:
1. parse options (`PageSize`/`PageRegion`, `FXOutputMode`→dpi (300/600),
   `FXColorMode` Black→gray, rest → color, `FXDocumentType`→FXDLUT block code)
2. PPD lookup (`PaperDimension`, `ImageableArea` for the page size)
3. fork/exec `gs` (`-r{dpi} -dDEVICEWIDTHPOINTS= -dDEVICEHEIGHTPOINTS=` from the PPD,
   `-sDEVICE=ppmraw -sOUTPUTFILE=%stdout`)
4. per page: parse P6 → crop (PM2FXR rule) → **per-pixel color calibration**
   (FXM_CC equivalent, §9: color = 3Dlut(CST(rgb)), gray = NTSC(CST(rgb)))
   → expand to 4-byte `[ff,R,G,B]` → pad width to multiple of 8
   (`ffffffff`) → sq21-encode → C/QR/body/payload/"SD" per page
5. PJL header + A-record once; job trailer at the end

Color calibration (ON by default; algorithm in §9, verification in §7):
- CST tables: `native/cst_tables.h` (generated, in git —
  `re/gen_cst_tables.py` from `re/extract-tables.py`)
- 3D LUT: `/usr/share/phaser6125/Phaser_6125N.dlut` (installed by
  `scripts/install-native-driver.sh`)
- env `FOO2SQ_CALIBRATE=0` disables calibration (raw, or gray-averaged)
- env `FOO2SQ_DLUT=<path>` overrides the .dlut path; without a usable .dlut
  the filter falls back to CST-only (like FXM_CC itself)
- gray uses no LUT (only CST + NTSC)

The encoder is **exactly lossless** (the official FXM_ALC is near-lossless, ±1-2 per
pixel — both are valid streams; our payload is ~0.8% larger).

### Core split (hbpl_core.c / hbpl_core.h)

Since the PAPPL integration (see §12) the encoder is split:
native/hbpl_core.c contains the complete pixel pipeline (sq21 codec, HBPL
container, FXM_CC calibration, PPM reader, PJL header, per-page processor,
trailer) as a reusable library — pure C99 + stdio, no POSIX. The CUPS
filter (native/foo2sq21hbpl.c) is now just a thin main(): parse CUPS argv,
fork gs, and feed the PPM stream through hbpl_page(). The same core
is reused by:
- win/hbblenc.c — standalone Windows tool (PPM stdin → HBPL stdout)
- pappl/phaser6125-pappl.c — PAPPL printer application (MIME filter:
  PDF → gs → PPM → hbpl_page() → device)

Both consumers are verified byte-identical (binary-identity regression
against the installed CUPS filter).

Build (clean under gcc -Wall -Wextra and clang -Weverything -Werror; tested
on clang 14 through 21):

```
gcc   -O2 -Wall -Wextra -o foo2sq21hbpl native/foo2sq21hbpl.c native/hbpl_core.c -lm
clang -Weverything -Werror -Wno-unsafe-buffer-usage -Wno-unknown-warning-option \
      -O2 -o foo2sq21hbpl native/foo2sq21hbpl.c native/hbpl_core.c -lm
```

`-Wno-unsafe-buffer-usage`: clang's experimental C++ bounds-safety lint
(included in `-Weverything` since clang 16); in C not solvable without
`_Ptr` annotations that gcc doesn't support. `-Wno-unknown-warning-option`
keeps the command line compatible with clang older than 16 (which doesn't
know the first flag).

## 7. Verification

- `re/sq21dec.py` — pure-Python sq21 decoder (cross-checked against Dave Coffin's
  `hbpldecode.c`); CLI: `sq21dec.py <file> <W> <H> [offset] [out.raw]`
- `re/sq21dec.c` — C port (~60x faster: 12.5s → 0.21s per A4@600 page),
  byte-identical output verified on all captures; build as CLI and as
  `libsq21dec.so` (ctypes). Also contains `sq21_expand` (crop+expand+pad in C).
- `re/verify-native.py <input.ps|pdf> \"<options>\"` — end-to-end: structure asserts
  (PJL/A/C/QR/body/SD/trailer) + per-page lossless round-trip vs a
  standalone gs render. Uses the C decoder; pages in parallel (ctypes releases
  the GIL). A4@600 color page verifies in ~1.5s.
- `re/verify_model.c` — byte-exact model of the FXM_CC color stage (CST +
  tetrahedral 3D LUT); parameters from live gdb dumps of the 32-bit binary
  (`re/dump-params.gdb`: `cst-param.bin`, `dlut-param.bin`, `xyz2srgbtrc.bin`).
  **Byte-exact** against ground truth: all 256^3 colors (synth.fxr→synth-cc.fxr)
  and a real page (s3-pr.fxr→s4-cc.fxr, 500,990 px).
- `re/calib.c` — oracle for the driver: same model, but reading from the .dlut
  file (like the driver does). Build as CLI (FXR→FXR, cross-check vs ground truth)
  and as `re/libcalib.so` (ctypes, for verify-native.py). Byte-exact against
  ground truth: synth (16,777,216 px), s3→s4 (500,990 px), gray-test→gray-out.
- `re/extract-tables.py` + `re/gen_cst_tables.py` — extract the CST tables from the
  FXM_CC ELF and generate `native/cst_tables.h` (in git; do not edit by hand).
- `re/FXM_CC-decompiled.c` — Ghidra decompilation of the 32-bit FXM_CC (16
  functions; `main` shows the CST→3Dlut order). Regenerate:
  `re/DecompileFxmCc.java` (Ghidra headless, Java script).
- Tested & EXACT (verify-native.py, A4@600 colorpage.pdf, ~32.6M px/page):
  raw (`FOO2SQ_CALIBRATE=0`), calibrated color (`FXDocumentType` Normal and
  Photo — two different DLUT blocks), calibrated gray
  (`FXColorMode=Black`), gray average, 1-page and 2-page (page numbering +
  stream alignment). The calibrated round-trip compares against the
  oracle (re/calib.c) — the driver is thus **byte-exact equal to the original
  FXM_CC transform**.
- Earlier tests & EXACT: A4@300, Letter@600, direct PDF input.
- **Real printer**: 1-page print (page count 8660→8661), 2-page print
  (8661→8663), colorpage with meahow (8663→8664); job log shows `Completed`
  with the right page count. (Printer is shared; counter is now at 8669 —
  be frugal with test pages.)

## 8. Installation (native, 64-bit)

```
sudo bash scripts/setup-cups-native.sh       # CUPS + ghostscript (no i386!)
sudo bash scripts/install-native-driver.sh   # build+install filter, PPD, queue
bash scripts/test-print.sh                   # prints tests/colorpage.pdf
```

- filter → `/usr/lib/cups/filter/foo2sq21hbpl` (root:root 0755)
- PPD → `/usr/share/cups/model/Xerox/en/Phaser_6125N-native.ppd`
  (same options as the original PPD; `*cupsFilter` points to foo2sq21hbpl)
- .dlut → `/usr/share/phaser6125/Phaser_6125N.dlut` (color calibration, §9; the
  install script also does a `clang -Weverything -Werror` sanity build)
- queue `Phaser6125N` → `socket://192.168.178.10:9100`

### CUPS "Printer drivers are deprecated" warning

`lpadmin` prints this message because PPD-based drivers (PPD + filter in
`/usr/lib/cups/filter`) will be removed in a future CUPS (3.x, OpenPrinting) in favor
of driverless IPP. For this setup it's a false alarm:

- Ubuntu 22.04 ships CUPS 2.4.1 **with** PPD/driver support; 22.04 is supported
  until 2027 (and WSL2 can stay on it even longer).
- The Phaser 6125N is a raw printer (port 9100/515/80, **no IPP**) — without
  PPD+filter there is NO driverless/IPP-Everywhere possible. PPD+filter is
  the only workable route here.
- Our filter is standalone: printing always works without CUPS via
  `native/foo2sq21hbpl 1 <user> <title> 1 \"<options>\" <file> | nc 192.168.178.10 9100`
  (the first test prints in this session went exactly like that).
- If you ever want to move off the PPD: see "Future: CUPS 3.x / driverless
  (ippserver)" below.

### Compatibility (tested in WSL distros)

| Ubuntu | CUPS | gcc | clang | ghostscript | status |
|--------|------|-----|-------|-------------|--------|
| 22.04 (this WSL) | 2.4.1 | 11.4 | 14 | 9.55 | ✓ everything works (build, CUPS queue, print) |
| 24.04 | 2.4.7 | 13.3 | 18 | 10.02 | ✓ everything works (gcc + clang -Weverything -Werror build, verify raw/calibrated color/gray EXACT, CUPS queue; print skipped) |
| 26.04 | 2.4.16 | 15.2 | 21 | 10.06 | ✓ everything works (gcc + clang -Weverything -Werror build, verify raw/calibrated color/gray EXACT, CUPS queue, **real print** — counter 8669→8670, job Completed) |

On 26.04 the filter build needs `libc6-dev` (not present by default) —
`scripts/test-compat-distro.sh` installs it along.

`scripts/test-compat-distro.sh` tests the whole repo in a WSL distro
(22.04/24.04/26.04): unpack repo, build filter (gcc + clang
`-Weverything -Werror`), run verify-native (raw + calibrated),
create queue, print (or `SKIP_PRINT=1`).

### Without CUPS (fallback)

`scripts/print-direct.sh <file.pdf|ps> [\"options\"]` — runs the filter
directly and sends the HBPL stream to `PRINTER_IP:9100` (raw JetDirect, `nc`).
Builds the filter itself if it isn't there yet. Works on any Linux with
gcc + ghostscript, without CUPS.

### Fetching status (extra gimmick)

`scripts/printer-status.py` — read-only status, two sources:
- **HTTP** (CentreWare web interface): global status, page count, paper trays,
  supplies, last job-log lines. (The pages end in a NUL byte; it is stripped.)
- **SNMP**: this printer speaks **only SNMPv1**, community `public`
  (`snmpwalk -v1 -c public …`); `prtMarkerSupplies` (RFC 1759) for
  description/max/level.

Env: `PRINTER_IP` (default 192.168.178.10), `SNMP_COMMUNITY`, `SNMP_TIMEOUT`.
Known limitation: **toner levels are not readable** — the cartridges are
non-original, the printer reports `--` (HTTP) / level -2 (SNMP). Imaging Unit
and Fuser do report percentages. (Gimmick/TODO: with original toners
the levels would be directly readable here.)

### Future: CUPS 3.x / driverless (ippserver)

CUPS 3.x removes PPD+filter drivers; the route then is IPP Everywhere. This
printer has no IPP itself, but our filter is a standalone binary (it writes the
complete PJL/HBPL stream to stdout) — the migration is small:

1. install `cups-ipp-utils` (contains `ippeveprinter`; in CUPS 3.x this is
   called `ippserver`)
2. install the command wrapper as a CUPS command filter
   (`scripts/ippeve-wrapper.sh` → `/usr/lib/cups/command/foo2sq21hbpl-ippeve`):
   reads the document from stdin, calls the filter, writes HBPL to stdout
3. run the printer as an IPP service — output goes to the raw socket:
   ```
   ippeveprinter -m "Phaser 6125N" -M "Xerox" -l "Home" \
       -D socket://192.168.178.10:9100 \
       -f application/pdf,application/postscript \
       -s 10,10 \
       -c /usr/lib/cups/command/foo2sq21hbpl-ippeve \
       "Phaser6125N"
   ```
   (CUPS 3.x: `ippserver` with the same options.) Clients then print driverless
   via IPP Everywhere to this host.

Not needed now: 22.04/24.04/26.04 all run CUPS 2.4.x with PPD support,
and `scripts/print-direct.sh` is the CUPS-free fallback. The wrapper is tested
locally: stdin → byte-identical HBPL stream as the direct filter.

## 9. Known limitations / follow-up work

- **Color calibration (FXM_CC + .dlut) — REPLICATED, byte-exact verified**
  (integration in the driver: §6; verification: §7). The cracked algorithm
  (Ghidra decompilation `re/FXM_CC-decompiled.c` + parameters from live gdb
  dumps of the 32-bit binary):

  **Pipeline** (from `main`): `color: out = 3Dlut(CST(in))`,
  `gray: out = NTSC(CST(in))` — CST is applied first, THEN the 3D LUT.

  **CST** (per pixel, 8-bit in → 8-bit out; lossy 8→16→12→8):
  1. gamma-2.2 TRC: `CST_TRC[i] = round(65535·(i/255)^2.2)` (8→16 bit)
  2. 3×3 matrix signed Q16: `out = ((M[i]>>2)·v + …) >> 18` == `m·v/16`
     (16→12 bit; signed arithmetic shifts — `sar`/`imul` in the assembly).
     `CST_M = {65543, 0, 5, -3, 65541, 3, -2, -1, 65535}` — sRGB primaries
     with gamma22TRC (≈ identity, small off-diagonals)
  3. clamp `[0, 0xfff]`
  4. inverse TRC `CST_ITRC` (4096 entries, 12→8 bit)
  Tables: `native/cst_tables.h` (generated, in git).

  **3D LUT** (tetrahedral, 17³ nodes):
  - node index `(c2>>4)·289 + (c1>>4)·17 + (c0>>4)`; fractions `f = c&0xf`
    (deltas 289/17/1), descending sort
  - weights `w0=16-f[0]`, `w1=f[0]-f[1]`, `w2=f[1]-f[2]`, `w3=f[2]` (sum 16)
  - nodes `base`, `base+d[0]`, `base+d[0]+d[1]`, `base+307` (c111)
  - result `(Σ w·node) >> 4` (≤ 255; clamp unnecessary)
  - white shortcut: `(c0 & c1 & c2) == 0xff` → `(255,255,255)`

  **.dlut format** (`driver/Phaser_6125N.dlut`, 265944 bytes, big-endian):
  - header: magic `DLUT`, u32be count=13, 13× u32be block-offsets,
    u32be metacount=79, 79× u32be `meta[]`
  - `meta[FXDLUT]` = 1-based block number; FXDLUT comes from the PPD
    (`FXDocumentType` setpagedevice: Normal 24, Photo 22, Presentation 26,
    Web 23, POP 32)
  - block data at `offsets[block-1]+8` (8-byte `dlt4`+size header), 20420
    bytes = 768 bytes 1D curves (unused in the pixel path) + 17³×4 LUT
  - block selection from meta[]: Normal 24→block 3, Photo 22→block 1,
    Presentation 26→block 5, Web 23→block 2, POP 32→block 11
  - **file vs memory**: FXM_CC keeps each 4-byte node reversed in memory —
    dump node = `[out2, out1, out0, pad]`, file node =
    `[pad, out0, out1, out2]` (proven: block 3 == `re/build/dlut-param.bin`
    with each node byte-reversed). The driver reads the file format.

  **Gray**: `g = (c0·77 + c1·151 + c2·28) >> 8` over the CST output
  (NTSC 0.30/0.59/0.11; verified: `gray-test.fxr` → `4c 96 1b 81`).

  **Ground truth**: 32-bit FXM_CC via the i386 sysroot (`re/run-stage.sh`);
  captures `synth.fxr→synth-cc.fxr` (all 256^3 colors),
  `s3-pr.fxr→s4-cc.fxr` (real page, 500,990 px), `gray-test.fxr→gray-out.fxr`.
  Model (`re/verify_model.c`) and oracle (`re/calib.c`) are byte-exact
  against these; the driver is byte-exact against the oracle (`re/verify-native.py`).

  **Env**: `FOO2SQ_CALIBRATE=0` (calibration off), `FOO2SQ_DLUT=<path>` (override);
  default `.dlut` = `/usr/share/phaser6125/Phaser_6125N.dlut`. Without a
  usable .dlut: CST-only (like FXM_CC itself).
- FXM_SBP / FXM_PR details (gray / turnpage) not replicated.
- The official chain is near-lossless; ours is exactly lossless.
- CUPS runs filters as user `lp`; `/usr/lib/cups/filter` must be root:root 0755.

## 10. USB (from Windows or Linux)

The Phaser 6125N has a **USB 2.0 device port** (type B) next to Ethernet.
The Windows driver INF (`analysis/windriver/…/fxhl2zi.inf`) matches the device
as `USBPRINT\\FUJI_XEROXDocuPrint_13C9` — a **standard USB Printing Class**
device (Windows `usbprint.sys`), no vendor-specific USB stack. Also
`LPTENUM\\FUJI_XEROXDocuPrint_13C9` (parallel port) is supported.
The PJL/HBPL protocol works over USB identically to raw JetDirect (9100).
(The INF is from the C525A — the sister model; the 6125N uses the same
engine, but the exact USB string may be `XEROX Phaser 6125…`.)

Three routes, from quick/simple to "own driver end-to-end":

### B. Raw USB from Windows (recommended — no driver needed)

Windows automatically attaches *USB Printing Support* (usbprint.sys) to the
device; the port shows up as `USB001` (or USB002…). Our filter generates
a complete PJL/HBPL stream — send it raw to the port:

1. USB A-B cable printer → Windows PC; in Device Manager *USB Printing Support*
   appears under *Universal Serial Bus devices*.
2. Generate the stream in WSL (Linux):
   ```
   cd /home/rick/src/phaser6125
   PPD="$PWD/driver/Phaser_6125N-native.ppd" ./native/foo2sq21hbpl \
       1 rick test 1 "PageSize=A4 FXOutputMode=600x600 FXColorMode=Color" \
       tests/colorpage.pdf > /tmp/colorpage.hbpl
   ```
3. Copy to Windows (e.g. via `\\\\wsl$\\Ubuntu\\home\\rick\\…` → `C:\temp\`) and print:
   ```
   copy /b C:\temp\colorpage.hbpl \\\\.\USB001
   ```

   **Update (2026-11-02, verified on WINNIE):** `\\.\USB00x` is **not** a
   DOS device file on current Windows 11 — `copy /b` and `CreateFile` both
   return err 2 (FILE_NOT_FOUND), and `dir \\.\` shows no USB entries.
   USB print ports are only reachable via the print spooler
   (OpenPrinter/WritePrinter), not as device files. So on Windows 11 24H2+
   use a **raw printer share** instead (see below) or route C (usbipd+CUPS)
   for USB, or the network (socket://) via our PAPPL gateway (section 12,
   fully verified).

   **Raw share route (works on Win11 24H2, verified):** create a raw printer
   queue on the USB port and share it — `copy /b` to the share goes through
   the spooler straight to the device:
   ```powershell
   Add-Printer -Name "Phaser Raw" -DriverName "Generic / Text Only" -PortName USB001
   Set-Printer -Name "Phaser Raw" -Shared $True -ShareName "PhaserRaw"
   ```
   ```bat
   copy /b C:\temp\colorpage.hbpl \\%COMPUTERNAME%\PhaserRaw
   ```
   The same channel is used by our gateway's `usbprint://HOST/PhaserRaw`
   device URI (OpenPrinter + StartDocPrinter(RAW) + WritePrinter — see §12).

### C. WSL2 + usbipd + CUPS usb:// (our native driver, end-to-end)

`usbipd` (free, `winget install usbipd`) forwards a USB device to WSL2; CUPS'
usb backend (libusb) then prints with our own driver:

```
# Windows (admin PowerShell)
usbipd wsl list
usbipd wsl attach --busid <busid>
# in WSL
lpinfo -v | grep usb          # gives the usb:// URI
sudo lpadmin -p Phaser6125N -v usb://FUJI_XEROX/DocuPrint_13C9
```

### A. Vendor Windows driver via USB (least interesting for this project)

The DPC525A DISM export (or the official Xerox 6125N Windows driver) can install
via USB (LanguageMonitor "SimpleMonitor", `fxhl2zi.inf`) — but that is "just
printing" with a 2012 driver (DriverVer 2.5.34.0), independent of our
reverse-engineering.

### Native Linux

`scripts/print-direct-usb.sh <file.pdf|ps> [\"options\"]` — filter →
`/dev/usb/lp0` (usblp kernel module; env `USB_DEVICE` override). In WSL2
`/dev/usb/lp0` does not exist (usbipd exposes the device via libusb) —
route C is the way there.

**Careful**: the printer is shared — every test page costs a page. Check the
device first (Device Manager / `lpinfo -v`), only print when the device is
confirmed. Optionally check in CentreWare IS (http://192.168.178.10, read-only)
whether the USB port is enabled.

## 11. Windows 11 driverless via IPP (context: V3/V4 driver deprecation)

Microsoft is phasing out legacy V3/V4 printer drivers (end-of-servicing plan,
learn.microsoft.com "End of servicing plan for third-party printer
drivers"): no new V3/V4 since Sept 2023, no new ones via Windows Update since
15 Jan 2026 (Win11+/Server 2025+), from 1 July 2026 preference for the IPP
inbox class driver, after July 2027 only security fixes. Existing drivers keep
working, but printers without IPP/Mopria fall back to manually installed drivers.

The Phaser 6125N speaks **no IPP/Mopria** (proven: CentreWare IS HTTP,
SNMPv1-only, raw 9100 HBPL) — the vendor driver (route A, §10) is a dead end on
Windows 11 24H2+ in the long run.

**Our setup is already the userspace translation** the new model wants:
`foo2sq21hbpl` renders PDF → HBPL in userspace (ghostscript + our chain),
and the WSL CUPS is an IPP print server. What was missing is only the
Windows→WSL IPP connection (driverless, inbox class driver):

1. `sudo bash scripts/setup-wsl-ipp.sh` — sets `Listen *:631` in cupsd.conf
   (backup `.pre-ipp`), restarts CUPS, tests the IPP endpoint via the WSL IP
   with `ipptool get-printer-attributes`
2. Windows: Settings → Printers & scanners → *Add device* →
   *The printer that I want isn't listed* → *Select a shared printer by name* →
   `http://<wsl-ip>:631/printers/Phaser6125N` — Windows detects IPP and uses the
   **inbox class driver** (no V3 driver)
3. Print from Windows → CUPS (WSL) → `foo2sq21hbpl` → HBPL → printer
   (socket 9100, or USB via §10 route C)

Firewall: Windows Firewall sometimes blocks inbound to WSL — add a rule for
port 631, or WSL2 mirrored networking (`[wsl2] networkingMode=mirrored` in
`%UserProfile%\\.wslconfig`, then `wsl --shutdown`) — WSL then gets the same IP
as Windows.

**USB as fallback** (§10): the raw-share route (`copy /b … \\HOST\\PhaserRaw`)
is entirely outside the print stack — immune to deprecation. Route C
(usbipd + CUPS `usb://`) is driverless but via CUPS.

For multiple printers / an appliance: PAPPL (github.com/istopwg/pappl)
or a Raspberry Pi + CUPS — our repo is already compat-tested on
22.04/24.04/26.04 (`scripts/test-compat-distro.sh`).


## 12. PAPPL printer application (driverless IPP, Windows-proof)

`pappl/phaser6125-pappl.c` wraps the HBPL encoder (`native/hbpl_core.c`) as a
[PAPPL](https://github.com/michaelrsweet/pappl) printer application. PAPPL is
the reference implementation for IPP Everywhere / Mopria printer apps — it
makes the Phaser 6125N appear as a **driverless IPP printer** on Windows 11
(inbox class driver), macOS, iOS, Android and Linux. This is the long-term
solution against the V3/V4 driver deprecation (§11): no vendor driver needed.

### Architecture

```
Client (Windows/macOS/Linux) → IPP (PDF/PS/JPEG/PNG)
  → PAPPL server (this app, TCP 8631 or Unix socket)
  → MIME filter (hbpl_filter_cb: gs → ppmraw → hbpl_core → HBPL)
  → device (socket://192.168.178.10:9100, usbprint://HOST/PhaserRaw, file:)
```

The MIME filter follows the OpenPrinting "non-raster printer" pattern
(openprinting.github.io/documentation/02-designing-printer-drivers):
`papplSystemAddMIMEFilter(system, "application/pdf", "application/vnd.xerox-hbpl", cb, NULL)`.

### Build & run (Linux, PAPPL 1.4.x)

```sh
# deps: libcups2-dev libavahi-client-dev + PAPPL 1.4.x installed in /usr/local
bash scripts/build-pappl-app.sh

LD_LIBRARY_PATH=/usr/local/lib pappl/phaser6125-pappl server -o server-port=8631 &
LD_LIBRARY_PATH=/usr/local/lib pappl/phaser6125-pappl add -d phaser     -m xerox_phaser_6125n -v socket://192.168.178.10:9100
```

### PAPPL 1.4.x gotchas (learned the hard way)

- **Custom `system_cb` skips TCP listeners**: `_papplMainloopRunServer` only adds
  the Unix domain socket when a custom system_cb is used. The TCP listener code
  lives in `default_system_cb` (never called). Fix: call
  `papplSystemAddListeners(system, "0.0.0.0")` inside our own `system_cb`.
- **MIME filter signature**: `bool cb(pappl_job_t *job, pappl_device_t *device, void *data)`
  — no doc_number, no options. Get options via `papplJobCreatePrintOptions(job, 0, true)`.
- **Default resolution bug**: when the client doesn't send `printer-resolution`,
  `options->printer_resolution[0]` is 300 (PAPPL internal default), NOT the
  advertised `printer-resolution-default=600dpi`. Fix: check
  `papplJobGetAttribute(job, "printer-resolution")` — if NULL, force 600 dpi.
- **Validation** in `papplPrinterSetDriverData`: `format` without `printfile_cb`
  → ERROR; all 5 raster callbacks required → use dummies; `raster_types` non-zero.
- `cupsCopyString` doesn't exist in CUPS 2.4 — use `snprintf`.
- `-o printer-resolution=600x600` is parsed as 600 **dpcm** — use `600x600dpi`.
- `add` CLI: `-d NAME` is required (positional arg = filename).
- `file:` device must exist before `add` (`touch` first).
- Server CLI: no `-l`/`-d` short options for log/spool — use `-o log-file=...` etc.
- Set the system hostname explicitly (`papplSystemSetHostName` with `<host>.local`)
  so the mDNS `adminurl` is not `localhost.local`.

### Verification (all PASS)

- `ipptool Get-Printer-Attributes` via TCP 8631: advertises
  `printer-resolution-default=600dpi`, `document-format-supported` includes
  pdf/ps/jpeg/png/text + `application/vnd.xerox-hbpl`
- `ipptool Print-Job` (PDF, no explicit resolution, like Windows) → job accepted,
  output **byte-identical** to the CUPS filter reference (after DATE/TIME/DNAME
  normalisation), `@PJL SET RESOLUTION=600`
- Real-hardware Windows 11 test: Create-Printer via IPP → job-state=completed →
  bytes to the physical printer (network and USB)

### USB via usbprint: scheme (verified on WINNIE 2026-11-02)

`usbprint:` device scheme registered via `papplDeviceAddScheme`:
- `usbprint://./USB001` → CreateFile/WriteFile on the device file (legacy; err 2 on Win11 24H2+)
- `usbprint://HOST/PhaserRaw` → OpenPrinter + StartDocPrinter(RAW) + WritePrinter
  (spooler channel, same as `copy /b file \\HOST\PhaserRaw`) — **verified working**:
  263,242 bytes HBPL written to the USB printer, job completed.
`run-portable.bat` auto-creates the `PhaserRaw` share (Generic/Text Only on USB001).
`PRINTER_DEV` selects socket:// (network) or usbprint:// (USB).

### mDNS/DNS-SD auto-discovery

- `native/mdns_mini.c` (~400 lines, RFC 6762) advertises `_ipp._tcp.local` +
  `_universal._sub._ipp._tcp.local` via multicast 224.0.0.251:5353 with a full
  IPP Everywhere TXT record set (rp, ty, product, pdl, URF=DM3, UUID=printer-uuid,
  mopria-certified, Color, Duplex, adminurl, …). Verified via unicast PTR queries.
- On Windows 10/11 UDP 5353 is always held by the OS mDNS responder or Bonjour —
  mdns_mini.c is a fallback only. PAPPL's built-in DNS-SD path (LoadLibrary
  dnssd.dll) advertises `_ipp._tcp` with full TXT records when Bonjour is present.
  **CONFIRMED: printer appears in Windows 11 "Add device" auto-discovery**
  (user-verified on WINNIE, 2026-11-02). tshark capture: `PTR Xerox Printer 6125N._ipp._tcp.local.`,
  `TXT rp=ipp/print/Xerox_Printer_6125N`, `SRV WINNIE.local.:8631`.
- **Windows 11 caveat**: Settings → "Add device" also sends WSD probes
  (239.255.255.250:3702, answered by other LAN devices, not by us — we are IPP-only).
  If the queue gets a WSD port: disable Windows auto-setup, delete queue + WSD
  ports, re-add via the manual IPP URL (see dist-windows/README.txt).

### WSD research (2026-11-02, repos in /home/rick/src/wsd-research)

Cloned b1tninja/fakeprint (Rust fake WSD+IPP printer) and roncapat/WSD-python
(PyWSD, client-only lib). fakeprint's WSD module alone: **21k LOC Rust, 60 SOAP
operations** (discovery, metadata transfer, full WSD Print incl. MTOM).
PyWSD: 3k LOC but client-side only. For us in C: discovery+metadata only
~1-1.5k LOC (would fix the Serial Number display); full WSD Print +1.5-2k LOC
(2-4 weeks). Recommendation: stay IPP-only + manual IPP-URL add; do NOT
advertise WSD without print ops (the queue would pick WSD and fail).

### SNMP→IPP status bridge — DONE

`native/snmp_poll.c` (own minimal SNMPv1 BER client, no net-snmp dep) polls the
Printer-MIB over UDP/161 (community "public"): 1 GET sysDescr (online check) +
1 GET hrDeviceStatus + short GETNEXT count walk + chunked multi-OID GETs
(batch 10 supplies / 4 alerts — the printer returns tooBig on larger batches).
Maps prtMarkerSuppliesTable → IPP marker-levels/names/types/colors and
prtAlertTable → printer-state-reasons (text-based matching: "paper" without
"able to print" → media-empty + queue paused via papplPrinterPause;
"Toner" → toner-low, etc.). Verified via ipptool:
`marker-levels=-1,-1,-1,-1,80,100` (CMYK unknown for non-Xerox toners,
Imaging Unit 80%, Fuser 100%), `printer-state=stopped` +
`reasons=media-empty,toner-low,paused` with the tray open; `idle` when resolved.

### TODO

- **Windows build — DONE** (mingw cross-compile + NuGet prebuilt .lib files,
  no VS2022 needed): `scripts/build-windows-mingw.sh` →
  `build-mingw/phaser6125-pappl.exe` (PE32+ x86-64, ~665 KB, DLL deps:
  KERNEL32, msvcrt, winspool, libcups2.dll, libpappl1.dll). Verified on real
  Windows 11 (native process, TCP 8631, Create-Printer, Print-Job completed)
  and under wine64. `start_gs()` _WIN32 path: CreateProcess + gswin64c.exe.
  Packaging: `scripts/package-windows.sh` → `Phaser6125N-PAPPL-win64.zip`
  (~25 MB: exe + DLLs + bundled Ghostscript 10.08 in `gs/` + .bat entrypoints
  + README.txt). Alternative: `vcnet/` MSVC project for those with VS2022.
- **Portable mode (no admin needed)**: port 8631 is unprivileged (>1024), spool
  in `%LocalAppData%`. `dist-windows/run-portable.bat` runs straight from the
  unzipped zip.
- **IPP printer-device-id with MFG/MDL/SN**: Windows Device Information shows
  Manufacturer/Model from `printer-device-id`; Serial Number only comes from WSD
  metadata (shows "Unavailable" for pure-IPP printers — known Windows limitation).
  SN=LAL035151 (read from the printer via SNMP prtGeneralSerialNumber) is in the
  device-id for clients that parse it.
- **Linux packaging (optional)**: `pappl-retrofit` (legacy-printer-app.c) can
  wrap our PPD + CUPS filter out-of-the-box for Snap packaging (Linux only).

## 13. Removing the vendor v3 driver (Windows 11 24H2)

Full procedure + debugging guide (six hiding places incl. the PCC cab that the
spooler re-imports, pnputil semantics, setupapi.dev.log as the golden source):
see **[docs/remove-vendor-driver-windows.md](docs/remove-vendor-driver-windows.md)**.
Case study: Xerox Phaser 6125N (`xrhwszi.inf` → `oem140.inf`), incl. raw-USB
printing after cleanup via shared queue `\\\\HOST\\PhaserRaw` (see also §12
usbprint: scheme).
