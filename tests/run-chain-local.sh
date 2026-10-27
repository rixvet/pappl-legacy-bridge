#!/bin/sh
# Lokale test van de complete FXM filter-chain ZONDER CUPS en zonder root.
#
# De filters zijn 32-bit i386 ELF's met interpreter /lib/ld-linux.so.2.
# Deze script gebruikt een user-level sysroot (standaard /home/rick/sysroot-i386)
# met de 32-bit libc + libcups, en wrappers die de binaries via de sysroot-loader
# starten. De PPD wordt gep patcht naar lokale paden.
#
# De sysroot is gebouwd met (geen root nodig):
#   mkdir -p /home/rick/apt/{etc,lists/partial,cache/archives/partial}
#   printf 'deb http://archive.ubuntu.com/ubuntu jammy main\ndeb http://archive.ubuntu.com/ubuntu jammy-updates main\ndeb http://archive.ubuntu.com/ubuntu jammy-security main\n' > /home/rick/apt/etc/sources.list
#   apt-get -o Dir::Etc::sourcelist=/home/rick/apt/etc/sources.list -o Dir::Etc::sourceparts=- \
#     -o Dir::State::lists=/home/rick/apt/lists -o Dir::Cache::archives=/home/rick/apt/cache/archives \
#     -o APT::Get::List-Cleanup=0 -o APT::Architecture=amd64 -o APT::Architectures::=amd64 \
#     -o APT::Architectures::=i386 -o Debug::NoLocking=1 update
#   apt-get <zelfde -o opties> --download-only --yes --no-install-recommends install libc6:i386 libgcc-s1:i386 libcups2:i386
#   for d in /home/rick/apt/cache/archives/*.deb; do dpkg-deb -x "$d" /home/rick/sysroot-i386; done
#
# Gebruik:  tests/run-chain-local.sh [input.ps] [opties-string]
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$REPO/tests/build"
SYSROOT="${SYSROOT:-/home/rick/sysroot-i386}"
LOADER="$SYSROOT/lib/ld-linux.so.2"
LIBPATH="$SYSROOT/usr/lib/i386-linux-gnu:$SYSROOT/lib/i386-linux-gnu"

if [ ! -x "$LOADER" ]; then
    echo "FOUT: sysroot-loader niet gevonden: $LOADER" >&2
    echo "Bouw de sysroot eerst (zie commentaar bovenaan deze script)." >&2
    exit 1
fi

IN="${1:-$REPO/tests/testpage.ps}"
OPTS="${2:-PageSize=A4 FXOutputMode=600x600 FXColorMode=Color InstalledMemory=64Meg Option1=1Tray-S FXInputSlot=AutoSelect}"
OUT="$BUILD/out.hbpl"

rm -rf "$BUILD"
mkdir -p "$BUILD/filter-bin/real"
cp "$REPO"/driver/filter/FXM_PF "$REPO"/driver/filter/FXM_MF \
   "$REPO"/driver/filter/FXM_PM2FXR "$REPO"/driver/filter/FXM_SBP \
   "$REPO"/driver/filter/FXM_PR "$REPO"/driver/filter/FXM_CC \
   "$REPO"/driver/filter/FXM_ALC "$REPO"/driver/filter/FXM_HBPL \
   "$BUILD/filter-bin/real/"
# De gepatchte PS2PM uit driver/ is een puur shell script (draait native)
cp "$REPO/driver/filter/FXM_PS2PM" "$BUILD/filter-bin/FXM_PS2PM"
chmod +x "$BUILD/filter-bin/FXM_PS2PM"
cp "$REPO/driver/Phaser_6125N.dlut" "$BUILD/"

# wrappers voor de 32-bit binaries (FXM_MF + de chain filters)
for f in FXM_PF FXM_MF FXM_PM2FXR FXM_SBP FXM_PR FXM_CC FXM_ALC FXM_HBPL; do
    printf '#!/bin/sh\nexec %s --library-path %s %s/filter-bin/real/%s "$@"\n' \
        "$LOADER" "$LIBPATH" "$BUILD" "$f" > "$BUILD/filter-bin/$f"
    chmod +x "$BUILD/filter-bin/$f"
done

# test-PPD met lokale paden
sed -e "s|/usr/lib/cups/filter/FXM_PF|$BUILD/filter-bin/FXM_PF|" \
    -e "s|/usr/lib/cups/filter/FXM_MF|$BUILD/filter-bin/FXM_MF|" \
    -e "s|/usr/lib/cups/filter|$BUILD/filter-bin|" \
    -e "s|/usr/share/cups/Xerox/dlut/Phaser_6125N.dlut|$BUILD/Phaser_6125N.dlut|" \
    "$REPO/driver/Phaser_6125N.ppd" > "$BUILD/Phaser_6125N.ppd"

# draai de chain: PS -> (gs/ppmraw) -> FXRaster -> HBPL
PPD="$BUILD/Phaser_6125N.ppd" CUPS_FONTPATH=/usr/share/fonts \
    "$BUILD/filter-bin/FXM_PF" 1 rick "Lokale test" 1 "$OPTS" "$IN" \
    > "$OUT" 2> "$BUILD/stderr.log"

echo "HBPL output: $OUT ($(stat -c%s "$OUT") bytes)"

# raster-dimensies uit de HBPL header halen (attr 0x9a 0xc4 = width/height als 2x uint32 LE)
off=$(grep -abo "ENTER LANGUAGE=HBPL" "$OUT" | head -1 | cut -d: -f1)
dd if="$OUT" bs=1 skip=$((off + 22)) count=256 2>/dev/null > "$BUILD/hdr.bin"
pat=$(printf '\232\304')   # bytes 0x9a 0xc4 (HBPL attr width/height)
poff=$(grep -abo "$pat" "$BUILD/hdr.bin" | head -1 | cut -d: -f1)
if [ -n "$poff" ]; then
    w=$(od -A n -t u4 -j $((poff + 2)) -N 4 "$BUILD/hdr.bin" | tr -d ' ')
    h=$(od -A n -t u4 -j $((poff + 6)) -N 4 "$BUILD/hdr.bin" | tr -d ' ')
    echo "Raster: ${w} x ${h} px"
    echo "(A4 imageable area @600dpi = 4776 x 6829, @300dpi = 2388 x 3415, @72dpi = 600 x 842)"
fi
echo "Structuur: $(strings -n 4 "$OUT" | grep -c '@PJL') PJL-commando's, EOJ: $(tail -c 20 "$OUT" | grep -c 'EOJ')"
