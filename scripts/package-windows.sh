#!/bin/bash
# package-windows.sh — build + package the Windows distribution zip.
#
# Output: build-mingw/Phaser6125N-PAPPL-win64.zip
#   Contains: phaser6125-pappl.exe, runtime DLLs, gs/ (Ghostscript 10.x),
#             install-phaser6125.bat, README.txt
#
# Ghostscript (AGPL) is downloaded from ArtifexSoftware's GitHub releases
# and bundled — no separate install needed.
# dnssd.dll (Apple Bonjour) is NOT bundled (licensing); if present on the
# system (or next to the .exe) PAPPL auto-advertises via mDNS/DNS-SD.
set -euo pipefail

cd "$(dirname "$0")/.."

bash scripts/build-windows-mingw.sh

DIST=build-mingw/dist
THIRD=build-mingw/3rdparty
ZIP=build-mingw/Phaser6125N-PAPPL-win64.zip

# --- Bundle Ghostscript ---
echo "==> bundling Ghostscript into $DIST/gs/"
GS_VER=10.08.0
GS_EXE="$THIRD/gs${GS_VER//./}w64.exe"
if [ ! -f "$GS_EXE" ]; then
    echo "    downloading Ghostscript $GS_VER win64 installer..."
    curl -sL -o "$GS_EXE" \
        "https://github.com/ArtifexSoftware/ghostpdl-downloads/releases/download/gs${GS_VER//./}/gs${GS_VER//./}w64.exe"
fi
mkdir -p "$THIRD/gs-extract"
7z x -y -o"$THIRD/gs-extract" "$GS_EXE" > /dev/null 2>&1
mkdir -p "$DIST/gs/bin"
cp "$THIRD/gs-extract/bin/gswin64c.exe" "$THIRD/gs-extract/bin/gsdll64.dll" "$DIST/gs/bin/"
cp -r "$THIRD/gs-extract/lib" "$DIST/gs/" 2>/dev/null || true
cp -r "$THIRD/gs-extract/Resource" "$DIST/gs/" 2>/dev/null || true
cp -r "$THIRD/gs-extract/iccprofiles" "$DIST/gs/" 2>/dev/null || true
echo "    gs/ contents: $(du -sh "$DIST/gs" | cut -f1)"

# --- Optional: dnssd.dll if present in 3rdparty (not auto-downloaded) ---
if [ -f "$THIRD/dnssd.dll" ]; then
    cp "$THIRD/dnssd.dll" "$DIST/"
    echo "==> dnssd.dll bundled (mDNS/DNS-SD auto-discovery enabled)"
else
    echo "==> dnssd.dll not bundled — install Bonjour Print Services for mDNS discovery"
fi

# --- Add installer + readme ---
cp dist-windows/install-phaser6125.bat "$DIST/"
cp dist-windows/README.txt "$DIST/"

# --- Zip ---
rm -f "$ZIP"
cd "$DIST"
zip -r -q "../$(basename "$ZIP")" . -x '*.pdb'
cd ../..

echo "==> OK: $ZIP ($(du -h "$ZIP" | cut -f1))"
unzip -l "$ZIP" | tail -25
