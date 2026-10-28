#!/bin/sh
# ippeve-wrapper.sh — CUPS "command" filter voor ippeveprinter / ippserver
# (de CUPS 3.x / driverless route; zie RE-NOTES.md §8).
#
# ippeveprinter (CUPS 2.4) / ippserver (CUPS 3.x) roept deze wrapper per job
# aan: het document (PDF/PS) komt op stdin binnen, de IPP_* env vars bevatten
# de job attributes.  De wrapper roept de native filter aan en schrijft de
# complete PJL/HBPL stream naar stdout — ippeveprinter stuurt die naar de
# -D device-uri (hier: socket://192.168.178.10:9100).
#
# Installeren als CUPS command filter:
#   sudo install -m 0755 scripts/ippeve-wrapper.sh \
#       /usr/lib/cups/command/foo2sq21hbpl-ippeve
# Draaien als IPP Everywhere service:
#   ippeveprinter -m "Phaser 6125N" -M "Xerox" -l "Thuis" \
#       -D socket://192.168.178.10:9100 \
#       -f application/pdf,application/postscript \
#       -s 10,10 \
#       -c /usr/lib/cups/command/foo2sq21hbpl-ippeve \
#       "Phaser6125N"
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
FILTER="$REPO/native/foo2sq21hbpl"
PPD="$REPO/driver/Phaser_6125N-native.ppd"

# media: IPP_MEDIA (bijv. iso_a4_210x297mm) -> PageSize; default A4
PAGESIZE=A4
case "${IPP_MEDIA:-}" in
    *letter*) PAGESIZE=Letter ;;
    *legal*)  PAGESIZE=Legal ;;
    *a5*)     PAGESIZE=A5 ;;
    *)        PAGESIZE=A4 ;;
esac

# filter bouwen als die er nog niet is
if [ ! -x "$FILTER" ]; then
    cc -O2 -Wall -Wextra -o "$FILTER" "$REPO/native/foo2sq21hbpl.c" -lm
fi

# document van stdin naar een temp file (de filter leest vanaf een bestand)
TMP="$(mktemp /tmp/ippeve-XXXXXX)"
trap 'rm -f "$TMP"' EXIT INT TERM
cat > "$TMP"

# de filter schrijft de HBPL stream naar stdout; ippeveprinter stuurt die
# door naar de -D device-uri
PPD="$PPD" "$FILTER" 1 "${IPP_USER:-user}" "${IPP_JOB_NAME:-job}" 1 \
    "PageSize=$PAGESIZE FXOutputMode=600x600 FXColorMode=Color" "$TMP"
