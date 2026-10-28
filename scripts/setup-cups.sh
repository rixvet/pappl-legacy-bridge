#!/bin/sh
# Installs CUPS + the 32-bit libraries the old Fuji Xerox filters need
# (they are 32-bit i386 ELFs from 2005-2012, with interpreter
# /lib/ld-linux.so.2 and dependent on a 32-bit libcups.so.2).
#
# Requires root:  sudo bash scripts/setup-cups.sh
set -e

dpkg --add-architecture i386
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y \
    cups cups-client cups-filters ghostscript \
    libc6-i386 libcups2:i386

systemctl enable --now cups 2>/dev/null || service cups start

echo "OK: CUPS + 32-bit libs installed."
cupsd -v 2>/dev/null || true
