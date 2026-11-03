# Licenties in deze repository

| Onderdeel | Licentie | Bestand |
|---|---|---|
| **Code** (C sources, scripts, build files, .bat/.ps1) | Apache License 2.0 | `LICENSE` |
| **Documentatie & content** (RE-NOTES.md, ANALYSIS.md, docs/, README's, dist-windows/README.txt) | CC BY 4.0 | `docs/CC-BY-4.0.txt` |

Apache-2.0 voor de code (attribution + patent grant — vergelijkbaar met CC BY 4.0).
CC BY 4.0 voor alle tekstuele documentatie.

## Third-party

- **PAPPL** (Michael Sweet) — Apache-2.0, zie https://github.com/michaelrsweet/pappl
- **Ghostscript** (Artifex) — AGPL-3.0, gebundeld in de Windows package als
  *apart* proces (`gs/bin/gswin64c.exe`); onze code roept het aan via subprocess,
  dus er is geen GPL/AGPL-afgeleide van onze code. Download-bron:
  https://github.com/ArtifexSoftware/ghostpdl-downloads/releases
- **NuGet packages** (libpappl1, libcups2, libressl, libjpeg-turbo, libpng, zlib) —
  eigen licenties per package, zie `vcnet/packages.config`
