# Xerox Phaser 6125N — reverse engineering & CUPS revival

> **Update:** de codec is inmiddels volledig gereverse-engineerd en er is een
> werkende **native 64-bit CUPS driver** (`native/foo2sq21hbpl.c`) die de
> printer printend houdt zonder 32-bit filters. Zie **`RE-NOTES.md`** voor het
> sq21/HBPL formaat, de verificatie en de installatie.

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

1. i386 multilib + CUPS installeren (Ubuntu 22.04). -> `scripts/setup-cups.sh`
2. Filters/PPD/dlut naar systeemlocatie kopiëren (dlut-pad fix zit in `driver/`). -> `scripts/install-driver.sh`
3. Printer toevoegen via `lpadmin` (socket://192.168.178.10:9100). -> `scripts/setup-printer.sh`
4. Testpagina printen via CUPS. -> `scripts/test-print.sh`

## Reverse-engineering resultaten (2026-10-27)

### De chain getest zonder CUPS (user-level 32-bit sysroot)

De filters draaien prima op Ubuntu 22.04 via een user-level sysroot
(`libc6:i386` + `libcups2:i386` debs gedownload via een custom apt-config,
uitgepakt in `/home/rick/sysroot-i386`, gestart via de sysroot-loader).
Zie `tests/run-chain-local.sh` (reproduceerbaar, geen root nodig).

### Wat FXM_PF écht doet

- Leest alleen de **eerste 4096 bytes** van de PPD ( Genoeg voor `FXMainFilter`).
- Schrijft de binnenkomende PS **ongewijzigd** door naar een tempfile
  (cupsTempFd) en exec't `FXM_MF`. De `%%BeginFeature:`/`%%Creator:`-strings zijn
  app-specifieke workarounds (OpenOffice.org/StarSuite), geen algemene
  optie-injectie.
- **Conclusie:** de PageSize/HWResolution `setpagedevice`-injectie gebeurt
  normaal door de CUPS front-end (`pstops`/`pdftops` emitten de PPD-featurecode
  via `ppdEmit`). Zonder CUPS rendert gs op zijn default (A4 @ 72dpi).

### Fix in `driver/filter/FXM_PS2PM`

- `-dDEBUG` verwijderd (moderne gs logt enorme hoeveelheden ruis naar stderr).
- Resolutie wordt nu expliciet afgeleid van de `FXOutputMode=WxH` PPD-optie
  (`-r600x600`, fallback = PPD default 600x600). De driver rendert nu altijd
  op de juiste resolutie, óók als de front-end niets injecteert.

### Fix in `driver/Phaser_6125N.ppd`

- `*FXDlutFilePath` wees naar `Phaser_6125.dlut`; het pakket levert
  `Phaser_6125N.dlut`. Pad gecorrigeerd (enige wijziging t.o.v. origineel).

### HBPL-formaat (gedecodeerd uit de output)

```
<ESC>%-12345X @PJL COMMENT/SET ... @PJL ENTER LANGUAGE=HBPL\n
binaire HBPL attribute-commando's (o.a. 0x9a 0xc4 = raster width/height als 2x uint32 LE)
gecomprimeerde rasterdata
SDB <ESC>%-12345X @PJL EOJ\n
```

Rasterafmetingen in de output:

| Instelling | Raster (px) | Opmerking |
|---|---|---|
| A4 @ 72dpi (zonder injectie) | 600 x 842 | gs default, fout |
| A4 @ 600dpi | **4776 x 6829** | = ImageableArea A4 `11.28 11.28 583.72 830.72` @ 600dpi |
| A4 @ 300dpi | 2392 x 3414 | = imageable area @ 300dpi |

`FXM_PM2FXR` knipt de PPM bij naar het imageable area (importeert `ppdPageSize`).

### Bewijs: de echte printer print de testpagina

Testpagina's gegenereerd met de chain en rechtstreeks naar
`192.168.178.10:9100` gestuurd:

- Job log (`http://192.168.178.10/job/logsys.htm`):
  `Testpage / rick / WINNIE / Completed / 1 / Port 9100 / 2026/10/27 22:14:19`
  `Testpage600 / rick / WINNIE / Completed / 1 / Port 9100 / 2026/10/27 22:18:32`
- Page Count op de statuspagina: **8659 -> 8660** (+1 pagina afgedrukt)
- Bewijslast in `tests/`: `testpage-600dpi.hbpl`, `testpage-72dpi.hbpl`

### Resterend werk (CUPS-integratie)

De chain + printer zijn bewezen werkend. Restant: CUPS installeren, driver naar
`/usr/lib/cups/filter` kopiëren, queue aanmaken. Scripts staan in `scripts/`.
