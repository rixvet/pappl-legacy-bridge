# Licenses in this repository

| Part | License | File |
|---|---|---|
| **Code** (C sources, scripts, build files, .bat/.ps1) | Apache License 2.0 | `LICENSE.md` |
| **Documentation & content** (RE-NOTES.md, ANALYSIS.md, docs/, READMEs, dist-windows/README.txt) | CC BY 4.0 | `docs/CC-BY-4.0.txt` |

Apache-2.0 for the code (attribution + patent grant — comparable to CC BY 4.0;
chosen over BSD because of the explicit patent grant, relevant for
reverse-engineered protocol work, and because PAPPL itself is Apache-2.0).
CC BY 4.0 for all textual documentation.

## Third-party

- **PAPPL** (Michael Sweet) — Apache-2.0, see https://github.com/michaelrsweet/pappl
- **Ghostscript** (Artifex) — AGPL-3.0, bundled in the Windows package as a
  *separate* process (`gs/bin/gswin64c.exe`); our code invokes it via subprocess,
  so our code is not a GPL/AGPL derivative. Download source:
  https://github.com/ArtifexSoftware/ghostpdl-downloads/releases
- **NuGet packages** (libpappl1, libcups2, libressl, libjpeg-turbo, libpng, zlib) —
  per-package licenses, see `vcnet/packages.config`
