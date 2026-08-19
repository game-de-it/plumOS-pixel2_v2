#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SERVICE="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
WATCHDOG="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-adbd-watchdog"
UEVENT_HELPER="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/adb-uevent"

test -x "$SERVICE"
test ! -e "$WATCHDOG"
test ! -e "$UEVENT_HELPER"
bash -n "$SERVICE"

grep -q 'mount -t functionfs adb' "$SERVICE"
grep -q '/usr/sbin/adbd.*&' "$SERVICE"
grep -q 'ep1.*ep2' "$SERVICE"
grep -q 'printf.*udc.*GADGET/UDC' "$SERVICE"
grep -q 'status_adbd' "$SERVICE"
grep -q 'ADB daemon lost its FunctionFS endpoints' "$SERVICE"
grep -q 'reset_dwc2_for_device' "$SERVICE"

! grep -q 'replug_adbd\|takeover_adbd\|adbd-replug-suppress-until' "$SERVICE"
! grep -q 'schedule_recovery\|action=watchdog-' "$SERVICE"
! grep -q 'downstream_present\|controller-reset-deferred' "$SERVICE"

restart_body="$(sed -n '/^restart_adbd() {/,/^}/p' "$SERVICE")"
grep -q 'stop_adbd' <<<"$restart_body"
grep -q 'reset_dwc2_for_device' <<<"$restart_body"
grep -q 'start_adbd' <<<"$restart_body"

TMP="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-adb-explicit.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/dwc2" "$TMP/platform" "$TMP/udc/ff300000.usb" "$TMP/logs"
: >"$TMP/dwc2/bind"
: >"$TMP/dwc2/unbind"
ln -s "$TMP/dwc2" "$TMP/platform/driver"

PLUMOS_ADB_DWC2_DRIVER="$TMP/dwc2" \
PLUMOS_ADB_PLATFORM_DEVICE="$TMP/platform" \
PLUMOS_ADB_PLATFORM_DEVICE_ID=ff300000.usb \
PLUMOS_ADB_UDC_ROOT="$TMP/udc" \
PLUMOS_ADB_DWC2_RESET_DELAY=0 \
PLUMOS_ADB_DWC2_UDC_WAIT=1 \
PLUMOS_ADB_LOG="$TMP/logs/controller.log" \
    "$SERVICE" reset-controller
grep -Fxq ff300000.usb "$TMP/dwc2/unbind"
grep -Fxq ff300000.usb "$TMP/dwc2/bind"
grep -q 'result=controller-reset-complete udc=ff300000.usb' \
    "$TMP/logs/controller.log"

printf '%s\n' 'pixel2_adbd_recovery=result-ok mode=explicit-only no-monitor=1'
