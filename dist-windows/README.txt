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
| `run-portable.bat` | **Portable start** — no admin needed, runs from the zip |

## Quick start

1. Unzip anywhere, double-click `run-portable.bat` (no admin needed), OR
   run `install-phaser6125.bat` as Administrator for a system-wide install.
2. The printer `Phaser6125` is added (socket://192.168.178.10:9100) and the
   IPP server starts on port 8631. Ghostscript is already bundled in `gs/`.
   Change `192.168.178.10` in the .bat to your printer's IP.

## Two ways to run

**Portable (no admin rights needed):**
Double-click `run-portable.bat` — runs straight from the unzipped folder.
Port 8631 is unprivileged (>1024), spool goes to `%LocalAppData%`.

**Installed:**
Run `install-phaser6125.bat` as Administrator (installs to Program Files).

## Adding the printer on Windows 11

Settings → Bluetooth & devices → Printers & scanners → **Add device** →
*The printer that I want isn't listed* → *Select a shared printer by name* →
enter:

```
http://<this-pc-ip>:8631/ipp/print/Phaser6125
```

Windows 11 detects IPP and uses the **inbox class driver** — no legacy
V3/V4 driver needed. (Replace `<this-pc-ip>` with the PC's IP address.)

## Auto-discovery (mDNS / DNS-SD)

The server advertises `_ipp._tcp.local` via a built-in minimal mDNS
responder (RFC 6762) — no Bonjour/Apple dnssd.dll needed. This makes the
printer auto-discoverable on **macOS, iOS, Android and Linux** clients.
Note: Windows 11's *Add device* UI primarily uses WS-Discovery, so on
Windows use the manual URL above (the IPP endpoint is fully functional).

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
