#!/bin/sh
# Stuurt een testpagina naar de Phaser 6125N via CUPS (native driver).
# Geen root nodig. Met een argument: lp -d Phaser6125N <jouw-file>
set -e
PRINTER=Phaser6125N
REPO="$(cd "$(dirname "$0")/.." && pwd)"

if [ $# -ge 1 ]; then
    lp -d "$PRINTER" "$1"
elif [ -f "$REPO/tests/colorpage.pdf" ]; then
    # full colour/image torture page: titel, gekleurde tekst, meahow, kleurbalken,
    # gradient, noise, schaakbord, hue-lijnen
    lp -d "$PRINTER" "$REPO/tests/colorpage.pdf"
elif [ -f "$REPO/tests/colorpage.ps" ]; then
    lp -d "$PRINTER" "$REPO/tests/colorpage.ps"
elif [ -f "$REPO/tests/testpage.pdf" ]; then
    lp -d "$PRINTER" "$REPO/tests/testpage.pdf"
else
    lp -d "$PRINTER" "$REPO/tests/testpage.ps"
fi

echo "Testpagina verstuurd naar '$PRINTER'."
echo "Check: lpstat -W all -o $PRINTER   en   tail /var/log/cups/error_log"
