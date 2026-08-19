#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SERVICE="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
WATCHDOG="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-adbd-watchdog"
UEVENT_HELPER="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/adb-uevent"

test -x "$SERVICE"
test -x "$WATCHDOG"
test -x "$UEVENT_HELPER"
bash -n "$SERVICE"
bash -n "$WATCHDOG"
bash -n "$UEVENT_HELPER"

grep -q 'mount -t functionfs adb' "$SERVICE"
grep -q '/usr/sbin/adbd.*&' "$SERVICE"
grep -q 'ep1.*ep2' "$SERVICE"
grep -q 'printf.*udc.*GADGET/UDC' "$SERVICE"
grep -q 'status_adbd' "$SERVICE"
grep -q 'ADB daemon lost its FunctionFS endpoints' "$SERVICE"
grep -q 'replug_adbd' "$SERVICE"
grep -q 'action=replug-restart reason=functionfs-disconnected' "$SERVICE"
grep -q 'result=replug-restarted' "$SERVICE"
grep -q 'adbd-replug-suppress-until' "$SERVICE"
grep -q 'start_watchdog' "$WATCHDOG"
grep -q 'stop_watchdog' "$WATCHDOG"
grep -q 'busybox.*uevent' "$WATCHDOG"
grep -q 'android_usb' "$UEVENT_HELPER"
grep -q 'USB_STATE.*DISCONNECTED' "$UEVENT_HELPER"
grep -q 'ADBD_CONTROL.*replug' "$UEVENT_HELPER"
grep -q 'ADBD_CONTROL.*takeover' "$UEVENT_HELPER"
grep -q 'reset_dwc2_for_device' "$SERVICE"
grep -q 'controller-reset-deferred reason=downstream-present' "$SERVICE"

replug_body="$(sed -n '/^replug_adbd() {/,/^}/p' "$SERVICE")"
grep -q 'stop_adbd' <<<"$replug_body"
grep -q 'reset_dwc2_for_device' <<<"$replug_body"
grep -q 'start_adbd' <<<"$replug_body"
! grep -q 'GADGET/UDC' <<<"$replug_body"
! grep -q 'endpoints_open' <<<"$replug_body"

! grep -q 'schedule_recovery' "$SERVICE"
! grep -q 'while :' "$WATCHDOG"

TMP="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-adb-uevent.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/plumos/config/network" "$TMP/card" "$TMP/run" "$TMP/logs"
cat >"$TMP/busybox" <<'EOF'
#!/bin/sh
test "$1" = uevent
shift
test -x "$1"
mkdir -p "$PLUMOS_TEST_PROC_ROOT/$$"
printf '%s\000' "$0" uevent "$1" >"$PLUMOS_TEST_PROC_ROOT/$$/cmdline"
trap 'rm -rf "$PLUMOS_TEST_PROC_ROOT/$$"; exit 0' TERM INT EXIT
while :; do :; done
EOF
chmod +x "$TMP/busybox"
mkdir -p "$TMP/proc"

monitor_env=(
    PLUMOS_BUSYBOX="$TMP/busybox"
    PLUMOS_ADB_UEVENT_HELPER="$UEVENT_HELPER"
    PLUMOS_ADB_WATCHDOG_PID="$TMP/run/monitor.pid"
    PLUMOS_ADB_LOG="$TMP/logs/adbd.log"
    PLUMOS_PROC_ROOT="$TMP/proc"
    PLUMOS_TEST_PROC_ROOT="$TMP/proc"
)
env "${monitor_env[@]}" "$WATCHDOG" start
monitor_pid="$(cat "$TMP/run/monitor.pid")"
kill -0 "$monitor_pid"
monitor_cmdline="$(tr '\000' ' ' <"$TMP/proc/$monitor_pid/cmdline")"
case "$monitor_cmdline" in
    *"busybox uevent $UEVENT_HELPER"*) ;;
    *) printf 'error: wrong ADB uevent monitor pid: %s\n' "$monitor_cmdline" >&2; exit 1 ;;
