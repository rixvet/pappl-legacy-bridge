#!/bin/sh
# Installeert alleen wat de NATIVE 64-bit driver (foo2sq21hbpl) nodig heeft:
# CUPS + ghostscript. Geen 32-bit i386 libraries nodig (in tegenstelling tot
# de oude FXM_* filter-chain uit 2005-2012).
#
# Vereist root:  sudo bash scripts/setup-cups-native.sh
set -e

apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y \
    cups cups-client cups-filters ghostscript

systemctl enable --now cups 2>/dev/null || service cups start

echo "OK: CUPS geinstalleerd (native, geen 32-bit libs)."
cupsd -v 2>/dev/null || true
