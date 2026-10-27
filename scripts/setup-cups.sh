#!/bin/sh
# Installeert CUPS + de 32-bit libraries die de oude Fuji Xerox filters nodig
# hebben (ze zijn 32-bit i386 ELF's uit 2005-2012, met interpreter
# /lib/ld-linux.so.2 en afhankelijk van een 32-bit libcups.so.2).
#
# Vereist root:  sudo bash scripts/setup-cups.sh
set -e

dpkg --add-architecture i386
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y \
    cups cups-client cups-filters ghostscript \
    libc6-i386 libcups2:i386

systemctl enable --now cups 2>/dev/null || service cups start

echo "OK: CUPS + 32-bit libs geinstalleerd."
cupsd -v 2>/dev/null || true
