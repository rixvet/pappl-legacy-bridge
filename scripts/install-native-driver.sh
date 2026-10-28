#!/bin/sh
# Installeert de NATIVE Xerox Phaser 6125N driver (foo2sq21hbpl) naar:
#   /usr/lib/cups/filter/foo2sq21hbpl              (64-bit ELF, gebouwd uit native/foo2sq21hbpl.c)
#   /usr/share/cups/model/Xerox/en/Phaser_6125N-native.ppd
# en maakt de CUPS-queue 'Phaser6125N' aan op socket://192.168.178.10:9100.
#
# Vereist root:  sudo bash scripts/install-native-driver.sh
#
# Let op: /usr/lib/cups/filter moet root:root 0755 zijn (CUPS weigert filters
# in een group/world-writable directory).
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
PRINTER=Phaser6125N

echo "==> filter bouwen (gcc, native 64-bit)"
cc -O2 -Wall -Wextra -o /tmp/foo2sq21hbpl "$REPO/native/foo2sq21hbpl.c" -lm
file /tmp/foo2sq21hbpl

install -d -m 0755 /usr/lib/cups/filter
install -d -m 0755 /usr/share/cups/model/Xerox/en

install -m 0755 /tmp/foo2sq21hbpl /usr/lib/cups/filter/foo2sq21hbpl
install -m 0644 "$REPO/driver/Phaser_6125N-native.ppd" /usr/share/cups/model/Xerox/en/

echo "==> queue aanmaken/bijwerken"
lpadmin -p "$PRINTER" -E -v socket://192.168.178.10:9100 \
    -P /usr/share/cups/model/Xerox/en/Phaser_6125N-native.ppd \
    -D "Xerox Phaser 6125N (native)" -L "Thuis"
cupsenable "$PRINTER"
cupsaccept "$PRINTER"

# standaard opties (A4, 600dpi, kleur)
lpoptions -p "$PRINTER" \
    -o PageSize=A4 -o FXOutputMode=600x600 -o FXColorMode=Color

# herstart CUPS zodat het nieuwe model opgepikt wordt
systemctl restart cups 2>/dev/null || service cups restart 2>/dev/null || true

echo "OK: native driver geinstalleerd."
ls -la /usr/lib/cups/filter/foo2sq21hbpl /usr/share/cups/model/Xerox/en/Phaser_6125N-native.ppd
lpstat -p "$PRINTER" -l 2>/dev/null || true
