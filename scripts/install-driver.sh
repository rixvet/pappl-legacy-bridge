#!/bin/sh
# Installeert de Xerox Phaser 6125N driver uit deze repo naar het systeem:
#   /usr/lib/cups/filter/            (FXM_* filters)
#   /usr/share/cups/model/Xerox/en/  (PPD)
#   /usr/share/cups/Xerox/dlut/      (kleur lookup table)
#
# Vereist root:  sudo bash scripts/install-driver.sh
#
# Let op: /usr/lib/cups/filter moet root:root 0755 zijn (CUPS weigert filters
# in een group/world-writable directory — zie troubleshooting op de notes-pagina).
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"

install -d -m 0755 /usr/lib/cups/filter
install -d -m 0755 /usr/share/cups/model/Xerox/en
install -d -m 0755 /usr/share/cups/Xerox/dlut

install -m 0755 "$REPO"/driver/filter/FXM_* /usr/lib/cups/filter/
install -m 0644 "$REPO"/driver/Phaser_6125N.dlut /usr/share/cups/Xerox/dlut/
install -m 0644 "$REPO"/driver/Phaser_6125N.ppd /usr/share/cups/model/Xerox/en/

# herstart CUPS zodat het nieuwe model opgepikt wordt
systemctl restart cups 2>/dev/null || service cups restart 2>/dev/null || true

echo "OK: driver geinstalleerd."
ls -la /usr/lib/cups/filter/FXM_* /usr/share/cups/model/Xerox/en/ /usr/share/cups/Xerox/dlut/
