#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SERVICE="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-adbd-recovery.XXXXXX")"
ADBD_PID=
cleanup() {
    if [[ -n "$ADBD_PID" ]]; then
        kill "$ADBD_PID" 2>/dev/null || true
        wait "$ADBD_PID" 2>/dev/null || true
    fi
    rm -rf "$TMP"
}
trap cleanup EXIT

mkdir -p "$TMP/gadget" "$TMP/udc/fake-udc" "$TMP/run" "$TMP/log" \
    "$TMP/config" "$TMP/proc"
printf '%s\n' fake-udc >"$TMP/gadget/UDC"
printf '%s\n' configured >"$TMP/udc/fake-udc/state"
printf '%s\n' adb_enabled=1 >"$TMP/config/services.conf"

sleep 30 &
ADBD_PID=$!
mkdir -p "$TMP/proc/$ADBD_PID"
printf '/usr/sbin/adbd\0' >"$TMP/proc/$ADBD_PID/cmdline"
printf '%s\n' "$ADBD_PID" >"$TMP/run/adbd.pid"

service_env=(
    PLUMOS_ADB_GADGET="$TMP/gadget"
    PLUMOS_ADB_UDC_CLASS="$TMP/udc"
    PLUMOS_ADB_PID="$TMP/run/adbd.pid"
    PLUMOS_ADB_RECOVERY_PID="$TMP/run/adbd-recovery.pid"
    PLUMOS_ADB_ACTION_LOCK="$TMP/run/adbd-action.lock"
    PLUMOS_ADB_LOG="$TMP/log/adbd.log"
    PLUMOS_ADB_PROC_ROOT="$TMP/proc"
    PLUMOS_ADB_SERVICES_CONF="$TMP/config/services.conf"
    PLUMOS_ADB_OPT_IN_MARKER="$TMP/config/enable-adb"
    PLUMOS_ADB_RECOVERY_WAIT=2
    PLUMOS_ADBD_RECOVERY_WORKER=1
)

env "${service_env[@]}" "$SERVICE" recover
grep -q 'result=recover-not-needed state=configured' "$TMP/log/adbd.log"
test "$(cat "$TMP/gadget/UDC")" = fake-udc

# A physical cable replug invalidates the host transport even when the UDC has
# already returned to configured.  The dedicated action must therefore rebind
# rather than use recover's healthy no-op.
env "${service_env[@]}" "$SERVICE" replug
grep -q 'action=rebind reason=usb-replug state=configured' "$TMP/log/adbd.log"
grep -q 'result=replug-recovered action=rebind state=configured' "$TMP/log/adbd.log"
test "$(cat "$TMP/gadget/UDC")" = fake-udc

printf '%s\n' 'not attached' >"$TMP/udc/fake-udc/state"
(
    sleep 0.2
    printf '%s\n' configured >"$TMP/udc/fake-udc/state"
) &
env "${service_env[@]}" "$SERVICE" recover
grep -q 'action=rebind reason=udc-not attached' "$TMP/log/adbd.log"
grep -q 'result=recovered action=rebind state=configured' "$TMP/log/adbd.log"
test "$(cat "$TMP/gadget/UDC")" = fake-udc

grep -q 'schedule_recovery' "$SERVICE"
grep -q 'action=watchdog-recover' "$SERVICE"

# Mutating actions must not overlap.  This is the failure mode that previously
# launched two adbd instances and collided on the JDWP/FunctionFS sockets.
mkdir "$TMP/run/adbd-action.lock"
printf '%s\n' "$$" >"$TMP/run/adbd-action.lock/pid"
if env "${service_env[@]}" PLUMOS_ADB_ACTION_LOCK_WAIT=0 \
    "$SERVICE" replug; then
    echo 'replug unexpectedly bypassed the active action lock' >&2
    exit 1
fi
grep -q 'result=action-busy action=replug' "$TMP/log/adbd.log"
rmdir "$TMP/run/adbd-action.lock" 2>/dev/null || {
    rm -f "$TMP/run/adbd-action.lock/pid"
    rmdir "$TMP/run/adbd-action.lock"
}

printf '%s\n' 'pixel2_adbd_recovery=result-ok'
