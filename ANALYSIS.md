# Xerox Phaser 6125N — reverse engineering & CUPS revival

> **Update:** the codec is now fully reverse-engineered and there is a working
> **native 64-bit CUPS driver** (`native/foo2sq21hbpl.c`) that keeps the
> printer printing without 32-bit filters. See **`RE-NOTES.md`** for the
> sq21/HBPL format, verification and installation.

Source page: https://rickvanderzwet.nl/notes/XeroxPhaser6125N/
Printer: Xerox Phaser 6125N @ 192.168.178.10 (web title confirms: "Phaser 6125N - Phaser 6125N-A717DF")

## Files (in `downloads/`)

| File | Contents |
|---|---|
| `cups-xerox-phaser-6125n-1.0.0.deb` | Original CUPS driver package (2012) by Rick |
| `cups-xerox-phaser-6125n-1.0.0.tar.gz` | Same package as tarball (for Fedora/SuSE) |
| `dism_driver_export_win11_amd64_DPC525A_20260115.7z` | Windows 11 driver export (DISM) of Fuji-Xerox DPC525A |

## Package contents (deb == tarball)

```
usr/share/cups/model/Xerox/en/Phaser_6125N.ppd   (62 kB, Fuji Xerox DocuPrint C525 A-AP PPD, modified)
usr/share/cups/Xerox/dlut/Phaser_6125N.dlut      (265 kB, "DLUT" color lookup table)
usr/lib/cups/filter/FXM_PF    (17 kB, 32-bit i386 ELF, fxprefilter)
usr/lib/cups/filter/FXM_MF    (15 kB, 32-bit i386 ELF, fxmainfilter)
usr/lib/cups/filter/FXM_PS2PM (753 B,  shell script: Ghostscript -> ppmraw)
usr/lib/cups/filter/FXM_PM2FXR (19 kB, 32-bit ELF, PPM -> FXRaster)
usr/lib/cups/filter/FXM_SBP    (43 kB, 32-bit ELF)
usr/lib/cups/filter/FXM_PR     (22 kB, 32-bit ELF)
usr/lib/cups/filter/FXM_CC     (52 kB, 32-bit ELF, color correction, uses .dlut)
usr/lib/cups/filter/FXM_ALC    (221 kB, 32-bit ELF, auto color / largest)
usr/lib/cups/filter/FXM_HBPL   (58 kB, 32-bit ELF, FXRaster -> HBPL + PJL wrapper)
```

All ELFs: `ELF 32-bit LSB executable, Intel 80386`, dynamic, `NEEDED: libcups.so.2, libc.so.6`,
with debug_info, not stripped. Built on SUSE with gcc 3.3.4 / glibc 2.3 (2004-2012).

## PPD core (Phaser_6125N.ppd)

```
*cupsFilter: "application/vnd.cups-postscript 0 /usr/lib/cups/filter/FXM_PF"
*FXMainFilter: "/usr/lib/cups/filter/FXM_MF"
*FXFilterDir: "/usr/lib/cups/filter"
*FXFilterChain: "FXM_PS2PM, FXM_PM2FXR, FXM_SBP, FXM_PR, FXM_CC, FXM_ALC, FXM_HBPL"
*FXDlutFilePath: "/usr/share/cups/Xerox/dlut/Phaser_6125.dlut"   <-- BUG: file is named Phaser_6125N.dlut
```

Defaults: `ColorSpace CMYK`, `Resolution 600dpi`, `PageSize A4`, `FXColorMode Color`,
`InstalledMemory 64Meg`, `Option1 1Tray-S` (250-sheet feeder).

## How the filters work (from strings/symbols)

- **FXM_PF** (`fxprefilter`): reads the PPD, patches the incoming PostScript
  (adds `%%BeginFeature:` blocks before `%%Page:`/`%%EndPageSetup`, with
  app-specific workarounds for OpenOffice.org/StarSuite), writes a tempfile
  (cupsTempFd) and exec's `FXMainFilter` (FXM_MF). Log: `/tmp/bjexec.log`.
- **FXM_MF** (`fxmainfilter`): reads `FXFilterChain` + `FXFilterDir` from the PPD
  (libcups `ppdFindMarkedChoice`), forks each filter and pipes them together:
  `FXM_PS2PM | FXM_PM2FXR | FXM_SBP | FXM_PR | FXM_CC | FXM_ALC | FXM_HBPL`.
- **FXM_PS2PM**: shell script, runs `gs -sDEVICE=ppmraw` (CUPS_FONTPATH).
- **FXM_HBPL** (`fxrastertohbpl`): reads FXRaster, writes PJL+HBPL:
  `@PJL SET ...`, `@PJL JOB MODE=PRINTER`, `@PJL ENTER LANGUAGE=HBPL`, RASTER data,
  `@PJL EOJ`, UEL. Source files: `fxrastertohbpl.c hbploption.c hbplcommand.c xpjl.c fxraster.c`.

## Printer network (tested from this environment)

- 192.168.178.10:9100 (raw JetDirect) — OPEN
- 192.168.178.10:515  (LPD)          — OPEN
- 192.168.178.10:80   (web UI)       — OPEN, title "Phaser 6125N"
- 631 (IPP), 161 (SNMP) — closed

## Windows driver export (7z)

