#!/bin/bash
# build-pappl-app.sh — build the PAPPL printer application for the
# Xerox Phaser 6125N / Fuji Xerox DocuPrint C525A.
#
# Requires: PAPPL 1.4.x dev files (libpappl), gcc/clang, libcups dev.
# On Ubuntu/WSL:  wsl.exe -d Ubuntu -u root -- apt install libcups2-dev libavahi-client-dev
# PAPPL itself:   build from source (https://github.com/michaelrsweet/pappl, tag v1.4.x)
#                 ./configure && make && sudo make install
#
# Output: pappl/phaser6125-pappl (binary)
set -euo pipefail

cd "$(dirname "$0")/.."

CC="${CC:-cc}"
CFLAGS="-O2 -Wall -Wextra"

echo "==> building pappl/phaser6125-pappl ($CC)"
# shellcheck disable=SC2086
$CC $CFLAGS -o pappl/phaser6125-pappl \
    pappl/phaser6125-pappl.c \
    native/hbpl_core.c \
    -I native \
    $(pkg-config --cflags --libs pappl) \
    -lm

echo "==> OK: $(ls -la pappl/phaser6125-pappl)"
echo
echo "Run the IPP server:"
echo "  LD_LIBRARY_PATH=/usr/local/lib pappl/phaser6125-pappl server -o server-port=8631"
echo "Add the printer:"
echo "  LD_LIBRARY_PATH=/usr/local/lib pappl/phaser6125-pappl add -d phaser -m xerox_phaser_6125n -v socket://192.168.178.10:9100"
echo "Print a test file:"
echo "  LD_LIBRARY_PATH=/usr/local/lib pappl/phaser6125-pappl submit -d phaser tests/colorpage.pdf"
