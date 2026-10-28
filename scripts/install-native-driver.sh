#!/bin/sh
# Installs the NATIVE Xerox Phaser 6125N driver (foo2sq21hbpl) to:
#   /usr/lib/cups/filter/foo2sq21hbpl              (64-bit ELF, built from native/foo2sq21hbpl.c)
#   /usr/share/cups/model/Xerox/en/Phaser_6125N-native.ppd
#   /usr/share/phaser6125/Phaser_6125N.dlut       (colour calibration: FXM_CC CST + 3D LUT,
#                                                   looked up by the filter; without this
#                                                   file the filter falls back to CST-only)
# and creates the CUPS queue 'Phaser6125N' on socket://192.168.178.10:9100.
#
# Requires root:  sudo bash scripts/install-native-driver.sh
#
# Note: /usr/lib/cups/filter must be root:root 0755 (CUPS refuses filters
# in a group/world-writable directory).
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
PRINTER=Phaser6125N

echo "==> building filter (gcc, native 64-bit)"
cc -O2 -Wall -Wextra -o /tmp/foo2sq21hbpl "$REPO/native/foo2sq21hbpl.c" -lm
file /tmp/foo2sq21hbpl
if command -v clang >/dev/null 2>&1; then
    echo "==> clang -Weverything -Werror sanity check (report, not fatal;"
    echo "    -Wno-unsafe-buffer-usage = experimental C++ lint, see RE-NOTES §6)"
    if clang -Weverything -Werror -Wno-unsafe-buffer-usage -Wno-unknown-warning-option \
            -O2 -o /tmp/foo2sq21hbpl-clang "$REPO/native/foo2sq21hbpl.c" -lm; then
        file /tmp/foo2sq21hbpl-clang
    else
        echo "NOTE: clang sanity check failed — the gcc build above is authoritative."
    fi
fi

install -d -m 0755 /usr/lib/cups/filter
install -d -m 0755 /usr/share/cups/model/Xerox/en
install -d -m 0755 /usr/share/phaser6125

install -m 0755 /tmp/foo2sq21hbpl /usr/lib/cups/filter/foo2sq21hbpl
install -m 0644 "$REPO/driver/Phaser_6125N-native.ppd" /usr/share/cups/model/Xerox/en/
install -m 0644 "$REPO/driver/Phaser_6125N.dlut" /usr/share/phaser6125/

echo "==> creating/updating queue"
lpadmin -p "$PRINTER" -E -v socket://192.168.178.10:9100 \
    -P /usr/share/cups/model/Xerox/en/Phaser_6125N-native.ppd \
    -D "Xerox Phaser 6125N (native)" -L "Home"
cupsenable "$PRINTER"
cupsaccept "$PRINTER"

# default options (A4, 600dpi, colour)
lpoptions -p "$PRINTER" \
    -o PageSize=A4 -o FXOutputMode=600x600 -o FXColorMode=Color

# restart CUPS so the new model is picked up
systemctl restart cups 2>/dev/null || service cups restart 2>/dev/null || true

echo "OK: native driver installed."
ls -la /usr/lib/cups/filter/foo2sq21hbpl \
      /usr/share/cups/model/Xerox/en/Phaser_6125N-native.ppd \
      /usr/share/phaser6125/Phaser_6125N.dlut
lpstat -p "$PRINTER" -l 2>/dev/null || true
