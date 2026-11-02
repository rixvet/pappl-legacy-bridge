# Phaser 6125N — Driverless IPP Print Server (Windows 11)

This package makes your Xerox Phaser 6125N (or Fuji Xerox DocuPrint C525A)
work on Windows 11 **without any legacy V3/V4 printer driver**. Windows uses
its built-in IPP class driver — future-proof against Microsoft's driver
deprecation plan.

## Contents

| File/Folder | Purpose |
|---|---|
| `phaser6125-pappl.exe` | The IPP print server (PAPPL-based) |
| `*.dll` | Runtime libraries (libpappl, libcups, libressl, …) |
| `gs/` | **Bundled Ghostscript 10.08** (AGPL) — `gs/bin/gswin64c.exe` |
| `install-phaser6125.bat` | One-click installer |
| `dnssd.dll` | *Optional* — Apple Bonjour, enables mDNS auto-discovery |

## Installation

1. Run `install-phaser6125.bat` **as Administrator**.
2. The printer `Phaser6125` is added (socket://192.168.178.10:9100) and the
   IPP server starts on port 8631. Ghostscript is already bundled in `gs/`.

## Auto-discovery (mDNS / DNS-SD)

PAPPL automatically advertises the printer via DNS-SD (`_ipp._tcp`) when
`dnssd.dll` (Apple Bonjour) is available. Install **Bonjour Print Services
for Windows** (https://support.apple.com/kb/DL999) or copy `dnssd.dll`
next to the .exe — then Windows 11 finds the printer automatically in
*Add device* without typing a URL.

Without Bonjour, use the manual URL below.

## Using from other Windows 11 PCs

With Bonjour: Settings → Printers & scanners → **Add device** — the
`Phaser6125` printer appears automatically.

Without Bonjour: *The printer that I want isn't listed* →
*Select a shared printer by name* → `http://<this-pc-ip>:8631/ipp/print/Phaser6125`

Windows 11 uses the inbox IPP class driver — no legacy V3/V4 driver needed.

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
