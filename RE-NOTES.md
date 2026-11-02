# RE-NOTES — sq21/HBPL codec & native 64-bit driver

Reverse-engineering resultaten voor de Xerox Phaser 6125N (Fuji Xerox HBPL,
@DRDM=RASTER). Zie `ANALYSIS.md` voor de analyse van de originele 32-bit
filter-chain. Alles hieronder is geverifieerd tegen de echte printer
(192.168.178.10, raw JetDirect poort 9100).

## 1. Het sq21 raster-codec (identiek aan "XRC" uit foo2zjs)

Single interleaved bitstream, MSB-first.

- **Tokens** `HID[hsel][k]`, canonieke Huffman, 2 rijen:
  - `k=0..4`: run van `run` pixels gekopieerd uit predictor `order[k]` (move-to-front)
  - `k=5`: diff — 4 byte-diffs (HPE0) opgeteld bij het linkerpixel
  - `k=6`: switch hsel-rij (code `11111` + één extra 0 bit)
- **Runlengtes** 1..17057: HRN0 korte codes / `111` + 3-bit group + extra bits.
  17057 = 674 + 14 bits (maximale codeerbare run; encoder chunkt langere runs).
- **Byte diffs**: HPE0 codebook — identiek aan Dave Coffin's `put_diff()`/`get_diff()`
  (bewezen: tabellen in Fuji's libsq21 == foo2hbpl1's XRC; zie foo2zjs PR #24 /
  foomatic-db PR #59 — zelfde codec, andere container).
- **Predictors** (semantisch, per pixel): 0=left 1=up 2=up-left 3=up-right 4=up+2right.
  Buiten het beeld lezen als `ff ff ff ff` (sq21 pixbuf marges; rechterkant up-right /
  up+2right raken de 0xff sentinel slot).
- **Bit bookkeeping**: token index = `bdir - bit`; `bit = (run < 17057)` na runs,
  `bit = 0` na een diff en na elke 17057-chunk. De MTF rotor (`order[]`) schuift
  mee als `bdir != 0`. Encoder wisselt van hsel-rij bij `|stat| > 8` (emits HID[6] + one 0 bit).
- **End marker**: HID[6] + `111` + 1 bit padding (byte-aligned).
- **Image layout**: (H+1) rows × (W+2) slots × 4 byte `[ff,R,G,B]`; row 0 = virtueel
  all-ff; slot 0 = linker marge ff; slot W+1 = rechter sentinel ff.

## 2. HBPL container (deze printer: @DRDM=RASTER, single interleaved stream)

- **PJL header**: vaste template, byte-exact uit capture `tests/testpage-600dpi.hbpl`
  (1079 bytes; alleen DATE/TIME/DNAME/user/host/copies/resolution variëren).
  Bevat o.a. `@PJL SET RENDERMODE=COLOR`, `@PJL SET JOBATTR="@DRDM=RASTER"`,
  `@PJL ENTER LANGUAGE=HBPL`.
- **A-record** 12 bytes (eenmalig, direct na ENTER LANGUAGE).
- **Per pagina**:
  - C-record 78 bytes: marges u16le @24-27, pageno u32le @33-36 (1-based),
    w u32le @39-42, h u32le @43-46, w/h nogmaals @70-77
  - QR-record 12 bytes: total u32le @8-11 = 24 + payload lengte
  - body 24 bytes (fixed): `18 00 00 00 00 01 00 00 10 32 04 00 a1 42 00 00 00 00 ff 00 00 00 00 00`
  - payload (sq21 stream)
  - `"SD"` 2 bytes
- De head (C+QR+body) is een vaste template met 3 patch points:
  `9a c4` w/h @51-58, `a2 c4` w/h @82-89, QR total @98-101.
- **Job end**: `'B'` + `ESC "%-12345X"` + `"@PJL EOJ\n"` (19 bytes).
- **Copies**: `@PJL SET COPIES=n`; pagina's worden NIET gerepliceerd in de stream.
- NB: foo2hbpl1's XRC framing (@DRDM=XRC, 5-stream layout) wordt door deze
  printer **geweigerd** — deze wil RASTER + single stream.

## 3. FXR formaat (tussenstages van de originele chain)

- 44-byte header = 11× uint32 LE `[w,h,bpp,rowbytes,datasize,resX,resY,0,0,f9,f10]`
- `f9`: 1 = raw RGB 24bpp, 2 = sq21 (f10=3)
- FXM_ALC output = FXR header + sq21 payload (zonder 16-byte magic header);
  FXM_HBPL stripped de FXR header (w/h zitten in de HBPL 9a/a2 records).
- sq21 magic = `0xc2c0cfba` (bytes `ba cf c0 c2`).

## 4. PM2FXR crop-reg regel (afgeleid uit de PPD zelf)

- PPD `*ImageableArea <PageSize>/...: "left bottom right top"` in pt; `r = dpi/72`
- `x0=round(left·r)`, `x1=round(right·r)`, `y0=in_h−round(top·r)`, `y1=in_h−round(bottom·r)`
- pass-through (geen crop) als de input binnen het imageable area past
- A4@600: crop @(94,94) → 4770×6829; Letter@600: @(94,94) → 4912×6412; A4@300: @(47,47) → 2385×3414
- gs page sizes: A4@600 → 4958×7017 (gs rond 842×600/72=7016.67 → 7017) voor alle
  aanroepvarianten; alleen expliciete `-g4958x7016` geeft 7016
- FXM_ALC pad de breedte naar een veelvoud van 8 (4770→4776); pad pixels = `ff ff ff ff`
- 6125N imageable marges: 11.28pt (~4mm) rondom

## 5. Windows driver (DPC525A DISM export, 64-bit)

- `fxhl2zim.dll` = "Fuji Xerox HBPL Printer Driver" (64-bit PJL/HBPL output + image processing)
- `fxhzdczd.dll` (1.5 MB) bevat de sq21 magic `ba cf c0 c2`
- `fxzarl32.dll` = CentreWare Address Resolution Library (NIET de codec)
- `fxhl2z.xrs` = resources (page sizes/marges), `fxhl2z.cdd` = color data, `fxhl2zp1.icm` = ICC
- Conclusie: bruikbaar als 64-bit reference, niet blockerend (crop offsets kwamen uit de PPD).

## 6. Native driver (`native/foo2sq21hbpl.c` + `native/hbpl_core.c`)

CUPS filter, libc only, native 64-bit (geen multilib). Interace zoals CUPS dat
van filters verwacht: `foo2sq21hbpl job-id user title copies options [file]`,
omgeving `PPD`.

Pipeline:
1. parse options (`PageSize`/`PageRegion`, `FXOutputMode`→dpi (300/600),
   `FXColorMode` Black→grijs, rest → kleur, `FXDocumentType`→FXDLUT block code)
2. PPD lookup (`PaperDimension`, `ImageableArea` voor de page size)
3. fork/exec `gs` (`-r{dpi} -dDEVICEWIDTHPOINTS= -dDEVICEHEIGHTPOINTS=` uit de PPD,
   `-sDEVICE=ppmraw -sOUTPUTFILE=%stdout`)
4. per pagina: P6 parsen → crop (PM2FXR regel) → **per pixel kleurcalibratie**
   (FXM_CC-equivalent, §9: kleur = 3Dlut(CST(rgb)), grijs = NTSC(CST(rgb)))
   → expand naar 4-byte `[ff,R,G,B]` → breedte padden naar veelvoud van 8
   (`ffffffff`) → sq21-encode → C/QR/body/payload/"SD" per pagina
5. PJL header + A-record eenmalig; job trailer aan het eind

Kleurcalibratie (standaard AAN; algoritme in §9, verificatie in §7):
- CST tabellen: `native/cst_tables.h` (gegenereerd, in git —
  `re/gen_cst_tables.py` uit `re/extract-tables.py`)
- 3D LUT: `/usr/share/phaser6125/Phaser_6125N.dlut` (geïnstalleerd door
  `scripts/install-native-driver.sh`)
- env `FOO2SQ_CALIBRATE=0` schakelt calibratie uit (raw, of grijs-gemiddeld)
- env `FOO2SQ_DLUT=<pad>` override voor het .dlut pad; zonder bruikbaar .dlut
  valt de filter terug op CST-only (zoals FXM_CC zelf)
- grijs gebruikt geen LUT (alleen CST + NTSC)

De encoder is **exact lossless** (de officieuze FXM_ALC is near-lossless, ±1-2 per
pixel — beide zijn geldige streams; onze payload is ~0.8% groter).

### Core split (hbpl_core.c / hbpl_core.h)

Sinds de PAPPL-integratie (zie §12) is de encoder opgesplitst:
native/hbpl_core.c bevat de complete pixel pipeline (sq21 codec, HBPL
container, FXM_CC calibratie, PPM reader, PJL header, per-page processor,
trailer) als herbruikbare library — pure C99 + stdio, geen POSIX.  De CUPS
filter (native/foo2sq21hbpl.c) is nu alleen een dunne main(): CUPS argv
parsen, gs forken, en de PPM stream door hbpl_page() leiden.  Dezelfde core
wordt hergebruikt door:
- win/hbblenc.c — standalone Windows tool (PPM stdin → HBPL stdout)
- pappl/phaser6125-pappl.c — PAPPL printer application (MIME filter:
  PDF → gs → PPM → hbpl_page() → device)

Beide consumers zijn byte-identical geverifieerd (binary-identity regression
tegen de geïnstalleerde CUPS filter).

Build (schoon onder gcc -Wall -Wextra én clang -Weverything -Werror; getest
op clang 14 t/m 21):

```
gcc   -O2 -Wall -Wextra -o foo2sq21hbpl native/foo2sq21hbpl.c native/hbpl_core.c -lm
clang -Weverything -Werror -Wno-unsafe-buffer-usage -Wno-unknown-warning-option \
      -O2 -o foo2sq21hbpl native/foo2sq21hbpl.c native/hbpl_core.c -lm
```

`-Wno-unsafe-buffer-usage`: clang's experimentele C++ bounds-safety lint
(zit vanaf clang 16 in `-Weverything`); in C niet op te lossen zonder
`_Ptr` annotaties die gcc niet ondersteunt. `-Wno-unknown-warning-option`
houdt de command line compatibel met clang ouder dan 16 (die kent de
eerste vlag niet).

## 7. Verificatie

- `re/sq21dec.py` — pure-Python sq21 decoder (cross-checked tegen Dave Coffin's
  `hbpldecode.c`); CLI: `sq21dec.py <file> <W> <H> [offset] [out.raw]`
- `re/sq21dec.c` — C port (~60x sneller: 12.5s → 0.21s per A4@600 pagina),
  byte-identical output geverifieerd op alle captures; build als CLI én als
  `libsq21dec.so` (ctypes). Bevat ook `sq21_expand` (crop+expand+pad in C).
- `re/verify-native.py <input.ps|pdf> "<options>"` — end-to-end: structuur-asserts
  (PJL/A/C/QR/body/SD/trailer) + per-pagina lossless round-trip vs een
  standalone gs render. Gebruikt de C decoder; pagina's parallel (ctypes released
  de GIL). A4@600 kleurpagina verifieert in ~1.5s.
