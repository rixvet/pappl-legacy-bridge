# Phaser 6125N — Driverless IPP Print Server (Windows 11)

This package makes your Xerox Phaser 6125N (or Fuji Xerox DocuPrint C525A)
work on Windows 11 **without any legacy V3/V4 printer driver**. Windows uses
its built-in IPP class driver — future-proof against Microsoft's driver
deprecation plan.

## Contents

| File | Purpose |
|---|---|
| `phaser6125-pappl.exe` | The IPP print server (PAPPL-based) |
| `libpappl1.dll`, `libcups2.dll`, … | Runtime libraries |
| `install-phaser6125.bat` | One-click installer |
| `gswin64c.exe` | **You must add this** — Ghostscript 10.x 64-bit |

## Installation

1. Download **Ghostscript 10.x for Windows (64-bit)** from
   https://ghostscript.com/releases/gsdnld.html — grab the `.msi`, install it,
   then copy `gswin64c.exe` from `C:\Program Files\gs\gs10.xx\bin\` into
   this folder.
2. Run `install-phaser6125.bat` **as Administrator**.
3. The printer `Phaser6125` is added and the IPP server starts on port 8631.

## Using from other Windows 11 PCs

Settings → Printers & scanners → **Add device** → *The printer that I want
isn't listed* → *Select a shared printer by name* → enter:

```
http://<this-pc-ip>:8631/ipp/print/Phaser6125
```

Windows 11 detects IPP and uses the inbox class driver automatically.
No driver installation needed on the client.

## Manual usage

```bat
phaser6125-pappl.exe server -o server-port=8631
phaser6125-pappl.exe add -d Phaser6125 -m xerox_phaser_6125n -v socket://192.168.178.10:9100
phaser6125-pappl.exe submit -d Phaser6125 document.pdf
phaser6125-pappl.exe printers
phaser6125-pappl.exe jobs
```

Change `192.168.178.10` to your printer's IP address.

## Web interface

Open `http://localhost:8631/` in a browser to see printers, jobs and logs.

## Technical details

The server receives PDF/PS/JPEG/PNG via IPP, rasterises with Ghostscript
(ppmraw), encodes to the printer's native HBPL format (reverse-engineered),
and sends it to the printer via raw socket (port 9100). Output is
byte-identical to the original Windows driver.

Source code & reverse-engineering notes: see the project repository
(RE-NOTES.md).
