#!/bin/sh
# Maakt de CUPS-queue aan voor de Xerox Phaser 6125N op 192.168.178.10.
# De printer spricht HBPL (via PJL) en is getest over raw JetDirect (poort 9100).
#
# Vereist root:  sudo bash scripts/setup-printer.sh
set -e
PRINTER=Phaser6125N
PPD=/usr/share/cups/model/Xerox/en/Phaser_6125N.ppd

# queue aanmaken (of bijwerken als hij al bestaat)
lpadmin -p "$PRINTER" -E -v socket://192.168.178.10:9100 -P "$PPD" \
    -D "Xerox Phaser 6125N" -L "Thuis"
cupsenable "$PRINTER"
cupsaccept "$PRINTER"

# standaard opties (A4, 600dpi, kleur, 250-sheet feeder tray)
lpoptions -p "$PRINTER" \
    -o PageSize=A4 -o FXOutputMode=600x600 -o FXColorMode=Color \
    -o Option1=1Tray-S -o InstalledMemory=64Meg

echo "OK: queue '$PRINTER' aangemaakt -> socket://192.168.178.10:9100"
lpstat -p "$PRINTER" -l 2>/dev/null || true
