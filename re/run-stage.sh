#!/bin/sh
# run-stage.sh STAGE "options" input output
# Draait één 32-bit FXM filter stage via de sysroot-loader, data via stdin/stdout.
STAGE=$1; OPTS=$2; IN=$3; OUT=$4
REPO=/home/rick/src/phaser6125
SYSROOT=/home/rick/sysroot-i386
LOADER=$SYSROOT/lib/ld-linux.so.2
LIBPATH=$SYSROOT/usr/lib/i386-linux-gnu:$SYSROOT/lib/i386-linux-gnu
FILTER=$REPO/analysis/tarball/usr/lib/cups/filter/$STAGE
PPD=$REPO/re/build/Phaser_6125N.ppd CUPS_FONTPATH=/usr/share/fonts \
  "$LOADER" --library-path "$LIBPATH" "$FILTER" 1 rick test 1 "$OPTS" \
  < "$IN" > "$OUT" 2> "$OUT.stderr"
echo "$STAGE: $(stat -c%s "$OUT") bytes out, exit=$?"