- `re/verify_model.c` — byte-exact model van de FXM_CC color stage (CST +
  tetrahedral 3D LUT); parameters uit live gdb dumps van de 32-bit binary
  (`re/dump-params.gdb`: `cst-param.bin`, `dlut-param.bin`, `xyz2srgbtrc.bin`).
  **Byte-exact** tegen ground truth: alle 256^3 kleuren (synth.fxr→synth-cc.fxr)
  en een echte pagina (s3-pr.fxr→s4-cc.fxr, 500.990 px).
- `re/calib.c` — oracle voor de driver: zelfde model, maar lezend uit het .dlut
  bestand (zoals de driver). Bouw als CLI (FXR→FXR, cross-check vs ground truth)
  én als `re/libcalib.so` (ctypes, voor verify-native.py). Byte-exact tegen
  ground truth: synth (16.777.216 px), s3→s4 (500.990 px), gray-test→gray-out.
- `re/extract-tables.py` + `re/gen_cst_tables.py` — halen de CST tabellen uit de
  FXM_CC ELF en genereren `native/cst_tables.h` (in git; niet handmatig editten).
- `re/FXM_CC-decompiled.c` — Ghidra decompilatie van de 32-bit FXM_CC (16
  functions; `main` toont de CST→3Dlut volgorde). Regenereren:
  `re/DecompileFxmCc.java` (Ghidra headless, Java script).
