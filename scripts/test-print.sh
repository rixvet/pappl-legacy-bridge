#!/bin/sh
# Stuurt een testpagina naar de Phaser 6125N via CUPS.
# Geen root nodig.
set -e
PRINTER=Phaser6125N
REPO="$(cd "$(dirname "$0")/.." && pwd)"

if [ -f "$REPO/tests/testpage.pdf" ]; then
    # PDF = de moderne pipeline (pdftopdf/pdftops -> FXM_PF -> HBPL)
    lp -d "$PRINTER" "$REPO/tests/testpage.pdf"
elif [ -f /usr/share/cups/data/default-testpage.pdf ]; then
    lp -d "$PRINTER" /usr/share/cups/data/default-testpage.pdf
elif [ -f /usr/share/cups/data/testprint ]; then
    lp -d "$PRINTER" /usr/share/cups/data/testprint
else
    lp -d "$PRINTER" "$REPO/tests/testpage.ps"
fi

echo "Testpagina verstuurd naar '$PRINTER'."
echo "Check: lpstat -W all -o $PRINTER   en   tail /var/log/cups/error_log"
