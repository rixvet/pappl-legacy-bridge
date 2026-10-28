#!/bin/sh
# Installeert de NATIVE Xerox Phaser 6125N driver (foo2sq21hbpl) naar:
#   /usr/lib/cups/filter/foo2sq21hbpl              (64-bit ELF, gebouwd uit native/foo2sq21hbpl.c)
#   /usr/share/cups/model/Xerox/en/Phaser_6125N-native.ppd
#   /usr/share/phaser6125/Phaser_6125N.dlut       (kleurcalibratie: FXM_CC CST + 3D LUT,
#                                                   opgezocht door de filter; zonder dit
#                                                   bestand valt de filter terug op CST-only)
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
if command -v clang >/dev/null 2>&1; then
    echo "==> clang -Weverything -Werror sanity check (rapport, niet fataal;"
    echo "    -Wno-unsafe-buffer-usage = experimentele C++ lint, zie RE-NOTES §6)"
    if clang -Weverything -Werror -Wno-unsafe-buffer-usage -Wno-unknown-warning-option \
            -O2 -o /tmp/foo2sq21hbpl-clang "$REPO/native/foo2sq21hbpl.c" -lm; then
        file /tmp/foo2sq21hbpl-clang
    else
        echo "LET OP: clang sanity check gefaald — de gcc build hierboven is leidend."
    fi
fi

install -d -m 0755 /usr/lib/cups/filter
install -d -m 0755 /usr/share/cups/model/Xerox/en
install -d -m 0755 /usr/share/phaser6125

install -m 0755 /tmp/foo2sq21hbpl /usr/lib/cups/filter/foo2sq21hbpl
install -m 0644 "$REPO/driver/Phaser_6125N-native.ppd" /usr/share/cups/model/Xerox/en/
install -m 0644 "$REPO/driver/Phaser_6125N.dlut" /usr/share/phaser6125/

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
ls -la /usr/lib/cups/filter/foo2sq21hbpl \
      /usr/share/cups/model/Xerox/en/Phaser_6125N-native.ppd \
      /usr/share/phaser6125/Phaser_6125N.dlut
lpstat -p "$PRINTER" -l 2>/dev/null || true
