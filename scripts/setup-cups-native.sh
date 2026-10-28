#!/bin/sh
# Installs only what the NATIVE 64-bit driver (foo2sq21hbpl) needs:
# CUPS + ghostscript. No 32-bit i386 libraries needed (unlike
# the old FXM_* filter chain from 2005-2012).
#
# Requires root:  sudo bash scripts/setup-cups-native.sh
set -e

apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y \
    cups cups-client cups-filters ghostscript

systemctl enable --now cups 2>/dev/null || service cups start

echo "OK: CUPS installed (native, no 32-bit libs)."
cupsd -v 2>/dev/null || true
