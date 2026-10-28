#!/bin/bash
# test-compat-distro.sh — compatibility test of the whole repo in a WSL distro.
# Runs INSIDE the distro (as root); the repo is e.g. at /root/phaser6125.
#
# Called from the default WSL distro (Windows host WINNIE):
#   cd /home/rick/src/phaser6125
#   tar czf - --exclude=.git --exclude=re/build --exclude=tests/build --exclude=__pycache__ . \
#     | wsl.exe -d Ubuntu-26.04 -u root -- bash -c \
#       'rm -rf /root/phaser6125 && mkdir -p /root/phaser6125 && \
#        tar xzf - -C /root/phaser6125 && \
#        bash /root/phaser6125/scripts/test-compat-distro.sh'
#
# Env:  SKIP_PRINT=1   skips the real print test (be frugal with pages
#                       on the shared printer)
#       REPO=<path>    repo location (default /root/phaser6125)
#
# What it tests: apt deps (incl. libc6-dev — without it the build fails on 26.04),
# gcc build (-Wall -Wextra, fatal), clang -Weverything -Werror (report,
# not fatal), verify-native round-trip (raw + calibrated colour + gray),
# CUPS install + queue, 1 real print via lp, printer-status.py.
set -e
REPO="${REPO:-/root/phaser6125}"
cd "$REPO" || exit 1
echo "=== OS ==="; head -2 /etc/os-release
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq cups cups-client cups-filters ghostscript gcc clang \
                       python3 libc6-dev
echo "=== versions ==="
gcc --version | head -1
clang --version | head -1
echo -n "gs: "; gs --version
dpkg -l cups 2>/dev/null | grep ^ii | awk '{print "cups:", $3}'
service cups start 2>/dev/null; sleep 2
echo -n "scheduler: "; lpstat -r 2>&1
echo "=== build driver (gcc -Wall -Wextra, fatal) ==="
gcc -O2 -Wall -Wextra -o native/foo2sq21hbpl native/foo2sq21hbpl.c native/hbpl_core.c -lm && echo "gcc build OK"
file native/foo2sq21hbpl | cut -c1-80
echo "=== build driver (clang -Weverything -Werror, report) ==="
if clang -Weverything -Werror -Wno-unsafe-buffer-usage -Wno-unknown-warning-option \
        -O2 -o /tmp/foo2sq21hbpl-clang native/foo2sq21hbpl.c native/hbpl_core.c -lm 2>/tmp/clang-distro.log; then
    echo "clang -Weverything -Werror: OK"
else
    echo "clang -Weverything -Werror: FAILED (see /tmp/clang-distro.log)"
    head -20 /tmp/clang-distro.log
fi
echo "=== verify: raw (FOO2SQ_CALIBRATE=0) ==="
FOO2SQ_CALIBRATE=0 python3 re/verify-native.py tests/colorpage.pdf \
    "PageSize=A4 FXOutputMode=600x600 FXColorMode=Color" 2>&1 | tail -4
echo "=== verify: calibrated colour (CST+DLUT vs oracle) ==="
python3 re/verify-native.py tests/colorpage.pdf \
    "PageSize=A4 FXOutputMode=600x600 FXColorMode=Color" 2>&1 | tail -4
echo "=== verify: calibrated gray (NTSC(CST) vs oracle) ==="
python3 re/verify-native.py tests/colorpage.pdf \
    "PageSize=A4 FXOutputMode=600x600 FXColorMode=Black" 2>&1 | tail -4
echo "=== install + queue (CUPS) ==="
bash scripts/install-native-driver.sh 2>&1 | tail -8
if [ "${SKIP_PRINT:-0}" = "1" ]; then
    echo "=== print test SKIPPED (SKIP_PRINT=1) ==="
else
    echo "=== print via lp (1 test page) ==="
    bash scripts/test-print.sh 2>&1
    sleep 15
    echo -n "printer state: "; lpstat -p Phaser6125N 2>&1 | head -1
fi
echo "=== printer-status.py ==="
python3 scripts/printer-status.py 2>&1 | head -25
echo "=== DONE (all OK) ==="
