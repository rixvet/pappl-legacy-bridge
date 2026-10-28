#!/bin/sh
# ippeve-wrapper.sh — CUPS "command" filter for ippeveprinter / ippserver
# (the CUPS 3.x / driverless route; see RE-NOTES.md §8).
#
# ippeveprinter (CUPS 2.4) / ippserver (CUPS 3.x) calls this wrapper per
# job: the document (PDF/PS) arrives on stdin, the IPP_* env vars contain
# the job attributes.  The wrapper calls the native filter and writes the
# complete PJL/HBPL stream to stdout — ippeveprinter sends it to the
# -D device-uri (here: socket://192.168.178.10:9100).
#
# Install as CUPS command filter:
#   sudo install -m 0755 scripts/ippeve-wrapper.sh \
#       /usr/lib/cups/command/foo2sq21hbpl-ippeve
# Run as IPP Everywhere service:
#   ippeveprinter -m "Phaser 6125N" -M "Xerox" -l "Home" \
#       -D socket://192.168.178.10:9100 \
#       -f application/pdf,application/postscript \
#       -s 10,10 \
#       -c /usr/lib/cups/command/foo2sq21hbpl-ippeve \
#       "Phaser6125N"
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
FILTER="$REPO/native/foo2sq21hbpl"
PPD="$REPO/driver/Phaser_6125N-native.ppd"

# media: IPP_MEDIA (e.g. iso_a4_210x297mm) -> PageSize; default A4
PAGESIZE=A4
case "${IPP_MEDIA:-}" in
    *letter*) PAGESIZE=Letter ;;
    *legal*)  PAGESIZE=Legal ;;
    *a5*)     PAGESIZE=A5 ;;
    *)        PAGESIZE=A4 ;;
esac

# build the filter if it is not there yet
if [ ! -x "$FILTER" ]; then
    cc -O2 -Wall -Wextra -o "$FILTER" "$REPO/native/foo2sq21hbpl.c" -lm
fi

# document from stdin to a temp file (the filter reads from a file)
TMP="$(mktemp /tmp/ippeve-XXXXXX)"
trap 'rm -f "$TMP"' EXIT INT TERM
cat > "$TMP"

# the filter writes the HBPL stream to stdout; ippeveprinter sends it
# on to the -D device-uri
PPD="$PPD" "$FILTER" 1 "${IPP_USER:-user}" "${IPP_JOB_NAME:-job}" 1 \
    "PageSize=$PAGESIZE FXOutputMode=600x600 FXColorMode=Color" "$TMP"