- Getest & EXACT (verify-native.py, A4@600 colorpage.pdf, ~32.6M px/pagina):
  raw (`FOO2SQ_CALIBRATE=0`), color gekalibreerd (`FXDocumentType` Normal én
  Photo — twee verschillende DLUT blocks), gray gekalibreerd
  (`FXColorMode=Black`), gray average, 1-page én 2-page (paginanummering +
  stream alignment). De gekalibreerde round-trip vergelijkt tegen de oracle
  (re/calib.c) — de driver is daarmee **byte-exact gelijk aan de originele
  FXM_CC transform**.
- Eerdere tests & EXACT: A4@300, Letter@600, direct PDF input.
- **Echte printer**: 1-page print (page count 8660→8661), 2-page print
  (8661→8663), colorpage met meahow (8663→8664); job log toont `Completed`
  met het juiste pagina-aantal. (Printer is gedeeld; teller staat nu op 8669 —
  print zuinig met testpagina's.)

## 8. Installatie (native, 64-bit)

```
sudo bash scripts/setup-cups-native.sh       # CUPS + ghostscript (geen i386!)
sudo bash scripts/install-native-driver.sh   # build+install filter, PPD, queue
bash scripts/test-print.sh                   # drukt tests/colorpage.pdf af
```

- filter → `/usr/lib/cups/filter/foo2sq21hbpl` (root:root 0755)
- PPD → `/usr/share/cups/model/Xerox/en/Phaser_6125N-native.ppd`
  (zelfde opties als de originele PPD; `*cupsFilter` wijst naar foo2sq21hbpl)
- .dlut → `/usr/share/phaser6125/Phaser_6125N.dlut` (kleurcalibratie, §9; de
  install script doet ook een `clang -Weverything -Werror` sanity build)
- queue `Phaser6125N` → `socket://192.168.178.10:9100`

### CUPS "Printer drivers are deprecated" waarschuwing

`lpadmin` print deze melding omdat PPD-gebaseerde drivers (PPD + filter in
`/usr/lib/cups/filter`) in een toekomstige CUPS (3.x, OpenPrinting) worden
afgeschaft ten gunste van driverless IPP. Voor deze setup is het loos alarm:

- Ubuntu 22.04 ship CUPS 2.4.1 **mét** PPD/driver support; 22.04 is ondersteund
  tot 2027 (en WSL2 kan daar nog langer op blijven).
- De Phaser 6125N is een raw printer (poort 9100/515/80, **geen IPP**) — zonder
  PPD+filter is er géén driverless/IPP-Everywhere mogelijk. PPD+filter is hier
  de énige werkbare route.
- Onze filter is standalone: printen kan altijd zonder CUPS via
  `native/foo2sq21hbpl 1 <user> <title> 1 "<options>" <file> | nc 192.168.178.10 9100`
  (de eerste testprints in deze sessie gingen precies zo).
- Wil je ooit toch van de PPD af: zie "Toekomst: CUPS 3.x / driverless
  (ippserver)" hieronder.

### Compatibiliteit (getest in WSL distro's)

| Ubuntu | CUPS | gcc | clang | ghostscript | status |
|--------|------|-----|-------|-------------|--------|
| 22.04 (deze WSL) | 2.4.1 | 11.4 | 14 | 9.55 | ✓ alles werkt (build, CUPS queue, print) |
| 24.04 | 2.4.7 | 13.3 | 18 | 10.02 | ✓ alles werkt (gcc + clang -Weverything -Werror build, verify raw/gekalibreerd color/grijs EXACT, CUPS queue; print overgeslagen) |
| 26.04 | 2.4.16 | 15.2 | 21 | 10.06 | ✓ alles werkt (gcc + clang -Weverything -Werror build, verify raw/gekalibreerd color/grijs EXACT, CUPS queue, **echte print** — teller 8669→8670, job Completed) |

Op 26.04 heeft de filter build `libc6-dev` nodig (standaard niet
aanwezig) — `scripts/test-compat-distro.sh` installeert het mee.

`scripts/test-compat-distro.sh` test de hele repo in een WSL distro
(22.04/24.04/26.04): repo uitpakken, filter bouwen (gcc + clang
`-Weverything -Werror`), verify-native draaien (raw + gekalibreerd),
queue aanmaken, print (of `SKIP_PRINT=1`).

### Zonder CUPS (fallback)

`scripts/print-direct.sh <bestand.pdf|ps> ["options"]` — draait de filter
direct en stuurt de HBPL stream naar `PRINTER_IP:9100` (raw JetDirect, `nc`).
Bouwt de filter zelf als die er nog niet is. Werkt op elke Linux met
gcc + ghostscript, zonder CUPS.

### Status ophalen (extra gimmick)

`scripts/printer-status.py` — read-only status, twee bronnen:
- **HTTP** (CentreWare webinterface): globale status, page count, papierladen,
  supplies, laatste job-log regels. (De pagina's eindigen in een NUL-byte; die
  wordt gestript.)
- **SNMP**: deze printer spreekt **alleen SNMPv1**, community `public`
  (`snmpwalk -v1 -c public …`); `prtMarkerSupplies` (RFC 1759) voor
  description/max/level.

Env: `PRINTER_IP` (default 192.168.178.10), `SNMP_COMMUNITY`, `SNMP_TIMEOUT`.
Bekende beperking: **toner-niveaus zijn niet afleesbaar** — de cartridges zijn
niet-origineel, de printer meldt `--` (HTTP) / level -2 (SNMP). Imaging Unit
en Fuser rapporteren wel percentages. (Gimmick/TODO: met originele toners
zouden de niveaus hier direct leesbaar zijn.)

### Toekomst: CUPS 3.x / driverless (ippserver)

CUPS 3.x schaft PPD+filter drivers af; de route is dan IPP Everywhere. Deze
printer heeft zelf geen IPP, maar onze filter is een standalone binary (die de
complete PJL/HBPL stream op stdout zet) — de migratie is klein:

1. `cups-ipp-utils` installeren (bevat `ippeveprinter`; in CUPS 3.x heet dit
   `ippserver`)
2. de command wrapper installeren als CUPS command filter
   (`scripts/ippeve-wrapper.sh` → `/usr/lib/cups/command/foo2sq21hbpl-ippeve`):
   leest het document van stdin, roept de filter aan, schrijft HBPL naar stdout
3. de printer als IPP service draaien — de output gaat naar de raw socket:
   ```
   ippeveprinter -m "Phaser 6125N" -M "Xerox" -l "Thuis" \
       -D socket://192.168.178.10:9100 \
       -f application/pdf,application/postscript \
       -s 10,10 \
       -c /usr/lib/cups/command/foo2sq21hbpl-ippeve \
       "Phaser6125N"
   ```
   (CUPS 3.x: `ippserver` met dezelfde opties.) Clients printen dan driverless
   via IPP Everywhere naar deze host.

Nu niet nodig: 22.04/24.04/26.04 draaien allemaal CUPS 2.4.x met PPD support,
en `scripts/print-direct.sh` is de CUPS-loze fallback. De wrapper is lokaal
getest: stdin → byte-identieke HBPL stream als de directe filter.

## 9. Bekende beperkingen / vervolgwerk

- **Kleurcalibratie (FXM_CC + .dlut) — GEREPLICEERD, byte-exact geverifieerd**
  (integratie in de driver: §6; verificatie: §7). Het gekraakte algoritme
  (Ghidra decompilatie `re/FXM_CC-decompiled.c` + parameters uit live gdb
  dumps van de 32-bit binary):

  **Pipeline** (uit `main`): `color: out = 3Dlut(CST(in))`,
  `gray: out = NTSC(CST(in))` — CST wordt eerst toegepast, dáárna de 3D LUT.

  **CST** (per pixel, 8-bit in → 8-bit uit; lossy 8→16→12→8):
  1. gamma-2.2 TRC: `CST_TRC[i] = round(65535·(i/255)^2.2)` (8→16 bit)
  2. 3×3 matrix signed Q16: `out = ((M[i]>>2)·v + …) >> 18` == `m·v/16`
     (16→12 bit; signed arithmetic shifts — `sar`/`imul` in de assembly).
     `CST_M = {65543, 0, 5, -3, 65541, 3, -2, -1, 65535}` — sRGB primaries
     met gamma22TRC (≈ identity, kleine off-diagonals)
  3. clamp `[0, 0xfff]`
  4. inverse TRC `CST_ITRC` (4096 entries, 12→8 bit)
  Tabellen: `native/cst_tables.h` (gegenereerd, in git).

  **3D LUT** (tetrahedral, 17³ nodes):
  - node index `(c2>>4)·289 + (c1>>4)·17 + (c0>>4)`; fracties `f = c&0xf`
    (deltas 289/17/1), descending sort
  - weights `w0=16-f[0]`, `w1=f[0]-f[1]`, `w2=f[1]-f[2]`, `w3=f[2]` (sommen 16)
  - nodes `base`, `base+d[0]`, `base+d[0]+d[1]`, `base+307` (c111)
  - resultaat `(Σ w·node) >> 4` (≤ 255; clamp overbodig)
  - white shortcut: `(c0 & c1 & c2) == 0xff` → `(255,255,255)`

  **.dlut formaat** (`driver/Phaser_6125N.dlut`, 265944 bytes, big-endian):
  - header: magic `DLUT`, u32be count=13, 13× u32be block-offsets,
    u32be metacount=79, 79× u32be `meta[]`
  - `meta[FXDLUT]` = 1-based blocknummer; FXDLUT komt uit de PPD
    (`FXDocumentType` setpagedevice: Normal 24, Photo 22, Presentation 26,
    Web 23, POP 32)
  - block data op `offsets[block-1]+8` (8-byte `dlt4`+size header), 20420
    bytes = 768 bytes 1D curves (ongebruikt in het pixel path) + 17³×4 LUT
  - block-selectie uit meta[]: Normal 24→block 3, Photo 22→block 1,
    Presentation 26→block 5, Web 23→block 2, POP 32→block 11
  - **file vs memory**: FXM_CC bewaart elke 4-byte node reversed in het
    geheugen — dump node = `[out2, out1, out0, pad]`, file node =
    `[pad, out0, out1, out2]` (bewezen: block 3 == `re/build/dlut-param.bin`
    met elke node byte-reversed). De driver leest het file formaat.

  **Gray**: `g = (c0·77 + c1·151 + c2·28) >> 8` over de CST output
  (NTSC 0.30/0.59/0.11; geverifieerd: `gray-test.fxr` → `4c 96 1b 81`).

  **Ground truth**: 32-bit FXM_CC via de i386 sysroot (`re/run-stage.sh`);
  captures `synth.fxr→synth-cc.fxr` (alle 256^3 kleuren),
  `s3-pr.fxr→s4-cc.fxr` (echte pagina, 500.990 px), `gray-test.fxr→gray-out.fxr`.
  Model (`re/verify_model.c`) en oracle (`re/calib.c`) zijn hier byte-exact
  tegen; de driver is byte-exact tegen de oracle (`re/verify-native.py`).

  **Env**: `FOO2SQ_CALIBRATE=0` (calibratie uit), `FOO2SQ_DLUT=<pad>` (override);
  default `.dlut` = `/usr/share/phaser6125/Phaser_6125N.dlut`. Zonder
  bruikbaar .dlut: CST-only (zoals FXM_CC zelf).
- FXM_SBP / FXM_PR details (gray / turnpage) niet gerepliceerd.
- Officieuze chain is near-lossless; de onze is exact lossless.
- CUPS draait filters als user `lp`; `/usr/lib/cups/filter` moet root:root 0755.

## 10. USB (vanuit Windows of Linux)

De Phaser 6125N heeft naast Ethernet een **USB 2.0 device-poort** (type B).
De Windows driver INF (`analysis/windriver/…/fxhl2zi.inf`) matcht het device
als `USBPRINT\FUJI_XEROXDocuPrint_13C9` — een **standaard USB Printing Class**
device (Windows `usbprint.sys`), géén vendor-specifieke USB stack. Ook
`LPTENUM\FUJI_XEROXDocuPrint_13C9` (parallelle poort) wordt ondersteund.
Het PJL/HBPL-protocol werkt over USB identiek aan raw JetDirect (9100).
(De INF is van de C525A — het zustermodel; de 6125N gebruikt dezelfde
engine, maar de exacte USB-string kan `XEROX Phaser 6125…` zijn.)

Drie routes, van snel/simpel naar "eigen driver end-to-end":

### B. Raw USB vanuit Windows (aanbevolen — geen driver nodig)

Windows hangt automatisch *USB Printing Support* (usbprint.sys) aan het
device; de poort verschijnt als `USB001` (of USB002…). Onze filter genereert
een complete PJL/HBPL stream — die stuur je raw naar de poort:

1. USB A-B kabel printer → Windows PC; in Device Manager verschijnt
   *USB Printing Support* onder *Universal Serial Bus devices*.
2. Genereer de stream in WSL (Linux):
   ```
   cd /home/rick/src/phaser6125
   PPD="$PWD/driver/Phaser_6125N-native.ppd" ./native/foo2sq21hbpl \
       1 rick test 1 "PageSize=A4 FXOutputMode=600x600 FXColorMode=Color" \
       tests/colorpage.pdf > /tmp/colorpage.hbpl
   ```
3. Kopieer naar Windows (bv. via `\\wsl$\Ubuntu\home\rick\…` → `C:\temp\`) en print:
   ```
   copy /b C:\temp\colorpage.hbpl \\.\USB001
   ```

### C. WSL2 + usbipd + CUPS usb:// (onze native driver, end-to-end)

`usbipd` (gratis, `winget install usbipd`) richt een USB-device door naar
WSL2; CUPS' usb backend (libusb) print dan met onze eigen driver:

```
# Windows (admin PowerShell)
usbipd wsl list
usbipd wsl attach --busid <busid>
# in WSL
lpinfo -v | grep usb          # geeft de usb:// URI
sudo lpadmin -p Phaser6125N -v usb://FUJI_XEROX/DocuPrint_13C9
```

### A. Vendor Windows driver via USB (minst interessant voor dit project)

De DPC525A DISM export (of de officiele Xerox 6125N Windows driver) kan via
USB installeren (LanguageMonitor "SimpleMonitor", `fxhl2zi.inf`) — maar dat
is "gewoon printen" met een driver uit 2012 (DriverVer 2.5.34.0), los van
onze reverse-engineering.

### Native Linux

`scripts/print-direct-usb.sh <bestand.pdf|ps> [\"options\"]` — filter →
`/dev/usb/lp0` (usblp kernel module; env `USB_DEVICE` override). In WSL2
bestaat `/dev/usb/lp0` niet (usbipd presenteert het device via libusb) —
daar is route C de manier.

**Opgelet**: de printer is gedeeld — elke testpagina kost een pagina. Eerst
het device checken (Device Manager / `lpinfo -v`), printen pas als het
device confirmed is. Controleer eventueel in CentreWare IS
(http://192.168.178.10, read-only) of de USB-poort ingeschakeld is.

## 11. Windows 11 driverless via IPP (context: V3/V4 driver deprecation)

Microsoft bouwt legacy V3/V4 printerdrivers af (end-of-servicing plan,
learn.microsoft.com "End of servicing plan for third-party printer
drivers"): sinds sept 2023 geen nieuwe V3/V4, sinds 15 jan 2026 geen
nieuwe via Windows Update (Win11+/Server 2025+), vanaf 1 juli 2026
voorrang voor de IPP inbox class driver, na juli 2027 alleen nog
securityfixes. Bestaande drivers blijven werken, maar printers zonder
IPP/Mopria vallen terug op handmatig geinstalleerde drivers.

De Phaser 6125N spreekt **geen IPP/Mopria** (bewezen: CentreWare IS HTTP,
SNMPv1-only, raw 9100 HBPL) — de vendor driver (route A, §10) is op
Windows 11 24H2+ op termijn een aflopende zaak.

**Onze setup is al de userspace-vertaling** die het nieuwe model verlangt:
`foo2sq21hbpl` rendert PDF → HBPL in userspace (ghostscript + onze chain),
en de WSL CUPS is een IPP-printserver. Wat ontbrak is alleen de
Windows→WSL IPP-verbinding (driverless, inbox class driver):

1. `sudo bash scripts/setup-wsl-ipp.sh` — zet `Listen *:631` in cupsd.conf
   (backup `.pre-ipp`), herstart CUPS, test de IPP endpoint via het WSL IP
   met `ipptool get-printer-attributes`
2. Windows: Instellingen → Printers & scanners → *Apparaat toevoegen* →
   *De printer die ik wil staat er niet bij* → *Een gedeelde printer op
   naam selecteren* → `http://<wsl-ip>:631/printers/Phaser6125N` — Windows
   detecteert IPP en gebruikt de **inbox class driver** (geen V3 driver)
3. Print vanuit Windows → CUPS (WSL) → `foo2sq21hbpl` → HBPL → printer
   (socket 9100, of USB via §10 route C)

Firewall: Windows Firewall blokkeert inkomend naar WSL soms — regel
toevoegen voor poort 631, of WSL2 mirrored networking
(`[wsl2] networkingMode=mirrored` in `%UserProfile%\.wslconfig`, daarna
`wsl --shutdown`) — WSL krijgt dan hetzelfde IP als Windows.

**USB als fallback** (§10): route B (raw `copy /b … \\.\USB001`) staat
helemaal buiten de print stack — immuun voor deprecation. Route C
(usbipd + CUPS `usb://`) is driverless maar via CUPS.

Voor meerdere printers / een appliance: PAPPL (github.com/istopwg/pappl)
of een Raspberry Pi + CUPS — onze repo is al compatibel getest op
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
  → device (socket://192.168.178.10:9100, usb:, file:)
```

The MIME filter follows the OpenPrinting "non-raster printer" pattern
(openprinting.github.io/documentation/02-designing-printer-drivers):
`papplSystemAddMIMEFilter(system, "application/pdf", "application/vnd.xerox-hbpl", cb, NULL)`.

### Build & run (Linux, PAPPL 1.4.x)

```sh
# deps: libcups2-dev libavahi-client-dev + PAPPL 1.4.x installed in /usr/local
bash scripts/build-pappl-app.sh

LD_LIBRARY_PATH=/usr/local/lib pappl/phaser6125-pappl server -o server-port=8631 &
LD_LIBRARY_PATH=/usr/local/lib pappl/phaser6125-pappl add -d phaser \
    -m xerox_phaser_6125n -v socket://192.168.178.10:9100
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

### Verification (all PASS)

- `ipptool Get-Printer-Attributes` via TCP 8631: advertises
  `printer-resolution-default=600dpi`, `document-format-supported` includes
  pdf/ps/jpeg/png/text + `application/vnd.xerox-hbpl`
- `ipptool Print-Job` (PDF, no explicit resolution, like Windows) → job accepted,
  output **byte-identical** to the CUPS filter reference (after DATE/TIME/DNAME
  normalisation), `@PJL SET RESOLUTION=600`
- `pappl submit -d phaser -o printer-resolution=600x600dpi tests/colorpage.pdf`
  → byte-identical as well

### TODO

- **Windows build — DONE (mingw cross-compile, no VS2022 needed)**:
  `scripts/build-windows-mingw.sh` downloads the prebuilt MSVC .lib files from
  NuGet (libpappl1_native 1.4.12, libcups2_native 2.4.20, libressl, libjpeg-turbo,
  libpng, zlib), compiles with `x86_64-w64-mingw32-gcc` and links against the
  .lib files (mingw ld handles MSVC COFF). Result: `build-mingw/phaser6125-pappl.exe`
  (PE32+ x86-64, 640 KB, only 4 DLL deps: KERNEL32, msvcrt, libcups2.dll,
  libpappl1.dll). Verified under wine64: `--help` shows full PAPPL usage.
  `start_gs()` _WIN32 path uses CreateProcess + gswin64c.exe (found next to
  the .exe or on PATH). Packaging: `scripts/package-windows.sh` →
  `Phaser6125N-PAPPL-win64.zip` (~25 MB, exe + 11 DLLs + bundled
  Ghostscript 10.08 in `gs/` + install-phaser6125.bat + README.txt).
  Ghostscript (AGPL) auto-downloaded from ArtifexSoftware GitHub releases
  and extracted with 7z — no separate install needed.
  Alternative: `vcnet/` MSVC project for those with VS2022.
- **mDNS/DNS-SD auto-discovery — eigen implementatie, geen Bonjour nodig**:
  `native/mdns_mini.c` (~400 lines, RFC 6762) adverteert `_ipp._tcp.local` +
  `_universal._tcp.local` via multicast 224.0.0.251:5353. Start in
  `system_cb` via `mdns_start("Phaser6125N", port, "ipp/print/Phaser6125")`.
  Verified: unicast PTR query → correct 519-byte response (ANCOUNT=4:
  PTR+SRV+TXT+A); multicast announcements confirmed via tcpdump.
  (WSL2 ziet eigen multicast niet terug door NAT — op echt Windows/Linux
  werkt discovery gewoon.) PAPPL's eigen Avahi/Bonjour path werkt ook
  als dnssd.dll aanwezig is, maar Apple's Bonjour SDK SLA staat
  redistributie van dnssd.dll in eigen packages niet toe — vandaar
  mdns_mini.c als open alternatief.
  **Verified on real Windows 11 (WINNIE)**: multicast PTR query from the
  network gets responses from the Windows host with
  `Phaser6125N._ipp._tcp.local` (ANCOUNT=4). The service IS discoverable
  via mDNS — visible to macOS/iOS/Android/Linux clients.
  **Windows discovery works via PAPPL's built-in DNS-SD**: on Windows 10/11
  UDP 5353 is always occupied by the OS mDNS responder (svchost/dnscache)
  or Bonjour (mDNSResponder.exe) — our mdns_mini.c cannot bind there (by
  design it's a fallback for systems without a DNS-SD stack). PAPPL's own
  path loads dnssd.dll via LoadLibrary (present when Bonjour installed,
  which is common) and registers `_ipp._tcp` with full IPP Everywhere TXT
  records automatically. Verified: after Create-Printer, multicast PTR
  query from the LAN interface (192.168.178.46) returns Phaser6125.
  mdns_mini.c fix: join multicast group on INADDR_ANY (getaddrinfo may
  pick the wrong interface IP on multi-homed Windows hosts).
  **Windows 11 caveat**: Settings → "Add device" primarily uses
  WS-Discovery (WSD), not mDNS. Windows users should use
  "The printer that I want isn't listed" → enter
  `http://<host-ip>:8631/ipp/print/Phaser6125` — the inbox IPP class
  driver picks it up from there. (WS-Discovery/SOAP responder is a
  possible future addition but much larger scope.)
- **Portable mode (geen admin nodig)**: poort 8631 is unprivileged (>1024),
  spool gaat naar `%LocalAppData%`. `dist-windows/run-portable.bat` draait
  direct uit de uitgepakte zip — geen installatie naar Program Files.
- **Linux packaging (optional)**: `pappl-retrofit` (legacy-printer-app.c)
  can wrap our PPD + CUPS filter out-of-the-box for Snap packaging
  (Linux only — libcupsfilters/libppd have no Windows build).
- **SNMP→IPP status bridge — DONE**: `native/snmp_poll.c` (own minimal
  SNMPv1 BER client, no net-snmp dep) polls the Printer-MIB over UDP/161
  (community "public"). Strategy: 1 GET sysDescr (online check) + 1 GET
  hrDeviceStatus + short GETNEXT count walk + chunked multi-OID GETs
  (batch=4 for alerts with long texts, 10 for supplies — printer returns
  tooBig on larger batches). Maps prtMarkerSuppliesTable → IPP
  marker-levels/names/types/colors and prtAlertTable → printer-state-reasons
  (text-based matching: "paper"→media-empty, "Toner"→toner-low, etc.).
  Verified via ipptool: marker-levels=-1,-1,-1,-1,80,100 (CMYK unknown for
  non-Xerox toners, Imaging Unit 80%, Fuser 100%), printer-state-reasons
  = media-empty,toner-low with tray open. PAPPL status_cb wired in
  driver_cb (has_supplies=true).
- **Real-hardware Windows test**: the .exe is verified under wine64
  (--help renders, PE32+ valid) and the HBPL output is byte-identical
  to the CUPS filter, but a full end-to-end print on real Windows 11
  with the physical printer is still pending (needs a Windows host).
