# pappl-legacy-bridge

Bridge/framework around [PAPPL](https://github.com/michaelrsweet/pappl) that brings
**legacy printers without IPP/Mopria** online as driverless IPP Everywhere printers —
no V3/V4 driver needed (Windows 11 is phasing those out).

- **Reference implementation:** Xerox Phaser 6125N — HBPL protocol fully
  reverse-engineered (byte-exact vs. the original Windows driver), printing via
  network (socket 9100) or USB (raw share), Windows .exe package with bundled
  Ghostscript, mDNS auto-discovery, SNMP→IPP status (toner levels, alerts).
- **Next candidate:** Fuji Xerox DocuPrint C525A (same HBPL family).

See [RE-NOTES.md](RE-NOTES.md) for the full reverse-engineering notes and
[docs/](docs/) for e.g. removing vendor v3 drivers on Windows 11.

## Build

```sh
bash scripts/build-pappl-app.sh        # Linux (PAPPL 1.4.x in /usr/local)
bash scripts/build-windows-mingw.sh    # Windows .exe via mingw + NuGet libs
bash scripts/package-windows.sh        # Phaser6125N-PAPPL-win64.zip (25 MB)
```

## License

- **Code**: Apache-2.0 — see [LICENSE.md](LICENSE.md)
- **Documentation/content** (RE-NOTES.md, docs/, READMEs): CC BY 4.0 — see [docs/CC-BY-4.0.txt](docs/CC-BY-4.0.txt)
- Third-party (PAPPL, Ghostscript, NuGet packages): see [LICENSE-NOTES.md](LICENSE-NOTES.md)
