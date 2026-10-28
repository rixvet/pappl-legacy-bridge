#!/bin/sh
# setup-wsl-ipp.sh — maak de WSL CUPS queue bereikbaar als driverless IPP
# printer voor Windows 11 (inbox class driver; geen V3/V4 driver nodig).
#
# Context: Microsoft bouwt legacy V3/V4 printerdrivers af (end-of-servicing
# plan). Printers zonder IPP/Mopria — zoals de Phaser 6125N — vallen op
# termijn terug op handmatig geinstalleerde drivers. Onze WSL CUPS + de
# native filter (foo2sq21hbpl) zijn al de userspace-vertaling; dit script
# opent alleen de IPP endpoint voor Windows (RE-NOTES.md §11).
#
# Vereist root:  sudo bash scripts/setup-wsl-ipp.sh
set -e
CONF=/etc/cups/cupsd.conf
QUEUE=Phaser6125N

# 1. CUPS moet draaien
lpstat -r >/dev/null 2>&1 || { echo "ERROR: CUPS scheduler draait niet (service cups start?)"; exit 1; }

# 2. Listen *:631 toevoegen (idempotent; backup eerst)
if ! grep -qE '^Listen +\*:631' "$CONF"; then
    cp -a "$CONF" "$CONF.pre-ipp"
    sed -i 's/^Listen localhost:631$/Listen localhost:631\nListen *:631/' "$CONF"
    echo "==> $CONF: 'Listen *:631' toegevoegd (backup: $CONF.pre-ipp)"
else
    echo "==> $CONF: 'Listen *:631' stond er al"
fi

# 3. herstart CUPS
systemctl restart cups 2>/dev/null || service cups restart
sleep 1

# 4. test de IPP endpoint via het WSL IP (zoals Windows die ziet)
IP=$(hostname -I | awk '{print $1}')
echo "==> WSL IP: $IP"
ipptool -tv "ipp://$IP/printers/$QUEUE" /usr/share/cups/ipptool/get-printer-attributes.test \
    2>&1 | tail -2 || echo "WAARSCHUWING: ipptool test via $IP gefaald (firewall?)"

# 5. Windows-side instructies
echo ""
echo "==> Windows (WINNIE): printer toevoegen, driverless:"
echo "    Instellingen -> Bluetooth & andere apparaten -> Printers & scanners"
echo "    -> 'Apparaat toevoegen' -> 'De printer die ik wil staat er niet bij'"
echo "    -> 'Een gedeelde printer op naam selecteren' ->"
echo "       http://$IP:631/printers/$QUEUE"
echo "    Windows detecteert IPP en gebruikt de inbox class driver (geen V3)."
echo ""
echo "    Komt Windows er niet bij? Windows Firewall blokkeert inkomend naar"
echo "    WSL soms — voeg een regel toe voor poort 631, of gebruik WSL2"
echo "    mirrored networking ([wsl2] networkingMode=mirrored in"
echo "    %UserProfile%\\.wslconfig, daarna: wsl --shutdown)."
echo ""
echo "==> Test vanuit Windows: testpagina -> CUPS (WSL) -> foo2sq21hbpl"
echo "    -> HBPL -> printer (socket 9100; USB kan via RE-NOTES.md §10)."
