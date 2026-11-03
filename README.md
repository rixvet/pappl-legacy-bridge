# pappl-legacy-bridge

Bridge/framework rond [PAPPL](https://github.com/michaelrsweet/pappl) waarmee
**legacy printers zonder IPP/Mopria** als driverless IPP Everywhere printer
online komen — zonder V3/V4 driver (die Windows 11 afbouwt).

- **Reference implementation:** Xerox Phaser 6125N — HBPL-protocol volledig
  reverse-engineered (byte-exact t.o.v. de originele Windows driver), print
  via netwerk (socket 9100) of USB (raw share), Windows .exe package met
  gebundelde Ghostscript, mDNS auto-discovery, SNMP→IPP status (toner-niveaus,
  alerts).
- **Volgende kandidaat:** Fuji Xerox DocuPrint C525A (zelfde HBPL familie).

Zie [RE-NOTES.md](RE-NOTES.md) voor de volledige reverse-engineering notes en
[docs/](docs/) voor o.a. het verwijderen van vendor v3-drivers op Windows 11.

## Build

```sh
bash scripts/build-pappl-app.sh        # Linux (PAPPL 1.4.x in /usr/local)
bash scripts/build-windows-mingw.sh    # Windows .exe via mingw + NuGet libs
bash scripts/package-windows.sh        # Phaser6125N-PAPPL-win64.zip (25 MB)
```

## License

- **Code**: Apache-2.0 — zie [LICENSE.md](LICENSE.md)
- **Documentatie/content** (RE-NOTES.md, docs/, README's): CC BY 4.0 — zie [docs/CC-BY-4.0.txt](docs/CC-BY-4.0.txt)
- Third-party (PAPPL, Ghostscript, NuGet packages): zie [LICENSE-NOTES.md](LICENSE-NOTES.md)
