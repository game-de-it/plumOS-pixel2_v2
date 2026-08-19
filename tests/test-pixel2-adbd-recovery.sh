#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SERVICE="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
WATCHDOG="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-adbd-watchdog"

test -x "$SERVICE"
test -x "$WATCHDOG"
bash -n "$SERVICE"
bash -n "$WATCHDOG"

# Keep the accepted 6f022d4 FunctionFS start/stop implementation and the
# daemon-only watchdog.  The only extension is a read-only status command for
# the Runtime API; do not reintroduce policy, endpoint recovery or UDC polling.
test "$(sha256sum "$SERVICE" | awk '{print $1}')" = \
    2cfdfd45a4b4b30f9de11205ef7dd7f491af047269addf4b53e6f0fc3400f4e0
test "$(sha256sum "$WATCHDOG" | awk '{print $1}')" = \
    b67891ffd006701d96e82442491ec89eacd9866f65e077946e12d0fde908b876

grep -q 'mount -t functionfs adb' "$SERVICE"
grep -q '/usr/sbin/adbd.*&' "$SERVICE"
grep -q 'ep1.*ep2' "$SERVICE"
grep -q 'printf.*udc.*GADGET/UDC' "$SERVICE"
grep -q 'status_adbd' "$SERVICE"
grep -q 'ADB daemon lost its FunctionFS endpoints' "$SERVICE"
grep -q 'event=daemon-missing action=restart' "$WATCHDOG"

! grep -Eq 'services\.conf|usb_mode|adb_enabled|recover|replug' "$SERVICE"
! grep -Eq 'UDC|usb_role|functionfs|transport|configured|not attached' "$WATCHDOG"

printf '%s\n' 'pixel2_adbd_baseline=result-ok source=6f022d4 status=compatible'
