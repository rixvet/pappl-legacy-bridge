#!/bin/sh
# print-direct.sh — print WITHOUT CUPS: native filter -> raw socket (port 9100).
#
# Fallback for when CUPS ever drops PPD-based drivers (CUPS 3.x):
# the filter is a standalone binary that puts a complete HBPL/PJL stream on
# stdout, so CUPS is not strictly needed.
#
# Usage:   scripts/print-direct.sh <file.pdf|ps> ["options"]
# Example: scripts/print-direct.sh tests/colorpage.pdf
# Env:       PRINTER_IP=192.168.178.10   (default)
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
IP="${PRINTER_IP:-192.168.178.10}"
FILE="${1:?usage: print-direct.sh <file> [\"options\"]}"
OPTS="${2:-PageSize=A4 FXOutputMode=600x600 FXColorMode=Color}"

# build the filter if it is not there yet
if [ ! -x "$REPO/native/foo2sq21hbpl" ]; then
    cc -O2 -Wall -Wextra -o "$REPO/native/foo2sq21hbpl" "$REPO/native/foo2sq21hbpl.c" "$REPO/native/hbpl_core.c" -lm
fi

PPD="$REPO/driver/Phaser_6125N-native.ppd" \
"$REPO/native/foo2sq21hbpl" 1 "${USER:-user}" "$(basename "$FILE")" 1 "$OPTS" "$FILE" \
    | nc -w 5 "$IP" 9100
echo "Sent (direct, without CUPS) to $IP:9100: $FILE"
