#!/bin/sh
# print-direct-usb.sh — print WITHOUT CUPS: native filter -> USB device (e.g. /dev/usb/lp0).
#
# The Phaser 6125N presents itself over USB as a standard USB Printing Class
# device (USBPRINT\FUJI_XEROXDocuPrint_13C9 — see the Windows driver INF
# fxhl2zi.inf), so no vendor driver is needed: the PJL/HBPL stream goes
# straight to the device.  Same stream as print-direct.sh, different sink.
#
# On native Linux the device is /dev/usb/lp0 (usblp kernel module).
# Under WSL2 /dev/usb/lp0 does not exist (usbipd exposes the device via
# libusb) — use the CUPS usb:// backend there instead (see RE-NOTES.md §10).
#
# Usage:   scripts/print-direct-usb.sh <file.pdf|ps> ["options"]
# Example: scripts/print-direct-usb.sh tests/colorpage.pdf
# Env:       USB_DEVICE=/dev/usb/lp0   (default)
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DEV="${USB_DEVICE:-/dev/usb/lp0}"
FILE="${1:?usage: print-direct-usb.sh <file> [\"options\"]}"
OPTS="${2:-PageSize=A4 FXOutputMode=600x600 FXColorMode=Color}"

# build the filter if it is not there yet
if [ ! -x "$REPO/native/foo2sq21hbpl" ]; then
    cc -O2 -Wall -Wextra -o "$REPO/native/foo2sq21hbpl" "$REPO/native/foo2sq21hbpl.c" -lm
fi

if [ ! -c "$DEV" ]; then
    echo "ERROR: $DEV is not a character device (missing? usblp module not loaded?)" >&2
    echo "On WSL2 use the CUPS usb:// backend instead (see RE-NOTES.md §10)." >&2
    exit 1
fi
if [ ! -w "$DEV" ]; then
    echo "ERROR: no write permission on $DEV (need root or the lp group?)" >&2
    exit 1
fi

PPD="$REPO/driver/Phaser_6125N-native.ppd" \
"$REPO/native/foo2sq21hbpl" 1 "${USER:-user}" "$(basename "$FILE")" 1 "$OPTS" "$FILE" \
    > "$DEV"
echo "Sent (direct USB, without CUPS) to $DEV: $FILE"
