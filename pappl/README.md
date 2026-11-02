# phaser6125-pappl — PAPPL Printer Application for Xerox Phaser 6125N

Driverless IPP printer application (IPP Everywhere / Mopria) built on
[PAPPL](https://github.com/michaelrsweet/pappl) 1.4.x. Makes the Phaser 6125N
(and Fuji Xerox DocuPrint C525A) appear as a standard IPP printer on
Windows 11 (inbox class driver), macOS, iOS, Android and Linux — no legacy
V3/V4 vendor driver needed.

## Linux build & run

```sh
# deps: libcups2-dev libavahi-client-dev + PAPPL 1.4.x in /usr/local
bash scripts/build-pappl-app.sh

LD_LIBRARY_PATH=/usr/local/lib pappl/phaser6125-pappl server -o server-port=8631 &
LD_LIBRARY_PATH=/usr/local/lib pappl/phaser6125-pappl add -d phaser \
    -m xerox_phaser_6125n -v socket://192.168.178.10:9100

# test
ipptool -tv ipp://127.0.0.1:8631/ipp/print/phaser <ipptool-test-file>
LD_LIBRARY_PATH=/usr/local/lib pappl/phaser6125-pappl submit -d phaser tests/colorpage.pdf
```

## Windows build & run

```bat
REM prereq: Visual Studio 2022 (v143 toolset), NuGet, Ghostscript 10.x (gswin64c.exe)
vcnet\build-windows.bat

x64\Release\phaser6125-pappl.exe server -o server-port=8631
x64\Release\phaser6125-pappl.exe add -d Phaser6125 -m xerox_phaser_6125n -v socket://192.168.178.10:9100
```

Place `gswin64c.exe` (Ghostscript for Windows, 64-bit) next to the .exe.
On other Windows 11 PCs: Settings → Printers → Add device →
`http://<this-pc>:8631/ipp/print/Phaser6125` — Windows uses the inbox
IPP class driver automatically.

## Architecture

```
IPP client → PAPPL (TCP 8631) → MIME filter → gs (ppmraw) → hbpl_core → HBPL → device
```

See `RE-NOTES.md` §12 for the full PAPPL 1.4.x gotcha list.
