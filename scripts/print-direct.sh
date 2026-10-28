#!/bin/sh
# print-direct.sh — print ZONDER CUPS: native filter -> raw socket (poort 9100).
#
# Fallback voor als CUPS ooit PPD-gebaseerde drivers laat vallen (CUPS 3.x):
# de filter is een standalone binary die een complete HBPL/PJL stream op
# stdout zet, dus CUPS is niet strikt nodig.
#
# Gebruik:   scripts/print-direct.sh <bestand.pdf|ps> ["options"]
# Voorbeeld: scripts/print-direct.sh tests/colorpage.pdf
# Env:       PRINTER_IP=192.168.178.10   (default)
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
IP="${PRINTER_IP:-192.168.178.10}"
FILE="${1:?gebruik: print-direct.sh <bestand> [\"options\"]}"
OPTS="${2:-PageSize=A4 FXOutputMode=600x600 FXColorMode=Color}"

# build de filter als die er nog niet is
if [ ! -x "$REPO/native/foo2sq21hbpl" ]; then
    cc -O2 -Wall -Wextra -o "$REPO/native/foo2sq21hbpl" "$REPO/native/foo2sq21hbpl.c" -lm
fi

PPD="$REPO/driver/Phaser_6125N-native.ppd" \
"$REPO/native/foo2sq21hbpl" 1 "${USER:-user}" "$(basename "$FILE")" 1 "$OPTS" "$FILE" \
    | nc -w 5 "$IP" 9100
echo "Verstuurd (direct, zonder CUPS) naar $IP:9100: $FILE"
