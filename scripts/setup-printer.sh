#!/bin/sh
# Creates the CUPS queue for the Xerox Phaser 6125N at 192.168.178.10.
# The printer speaks HBPL (via PJL) and is tested over raw JetDirect (port 9100).
#
# Requires root:  sudo bash scripts/setup-printer.sh
set -e
PRINTER=Phaser6125N
PPD=/usr/share/cups/model/Xerox/en/Phaser_6125N.ppd

# create queue (or update if it already exists)
lpadmin -p "$PRINTER" -E -v socket://192.168.178.10:9100 -P "$PPD" \
    -D "Xerox Phaser 6125N" -L "Home"
cupsenable "$PRINTER"
cupsaccept "$PRINTER"

# default options (A4, 600dpi, colour, 250-sheet feeder tray)
lpoptions -p "$PRINTER" \
    -o PageSize=A4 -o FXOutputMode=600x600 -o FXColorMode=Color \
    -o Option1=1Tray-S -o InstalledMemory=64Meg

echo "OK: queue '$PRINTER' created -> socket://192.168.178.10:9100"
lpstat -p "$PRINTER" -l 2>/dev/null || true