27 files, incl. `fxhl2z.cdd/.cfg/.cnt/.hlp/.xrs`, `fxhl2zi.inf/.PNF`,
`fxhl2zil.dll`, `fxhl2zim.dll` (render), `fxhl2zir.xrs` (5 MB resource),
`fxhl2ziu.dll`, `fxhl2zp1.ddd` + `fxhl2zp1.icm` (color profile),
`fxhzdcis.dll`, `fxhzdczd.dll`, `fxlzjnie.dll`, `fxlznie1.dll`,
`fxz525a3.dll`, `fxzadcmn.dll`, `fxzarl32.dll`, `fxzddmif.dll`, `fxzdhb32.dll`,
`fxzdmifk.dll`, `fxzpmc3.dll`, `fxhl2dcr.dll`, `fxhl2jdm.dll`.
This is the Windows equivalent of the same HBPL pipeline (fxz* = zjs/HBPL, fxhl2* = UI).

## Known issues / fix list

1. **dlut path bug**: PPD references `Phaser_6125.dlut`, the package ships `Phaser_6125N.dlut`.
2. **32-bit**: filters need `/lib/ld-linux.so.2` + 32-bit `libcups.so.2`
   (`libc6-i386`, `libcups2:i386`).
3. **Filter permissions**: CUPS requires root:root 0755 on `/usr/lib/cups/filter`
   (see troubleshooting on the page: "insecure permissions (040775)").
4. FXM_PS2PM uses hardcoded `bindir=/usr/bin` for `gs` — check gs version
   (old script; the `-dDEBUG` flag is a no-op in modern gs but harmless).
5. Modern CUPS (2.4) converts PDF → `application/vnd.cups-postscript`
   (pdftops from `cups-filters`) before FXM_PF gets the job. Only PS input
   is declared in the PPD.

## Plan

1. install i386 multilib + CUPS (Ubuntu 22.04). -> `scripts/setup-cups.sh`
2. copy filters/PPD/dlut to system locations (dlut path fix is in `driver/`). -> `scripts/install-driver.sh`
3. add printer via `lpadmin` (socket://192.168.178.10:9100). -> `scripts/setup-printer.sh`
4. print test page via CUPS. -> `scripts/test-print.sh`

## Reverse-engineering results (2026-10-27)

### The chain tested without CUPS (user-level 32-bit sysroot)

The filters run fine on Ubuntu 22.04 via a user-level sysroot
(`libc6:i386` + `libcups2:i386` debs downloaded via a custom apt-config,
unpacked in `/home/rick/sysroot-i386`, started via the sysroot loader).
See `tests/run-chain-local.sh` (reproducible, no root needed).

### What FXM_PF really does

- Reads only the **first 4096 bytes** of the PPD (enough for `FXMainFilter`).
- Writes the incoming PS **unchanged** to a tempfile
  (cupsTempFd) and exec's `FXM_MF`. The `%%BeginFeature:`/`%%Creator:` strings are
  app-specific workarounds (OpenOffice.org/StarSuite), not general
  option injection.
- **Conclusion:** the PageSize/HWResolution `setpagedevice` injection normally happens
  in the CUPS front-end (`pstops`/`pdftops` emit the PPD feature code
  via `ppdEmit`). Without CUPS, gs renders at its default (A4 @ 72dpi).

### Fix in `driver/filter/FXM_PS2PM`

- Removed `-dDEBUG` (modern gs logs huge amounts of noise to stderr).
- Resolution is now explicitly derived from the `FXOutputMode=WxH` PPD option
  (`-r600x600`, fallback = PPD default 600x600). The driver now always renders
  at the right resolution, even if the front-end injects nothing.

### Fix in `driver/Phaser_6125N.ppd`

- `*FXDlutFilePath` pointed to `Phaser_6125.dlut`; the package ships
  `Phaser_6125N.dlut`. Path corrected (only change vs. original).

### HBPL format (decoded from the output)

```
<ESC>%-12345X @PJL COMMENT/SET ... @PJL ENTER LANGUAGE=HBPL\n
binary HBPL attribute commands (incl. 0x9a 0xc4 = raster width/height as 2x uint32 LE)
compressed raster data
SDB <ESC>%-12345X @PJL EOJ\n
```

Raster dimensions in the output:

| Setting | Raster (px) | Note |
|---|---|---|
| A4 @ 72dpi (without injection) | 600 x 842 | gs default, wrong |
| A4 @ 600dpi | **4776 x 6829** | = ImageableArea A4 `11.28 11.28 583.72 830.72` @ 600dpi |
| A4 @ 300dpi | 2392 x 3414 | = imageable area @ 300dpi |

`FXM_PM2FXR` crops the PPM to the imageable area (imports `ppdPageSize`).

### Proof: the real printer prints the test page

Test pages generated with the chain and sent straight to
`192.168.178.10:9100`:

- Job log (`http://192.168.178.10/job/logsys.htm`):
  `Testpage / rick / WINNIE / Completed / 1 / Port 9100 / 2026/10/27 22:14:19`
  `Testpage600 / rick / WINNIE / Completed / 1 / Port 9100 / 2026/10/27 22:18:32`
- Page Count on the status page: **8659 -> 8660** (+1 page printed)
- Evidence in `tests/`: `testpage-600dpi.hbpl`, `testpage-72dpi.hbpl`

### Remaining work (CUPS integration)

The chain + printer are proven working. Remaining: install CUPS, copy driver to
`/usr/lib/cups/filter`, create queue. Scripts are in `scripts/`.
