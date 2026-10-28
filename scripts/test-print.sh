#!/bin/sh
# Sends a test page to the Phaser 6125N via CUPS (native driver).
# No root needed. With an argument: lp -d Phaser6125N <your-file>
set -e
PRINTER=Phaser6125N
REPO="$(cd "$(dirname "$0")/.." && pwd)"

if [ $# -ge 1 ]; then
    lp -d "$PRINTER" "$1"
elif [ -f "$REPO/tests/colorpage.pdf" ]; then
    # full colour/image torture page: title, coloured text, meahow, colour bars,
    # gradient, noise, checkerboard, hue lines
    lp -d "$PRINTER" "$REPO/tests/colorpage.pdf"
elif [ -f "$REPO/tests/colorpage.ps" ]; then
    lp -d "$PRINTER" "$REPO/tests/colorpage.ps"
elif [ -f "$REPO/tests/testpage.pdf" ]; then
    lp -d "$PRINTER" "$REPO/tests/testpage.pdf"
else
    lp -d "$PRINTER" "$REPO/tests/testpage.ps"
fi

echo "Test page sent to '$PRINTER'."
echo "Check: lpstat -W all -o $PRINTER   and   tail /var/log/cups/error_log"
