#!/bin/sh
# Installs the Xerox Phaser 6125N driver from this repo to the system:
#   /usr/lib/cups/filter/            (FXM_* filters)
#   /usr/share/cups/model/Xerox/en/  (PPD)
#   /usr/share/cups/Xerox/dlut/      (colour lookup table)
#
# Requires root:  sudo bash scripts/install-driver.sh
#
# Note: /usr/lib/cups/filter must be root:root 0755 (CUPS refuses filters
# in a group/world-writable directory — see troubleshooting on the notes page).
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"

install -d -m 0755 /usr/lib/cups/filter
install -d -m 0755 /usr/share/cups/model/Xerox/en
install -d -m 0755 /usr/share/cups/Xerox/dlut

install -m 0755 "$REPO"/driver/filter/FXM_* /usr/lib/cups/filter/
install -m 0644 "$REPO"/driver/Phaser_6125N.dlut /usr/share/cups/Xerox/dlut/
install -m 0644 "$REPO"/driver/Phaser_6125N.ppd /usr/share/cups/model/Xerox/en/

# restart CUPS so the new model is picked up
systemctl restart cups 2>/dev/null || service cups restart 2>/dev/null || true

echo "OK: driver installed."
ls -la /usr/lib/cups/filter/FXM_* /usr/share/cups/model/Xerox/en/ /usr/share/cups/Xerox/dlut/
