#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SERVICE="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
WATCHDOG="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-adbd-watchdog"

test -x "$SERVICE"
test -x "$WATCHDOG"
bash -n "$SERVICE"
bash -n "$WATCHDOG"

# Lock the two files to the simple FunctionFS implementation that was accepted
# on physical Pixel2 hardware at commit 6f022d4.  Do not silently reintroduce
# frontend policy, USB-mode switching, endpoint recovery or UDC polling here.
test "$(sha256sum "$SERVICE" | awk '{print $1}')" = \
    6d18796073275d667889a9d2c5b9e2df2eae298003c2bbb94f2d937579c81d22
test "$(sha256sum "$WATCHDOG" | awk '{print $1}')" = \
    b67891ffd006701d96e82442491ec89eacd9866f65e077946e12d0fde908b876

grep -q 'mount -t functionfs adb' "$SERVICE"
grep -q '/usr/sbin/adbd.*&' "$SERVICE"
grep -q 'ep1.*ep2' "$SERVICE"
grep -q 'printf.*udc.*GADGET/UDC' "$SERVICE"
grep -q 'event=daemon-missing action=restart' "$WATCHDOG"

! grep -Eq 'services\.conf|usb_mode|adb_enabled|recover|replug|status' "$SERVICE"
! grep -Eq 'UDC|usb_role|functionfs|transport|configured|not attached' "$WATCHDOG"

printf '%s\n' 'pixel2_adbd_baseline=result-ok source=6f022d4'
