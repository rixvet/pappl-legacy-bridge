#!/bin/bash
# build-windows-mingw.sh — cross-compile phaser6125-pappl.exe for Windows
# from Linux/WSL using mingw-w64 + prebuilt NuGet .lib files (no VS2022 needed).
#
# Prerequisites (Ubuntu/WSL):
#   sudo apt install gcc-mingw-w64-x86-64 unzip curl
#
# Output: build-mingw/phaser6125-pappl.exe + build-mingw/dist/ (exe + DLLs)
set -euo pipefail

cd "$(dirname "$0")/.."

PREFIX=build-mingw
NUGET=$PREFIX/nuget
OBJ=$PREFIX/obj
DIST=$PREFIX/dist
MINGW=x86_64-w64-mingw32-gcc

# --- NuGet package versions (match vcnet/packages.config) ---
declare -A PKGS=(
    [libpappl1_native]=1.4.12
    [libcups2_native]=2.4.20
    [libressl_native]=3.7.3.0
    [libjpeg-turbo-v142]=2.0.4.3
    [libpng_native]=1.6.30
    [zlib_native]=1.2.11
)
declare -A REDIST=(
    [libpappl1_native.redist]=1.4.12
    [libcups2_native.redist]=2.4.20
    [libressl_native.redist]=3.7.3.0
    [libjpeg-turbo-v142]=2.0.4.3   # DLLs come from the non-redist package
    [libpng_native.redist]=1.6.30
    [zlib_native.redist]=1.2.11
)

mkdir -p "$NUGET" "$OBJ" "$DIST"

# --- Download + extract NuGet packages ---
for name in "${!PKGS[@]}"; do
    ver=${PKGS[$name]}
    nupkg="$NUGET/$name.$ver.nupkg"
    if [ ! -f "$nupkg" ]; then
        echo "==> downloading $name $ver"
        curl -sL "https://www.nuget.org/api/v2/package/$name/$ver" -o "$nupkg"
    fi
    if [ ! -d "$NUGET/$name.$ver" ]; then
        mkdir -p "$NUGET/$name.$ver"
        unzip -qo "$nupkg" -d "$NUGET/$name.$ver"
    fi
done
for name in libpappl1_native libcups2_native libressl_native libpng_native zlib_native; do
    ver=${PKGS[$name]}
    rname="$name.redist"
    rver=${REDIST[$rname]:-$ver}
    nupkg="$NUGET/$rname.$rver.nupkg"
    if [ ! -f "$nupkg" ]; then
        echo "==> downloading $rname $rver"
        curl -sL "https://www.nuget.org/api/v2/package/$rname/$rver" -o "$nupkg" || true
    fi
    if [ -f "$nupkg" ] && [ ! -d "$NUGET/$rname.$rver" ]; then
        mkdir -p "$NUGET/$rname.$rver"
        unzip -qo "$nupkg" -d "$NUGET/$rname.$rver" 2>/dev/null || true
    fi
done

# --- Include paths ---
INC=""
for name in "${!PKGS[@]}"; do
    ver=${PKGS[$name]}
    INC="$INC -I$NUGET/$name.$ver/build/native/include"
done
INC="$INC -I$(cd /home/rick/src/pappl 2>/dev/null && pwd)/vcnet -Inative"
# fallback if pappl source not at ../pappl
if [ ! -d /home/rick/src/pappl/vcnet ]; then
    INC="$INC -Ivcnet"
fi

# --- Compile ---
echo "==> compiling (mingw-w64)"
$MINGW -O2 -DWIN32 -D_WINDOWS -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE \
    $INC -c pappl/phaser6125-pappl.c -o "$OBJ/phaser6125-pappl.o"
$MINGW -O2 -DWIN32 -Inative -c native/hbpl_core.c -o "$OBJ/hbpl_core.o"
$MINGW -O2 -DWIN32 -D_WINDOWS -Inative -c native/mdns_mini.c -o "$OBJ/mdns_mini.o"
$MINGW -O2 -DWIN32 -D_WINDOWS -Inative -c native/snmp_poll.c -o "$OBJ/snmp_poll.o"

# --- Link against NuGet .lib files (MSVC COFF — mingw ld handles these) ---
echo "==> linking"
LIBS=""
LIBS="$LIBS $NUGET/libpappl1_native.1.4.12/build/native/lib/x64/Release/libpappl1.lib"
LIBS="$LIBS $NUGET/libcups2_native.2.4.20/build/native/lib/x64/Release/libcups2.lib"
LIBS="$LIBS $NUGET/libressl_native.3.7.3.0/build/native/lib/x64/Release/ssl.lib"
LIBS="$LIBS $NUGET/libressl_native.3.7.3.0/build/native/lib/x64/Release/crypto.lib"
LIBS="$LIBS $NUGET/libressl_native.3.7.3.0/build/native/lib/x64/Release/tls.lib"
LIBS="$LIBS $NUGET/libpng_native.1.6.30/build/native/lib/x64/Release/libpng16.lib"
LIBS="$LIBS $NUGET/zlib_native.1.2.11/build/native/lib/x64/Release/zlib.lib"
LIBS="$LIBS $NUGET/libjpeg-turbo-v142.2.0.4.3/build/native/lib/x64/v142/Release/jpeg.lib"

$MINGW -O2 -o "$PREFIX/phaser6125-pappl.exe" \
    "$OBJ/phaser6125-pappl.o" "$OBJ/hbpl_core.o" "$OBJ/mdns_mini.o" "$OBJ/snmp_poll.o" \
    $LIBS -lws2_32 -ladvapi32 -lshell32 -luser32 -lcrypt32 -lsecur32

echo "==> OK: $PREFIX/phaser6125-pappl.exe ($(stat -c%s "$PREFIX/phaser6125-pappl.exe") bytes)"

# --- Assemble dist/ (exe + runtime DLLs) ---
echo "==> assembling $DIST/"
cp "$PREFIX/phaser6125-pappl.exe" "$DIST/"
cp "$NUGET/libpappl1_native.redist.1.4.12/build/native/bin/x64/Release/libpappl1.dll" "$DIST/" 2>/dev/null || true
cp "$NUGET/libcups2_native.redist.2.4.20/build/native/bin/x64/Release/libcups2.dll" "$DIST/" 2>/dev/null || true
cp "$NUGET/libcups2_native.redist.2.4.20/build/native/bin/x64/Release/regex.dll" "$DIST/" 2>/dev/null || true
cp "$NUGET/libressl_native.redist.3.7.3.0/build/native/bin/x64/Release/"*.dll "$DIST/" 2>/dev/null || true
cp "$NUGET/libpng_native.redist.1.6.30/build/native/bin/x64/Release/libpng16.dll" "$DIST/" 2>/dev/null || true
cp "$NUGET/zlib_native.redist.1.2.11/build/native/bin/x64/Release/zlib.dll" "$DIST/" 2>/dev/null || true
cp "$NUGET/libjpeg-turbo-v142.2.0.4.3/build/native/bin/x64/v142/Release/"*.dll "$DIST/" 2>/dev/null || true

echo "==> dist contents:"
ls "$DIST/"
echo
echo "To package:  cd $DIST && zip -r ../Phaser6125N-PAPPL.zip ."
echo "On Windows:  put gswin64c.exe next to the .exe, then run:"
echo "  phaser6125-pappl.exe server -o server-port=8631"
echo "  phaser6125-pappl.exe add -d Phaser6125 -m xerox_phaser_6125n -v socket://192.168.178.10:9100"
