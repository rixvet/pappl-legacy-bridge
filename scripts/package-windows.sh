#!/bin/bash
# package-windows.sh — build + package the Windows distribution zip.
#
# Output: build-mingw/Phaser6125N-PAPPL-win64.zip
#   Contains: phaser6125-pappl.exe, all runtime DLLs, install-phaser6125.bat,
#             README.txt
#
# The user must add gswin64c.exe (Ghostscript) themselves (licensing).
set -euo pipefail

cd "$(dirname "$0")/.."

bash scripts/build-windows-mingw.sh

DIST=build-mingw/dist
ZIP=build-mingw/Phaser6125N-PAPPL-win64.zip

# Add installer + readme to dist
cp dist-windows/install-phaser6125.bat "$DIST/"
cp dist-windows/README.txt "$DIST/"

rm -f "$ZIP"
cd "$DIST"
zip -r "../$(basename "$ZIP")" . -x '*.pdb'
cd ../..

echo "==> OK: $ZIP ($(stat -c%s "$ZIP") bytes)"
unzip -l "$ZIP"
