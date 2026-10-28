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

## 6. Native driver (`native/foo2sq21hbpl.c`)

CUPS filter, libc only, native 64-bit (geen multilib). Interace zoals CUPS dat
van filters verwacht: `foo2sq21hbpl job-id user title copies options [file]`,
omgeving `PPD`.

Pipeline:
1. parse options (`PageSize`/`PageRegion`, `FXOutputMode`→dpi (300/600),
   `FXColorMode` Black→grijs, rest → kleur)
2. PPD lookup (`PaperDimension`, `ImageableArea` voor de page size)
3. fork/exec `gs` (`-r{dpi} -dDEVICEWIDTHPOINTS= -dDEVICEHEIGHTPOINTS=` uit de PPD,
   `-sDEVICE=ppmraw -sOUTPUTFILE=%stdout`)
4. per pagina: P6 parsen → crop (PM2FXR regel) → expand naar 4-byte `[ff,R,G,B]`
   → breedte padden naar veelvoud van 8 (`ffffffff`) → sq21-encode →
   C/QR/body/payload/"SD" per pagina
5. PJL header + A-record eenmalig; job trailer aan het eind

De encoder is **exact lossless** (de officieuze FXM_ALC is near-lossless, ±1-2 per
pixel — beide zijn geldige streams; onze payload is ~0.8% groter).

Build (schoon onder gcc -Wall -Wextra én clang -Weverything -Werror):

```
gcc -O2 -Wall -Wextra -o foo2sq21hbpl native/foo2sq21hbpl.c -lm
```

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
- Getest & EXACT: A4@600 1-page, A4@600 2-page, A4@300, Letter@600,
  FXColorMode=Black, direct PDF input, colorpage (kleur/noise/schaakbord)
- **Echte printer**: 1-page print (page count 8660→8661), 2-page print
  (8661→8663), colorpage met meahow (8663→8664); job log toont `Completed`
  met het juiste pagina-aantal.

## 8. Installatie (native, 64-bit)

```
sudo bash scripts/setup-cups-native.sh       # CUPS + ghostscript (geen i386!)
sudo bash scripts/install-native-driver.sh   # build+install filter, PPD, queue
bash scripts/test-print.sh                   # drukt tests/colorpage.pdf af
```

- filter → `/usr/lib/cups/filter/foo2sq21hbpl` (root:root 0755)
- PPD → `/usr/share/cups/model/Xerox/en/Phaser_6125N-native.ppd`
  (zelfde opties als de originele PPD; `*cupsFilter` wijst naar foo2sq21hbpl)
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
- Wil je ooit toch van de PPD af: de filter kan verpakt worden als IPP printer
  application (`ippserver`) — nu niet nodig.

## 9. Bekende beperkingen / vervolgwerk

- **Color calibration (FXM_CC + dlut) NIET gerepliceerd** — maar het formaat is
  inmiddels goeddeels gekraakt (zie onder). Kleuren zijn structureel correct
  (zwart→zwart, wit→wit geverifieerd; de kleurtestpagina ziet er goed uit),
  maar niet gekalibreerd zoals de officieuze driver.

  **DLUT formaat** (`driver/Phaser_6125N.dlut`, 265944 bytes, big-endian):
  - header: magic `DLUT`, u32be count=13, 13× u32be block-offsets
    (13 aaneengesloten blokken van 20428 bytes; laatste blok eindigt exact
    op EOF)
  - elk blok: magic `dlt4` + u32be(0) + **3×256 bytes 1D curves** (R,G,B —
    in dit bestand allemaal identity) + **17³ × 4 bytes 3D LUT** (4913 nodes,
    byte range 0..255; vermoedelijk RGB+pad — block 11 toont (0,~59,255,0)-achtige
    kleuren, block 6 een schone G-ramp met stap 16 = 256/16 ✓)
  - 13 blokken = ~9 distincte calibratie-sets (sommige duplicaten) — vermutlich
    per papiertype/printmodus; block-selectie-logica nog te bepalen
  - FXM_CC heet intern `fxcolorconv` en refereert ICC gamma-TRC tags
    (`gamma10/14/18/22/26TRC`) — waarschijnlijk voor een ICC-input path;
    voor deze printer is de stage een per-pixel RGB→RGB 24bpp transform
    (s3-pr.fxr en s4-cc.fxr hebben identieke FXR headers: 595×842, 24bpp)
  - **Inschatting vervolgwerk: ~half dag (3-5 uur).** Resteren: node byte order,
    grid axis order, interpolatie (trilinear vs tetrahedral), block-selectie.
    Implementatie = ~150 regels C (trilinear over 17³) in de driver; oracle =
    32-bit FXM_CC via de i386 sysroot + capture `re/build/s4-cc.fxr` voor
    byte-exact verificatie. Laag risico dankzij de oracle.
  - Alternatief (niet exact): de Windows driver ship `fxhl2zp1.icm`; met lcms2
    krijg je ICC-gekalibreerde output zonder de DLUT te reversen, maar dat
    matcht de officieuze driver niet byte-exact.
- FXM_SBP / FXM_PR details (gray / turnpage) niet gerepliceerd.
- Officieuze chain is near-lossless; de onze is exact lossless.
- CUPS draait filters als user `lp`; `/usr/lib/cups/filter` moet root:root 0755.
