# Xerox Printer 6125N — Driverless IPP Print Server (Windows 11)

This package makes your Xerox Printer 6125N (or Fuji Xerox DocuPrint C525A)
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

## Device Information (Manufacturer / Model / Serial)

The gateway reports IEEE-1284 device ID via IPP `printer-device-id`:
`MFG:Xerox;MDL:Printer 6125N;SN:LAL035151;CMD:...` — Windows 11 Device
Information shows Manufacturer=Xerox, Model=Printer 6125N,
Serial=LAL035151 (read from the printer via SNMP prtGeneralSerialNumber).
To change the serial for another unit, edit the `printer-device-id`
option in the .bat add command.

Known limitation: Windows 11 Device Information shows Manufacturer and Model
from the IPP printer-device-id (MFG:/MDL:), but **Serial Number only comes
from WSD device metadata** — for a pure-IPP printer it shows "Unavailable".
The SN= field is present in printer-device-id for clients that parse it.

## Quick start

1. Unzip anywhere, double-click `run-portable.bat` (no admin needed), OR
   run `install-phaser6125.bat` as Administrator for a system-wide install.
2. The printer `Xerox Printer 6125N` is added (socket://192.168.178.10:9100) and the
   IPP server starts on port 8631. Ghostscript is already bundled in `gs/`.
   Set the printer IP via the `PRINTER_IP` variable at the top of the .bat file (default `192.168.178.10`), or via environment variable: `set PRINTER_IP=192.168.1.50` before running.

### USB instead of network

If the printer is connected via USB to the gateway PC, set the device
(default is now the USB raw share — run-portable.bat creates it):
instead of the IP:

```bat
set PRINTER_DEV=usbprint://%COMPUTERNAME%/PhaserRaw
```

`usbprint:` writes the raw HBPL stream to a **raw printer share** on the USB
port (`\\%COMPUTERNAME%\PhaserRaw`, created by run-portable.bat with the
Generic/Text Only driver on USB001). This uses the spooler API
(OpenPrinter/WritePrinter, RAW datatype) — the same channel as
`copy /b out.bin \\%COMPUTERNAME%\PhaserRaw`. Note: `\\.\USB00x` device
files do not exist on Windows 11 24H2+ (err 2), hence the share route.
Verified end-to-end: 263 KB HBPL job written to the USB printer, job completed.
 — just switch PRINTER_DEV.

For the USB00x device file to exist, Windows must bind **usbprint.sys**
("USB Printing Support") to the printer: Device Manager → USB devices →
Xerox Phaser 6125N (VID_046D&PID_3D61) → Update driver → Browse →
"USB Printing Support". Then a USB00x port appears and usbprint: works.
While the vendor driver owns the device, no USB00x device file exists. — just switch PRINTER_DEV.

## Two ways to run

**Portable (no admin rights needed):**
Double-click `run-portable.bat` — runs straight from the unzipped folder.
Port 8631 is unprivileged (>1024), spool goes to `%LocalAppData%`.

**Installed:**
Run `install-phaser6125.bat` as Administrator (installs to Program Files).

## Troubleshooting: "Sent to printer, Error" in Windows

If Windows shows "Sent to printer, Error" (also after "Resume All"), the
queue was auto-created with a **WSD port** (see Printer Properties → Ports →
"WSD-..."). Our gateway speaks **IPP only** — WSD jobs fail. Fix:

1. Delete the printer queue (right-click → Remove device).
2. Re-add manually: Settings → Printers & scanners → Add device →
   *The printer that I want isn't listed* → *Select a shared printer by name* →
   `http://<this-pc-ip>:8631/ipp/print/Xerox_Printer_6125N`
3. Windows then creates an **IPP port** and prints via IPP (verified working).

Auto-discovery via mDNS finds the gateway. If Windows assigns a **WSD port**
to the discovered device (Printer Properties → Ports → "WSD-..."), two
things to check:

1. **Turn OFF automatic device setup**: Settings → Bluetooth & devices →
   Devices → "Let Windows automatically install apps for network devices"
   (or equivalent auto-setup toggle). With auto-setup ON, Windows installs
   discovered devices via WSD by default. With it OFF, the Add-device wizard
   uses IPP classification (TXT record → live IPP probe → driver bind).
2. **Remove stale WSD queues/ports** (Windows caches Function Discovery
   results — old WSD entries respawn): run in PowerShell as admin:
   ```powershell
   Get-Printer | Where-Object { $_.Name -match 'Xerox|Phaser|6125' } | Remove-Printer
   Get-PrinterPort | Where-Object { $_.Name -like 'WSD*' } | Remove-PrinterPort
   Restart-Service fdPHost, FDResPub, Dnscache -Force
   ```
3. Re-add via the manual IPP URL above — Windows then creates an IPP port
   and printing works (verified: jobs complete, bytes reach the printer).

Note: WS-Discovery uses multicast group 239.255.255.250:3702 (captured with
tshark during Add Device). Our gateway answers mDNS (UDP 5353) but not WSD,
so a WSD-assigned port can never print to it.

Our gateway advertises complete IPP Everywhere TXT via mDNS (rp, ty,
product, pdl, URF, UUID=printer-uuid, mopria-certified, Color, Duplex,
adminurl=http://<host>:8631/...) but has no WSD responder — WSD ports
cannot print to it. Also: the mDNS service name follows the queue name — run-portable.bat
now removes a legacy "Phaser6125" queue and creates "Xerox Printer 6125N",
so discovery shows the official name.

## Adding the printer on Windows 11

Settings → Bluetooth & devices → Printers & scanners → **Add device** →
*The printer that I want isn't listed* → *Select a shared printer by name* →
enter:

```
http://<this-pc-ip>:8631/ipp/print/Xerox_Printer_6125N
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
*Select a shared printer by name* → `http://<this-pc-ip>:8631/ipp/print/Xerox_Printer_6125N`

Windows 11 uses the inbox IPP class driver — no legacy V3/V4 driver needed.

## Manual usage

```bat
phaser6125-pappl.exe server -o server-port=8631
phaser6125-pappl.exe add -d "Xerox Printer 6125N" -m xerox_phaser_6125n -v socket://192.168.178.10:9100
phaser6125-pappl.exe submit -d "Xerox Printer 6125N" document.pdf
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
