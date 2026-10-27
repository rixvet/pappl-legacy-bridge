# Xerox Phaser 6125N — reverse engineering & CUPS revival

Source page: https://rickvanderzwet.nl/notes/XeroxPhaser6125N/
Printer: Xerox Phaser 6125N @ 192.168.178.10 (web title confirms: "Phaser 6125N - Phaser 6125N-A717DF")

## Bestanden (in `downloads/`)

| Bestand | Inhoud |
|---|---|
| `cups-xerox-phaser-6125n-1.0.0.deb` | Origineel CUPS-driverpakket (2012) van Rick |
| `cups-xerox-phaser-6125n-1.0.0.tar.gz` | Zelfde pakket als tarbal (voor Fedora/SuSE) |
| `dism_driver_export_win11_amd64_DPC525A_20260115.7z` | Windows 11 driver-export (DISM) van Fuji-Xerox DPC525A |

## Pakket-inhoud (deb == tarball)

```
usr/share/cups/model/Xerox/en/Phaser_6125N.ppd   (62 kB, Fuji Xerox DocuPrint C525 A-AP PPD, aangepast)
usr/share/cups/Xerox/dlut/Phaser_6125N.dlut      (265 kB, "DLUT" color lookup table)
usr/lib/cups/filter/FXM_PF    (17 kB, 32-bit i386 ELF, fxprefilter)
usr/lib/cups/filter/FXM_MF    (15 kB, 32-bit i386 ELF, fxmainfilter)
usr/lib/cups/filter/FXM_PS2PM (753 B,  shell script: Ghostscript -> ppmraw)
usr/lib/cups/filter/FXM_PM2FXR (19 kB, 32-bit ELF, PPM -> FXRaster)
usr/lib/cups/filter/FXM_SBP    (43 kB, 32-bit ELF)
usr/lib/cups/filter/FXM_PR     (22 kB, 32-bit ELF)
usr/lib/cups/filter/FXM_CC     (52 kB, 32-bit ELF, color correction, gebruikt .dlut)
usr/lib/cups/filter/FXM_ALC    (221 kB, 32-bit ELF, auto color / largest)
usr/lib/cups/filter/FXM_HBPL   (58 kB, 32-bit ELF, FXRaster -> HBPL + PJL wrapper)
```

Alle ELF's: `ELF 32-bit LSB executable, Intel 80386`, dynamic, `NEEDED: libcups.so.2, libc.so.6`,
met debug_info, niet gestript. Gebouwd op SUSE met gcc 3.3.4 / glibc 2.3 (2004-2012).

## PPD-kern (Phaser_6125N.ppd)

```
*cupsFilter: "application/vnd.cups-postscript 0 /usr/lib/cups/filter/FXM_PF"
*FXMainFilter: "/usr/lib/cups/filter/FXM_MF"
*FXFilterDir: "/usr/lib/cups/filter"
*FXFilterChain: "FXM_PS2PM, FXM_PM2FXR, FXM_SBP, FXM_PR, FXM_CC, FXM_ALC, FXM_HBPL"
*FXDlutFilePath: "/usr/share/cups/Xerox/dlut/Phaser_6125.dlut"   <-- BUG: bestand heet Phaser_6125N.dlut
```

Defaults: `ColorSpace CMYK`, `Resolution 600dpi`, `PageSize A4`, `FXColorMode Color`,
`InstalledMemory 64Meg`, `Option1 1Tray-S` (250-sheet feeder).

## Werking van de filters (uit strings/symbols)

- **FXM_PF** (`fxprefilter`): leest PPD, patcht de binnenkomende PostScript
  (voegt `%%BeginFeature:` blokken toe vóór `%%Page:`/`%%EndPageSetup`, met
  app-specifieke workarounds voor OpenOffice.org/StarSuite), schrijft tempfile
  (cupsTempFd) en exec't `FXMainFilter` (FXM_MF). Log: `/tmp/bjexec.log`.
- **FXM_MF** (`fxmainfilter`): leest `FXFilterChain` + `FXFilterDir` uit de PPD
  (libcups `ppdFindMarkedChoice`), fork't elke filter en pipe't ze aan elkaar:
  `FXM_PS2PM | FXM_PM2FXR | FXM_SBP | FXM_PR | FXM_CC | FXM_ALC | FXM_HBPL`.
- **FXM_PS2PM**: shell script, draait `gs -sDEVICE=ppmraw` (CUPS_FONTPATH).
- **FXM_HBPL** (`fxrastertohbpl`): leest FXRaster, schrijft PJL+HBPL:
  `@PJL SET ...`, `@PJL JOB MODE=PRINTER`, `@PJL ENTER LANGUAGE=HBPL`, RASTER data,
  `@PJL EOJ`, UEL. Bronbestanden: `fxrastertohbpl.c hbploption.c hbplcommand.c xpjl.c fxraster.c`.

## Printer-netwerk (getest vanuit deze omgeving)

- 192.168.178.10:9100 (raw JetDirect) — OPEN
- 192.168.178.10:515  (LPD)          — OPEN
- 192.168.178.10:80   (web UI)       — OPEN, titel "Phaser 6125N"
- 631 (IPP), 161 (SNMP) — dicht

## Windows driver export (7z)

27 bestanden, o.a. `fxhl2z.cdd/.cfg/.cnt/.hlp/.xrs`, `fxhl2zi.inf/.PNF`,
`fxhl2zil.dll`, `fxhl2zim.dll` (render), `fxhl2zir.xrs` (5 MB resource),
`fxhl2ziu.dll`, `fxhl2zp1.ddd` + `fxhl2zp1.icm` (kleurprofiel),
`fxhzdcis.dll`, `fxhzdczd.dll`, `fxlzjnie.dll`, `fxlznie1.dll`,
`fxz525a3.dll`, `fxzadcmn.dll`, `fxzarl32.dll`, `fxzddmif.dll`, `fxzdhb32.dll`,
`fxzdmifk.dll`, `fxzpmc3.dll`, `fxhl2dcr.dll`, `fxhl2jdm.dll`.
Dit is de Windows-equivalent van dezelfde HBPL-pipeline (fxz* = zjs/HBPL, fxhl2* = UI).

## Bekende problemen / fix-lijst

1. **dlut-pad bug**: PPD refereert `Phaser_6125.dlut`, pakket levert `Phaser_6125N.dlut`.
2. **32-bit**: filters hebben `/lib/ld-linux.so.2` + 32-bit `libcups.so.2` nodig
   (`libc6-i386`, `libcups2:i386`).
3. **Filter-permissies**: CUPS eist root:root 0755 op `/usr/lib/cups/filter`
   (zie troubleshooting op de pagina: "insecure permissions (040775)").
4. FXM_PS2PM gebruikt hardcoded `bindir=/usr/bin` voor `gs` — check gs-versie
   (oud script, `-dDEBUG` flag isены no-op in moderne gs maar onschadelijk).
5. Modern CUPS (2.4) converteert PDF -> `application/vnd.cups-postscript`
   (pdftops uit `cups-filters`) voordat FXM_PF de job krijgt. Alleen PS-input
   is gedeclareerd in de PPD.

## Plan

1. i386 multilib + CUPS installeren (Ubuntu 22.04).
2. Filters/PPD/dlut naar systeemlocatie kopiëren, dlut-pad fixen, PPD valideren.
3. Filter-chain handmatig testen (PS -> HBPL) zonder CUPS.
4. Printer toevoegen via `lpadmin` (socket://192.168.178.10:9100), testpagina.
5. Fouten itereren via `/var/log/cups/error_log` (LogLevel debug2).