esac
env "${monitor_env[@]}" "$WATCHDOG" stop
if kill -0 "$monitor_pid" 2>/dev/null; then
    printf 'error: ADB uevent monitor survived stop\n' >&2
    exit 1
fi
test ! -e "$TMP/run/monitor.pid"

printf '%s\n' 'usb_mode=adb' 'adb_enabled=1' \
    >"$TMP/plumos/config/network/services.conf"
cat >"$TMP/adbd-control" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >>"$PLUMOS_TEST_ADBD_CALLS"
EOF
chmod +x "$TMP/adbd-control"

helper_env=(
    PLUMOS_ADB_SERVICES_CONF="$TMP/plumos/config/network/services.conf"
    PLUMOS_ADB_RECOVERY_MARKER="$TMP/card/plumos-enable-adb"
    PLUMOS_ADB_CONTROL="$TMP/adbd-control"
    PLUMOS_ADB_SUPPRESS_UNTIL="$TMP/run/suppress-until"
    PLUMOS_ADB_LOG="$TMP/logs/adbd.log"
    PLUMOS_ADB_RECOVERY_SETTLE_SECONDS=0
    PLUMOS_TEST_ADBD_CALLS="$TMP/adbd-calls"
)

SUBSYSTEM=android_usb USB_STATE=CONNECTED \
    env "${helper_env[@]}" "$UEVENT_HELPER"
test ! -e "$TMP/adbd-calls"
SUBSYSTEM=android_usb USB_STATE=DISCONNECTED \
    env "${helper_env[@]}" "$UEVENT_HELPER"
grep -Fxq replug "$TMP/adbd-calls"

SUBSYSTEM=usb ACTION=remove DEVTYPE=usb_device \
    DEVPATH=/devices/platform/ff300000.usb/usb1/1-1 PRODUCT=bda/c820/200 \
    env "${helper_env[@]}" "$UEVENT_HELPER"
grep -Fxq takeover "$TMP/adbd-calls"

SUBSYSTEM=usb ACTION=remove DEVTYPE=usb_device \
    DEVPATH=/devices/platform/ff340000.usb/usb2/2-1 PRODUCT=ffff/ffff/0 \
    env "${helper_env[@]}" "$UEVENT_HELPER"
test "$(grep -c '^takeover$' "$TMP/adbd-calls")" -eq 1

printf '%s\n' "$(( $(date +%s) + 30 ))" >"$TMP/run/suppress-until"
SUBSYSTEM=android_usb USB_STATE=DISCONNECTED \
    env "${helper_env[@]}" "$UEVENT_HELPER"
test "$(grep -c '^replug$' "$TMP/adbd-calls")" -eq 1
grep -q 'reason=self-rebind' "$TMP/logs/adbd.log"

printf '%s\n' 'usb_mode=wifi' 'adb_enabled=0' \
    >"$TMP/plumos/config/network/services.conf"
rm -f "$TMP/run/suppress-until"
SUBSYSTEM=android_usb USB_STATE=DISCONNECTED \
    env "${helper_env[@]}" "$UEVENT_HELPER"
test "$(grep -c '^replug$' "$TMP/adbd-calls")" -eq 1
test "$(grep -c '^takeover$' "$TMP/adbd-calls")" -eq 1

mkdir -p "$TMP/dwc2" "$TMP/platform" "$TMP/udc/ff300000.usb"
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
PLUMOS_ADB_SUPPRESS_UNTIL="$TMP/run/controller-suppress-until" \
    "$SERVICE" reset-controller
grep -Fxq ff300000.usb "$TMP/dwc2/unbind"
grep -Fxq ff300000.usb "$TMP/dwc2/bind"
grep -q 'result=controller-reset-complete udc=ff300000.usb' \
    "$TMP/logs/controller.log"

printf '%s\n' 'pixel2_adbd_recovery=result-ok source=v90s-event-driven mode=functionfs-restart'
