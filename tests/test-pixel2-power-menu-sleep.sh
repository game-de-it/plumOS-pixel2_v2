#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-power.XXXXXX")"
OWNER_PID=""

cleanup() {
    if [ -n "$OWNER_PID" ]; then
        kill -CONT "$OWNER_PID" 2>/dev/null || true
        kill "$OWNER_PID" 2>/dev/null || true
        wait "$OWNER_PID" 2>/dev/null || true
    fi
    rm -rf "$TEST_ROOT"
}
trap cleanup EXIT

mkdir -p "$TEST_ROOT/plumos/bin" "$TEST_ROOT/run" "$TEST_ROOT/logs"

cat >"$TEST_ROOT/busybox" <<'EOF'
#!/bin/sh
exec "$@"
EOF

cat >"$TEST_ROOT/adbd" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >>"$PLUMOS_TEST_CALLS"
case "$1" in status) printf 'running=1\n' ;; esac
EOF

for helper in display volume rk817; do
    cat >"$TEST_ROOT/$helper" <<'EOF'
#!/bin/sh
printf '%s %s\n' "${0##*/}" "$*" >>"$PLUMOS_TEST_CALLS"
EOF
done

chmod 0755 "$TEST_ROOT/busybox" "$TEST_ROOT/adbd" \
    "$TEST_ROOT/display" "$TEST_ROOT/volume" "$TEST_ROOT/rk817"
printf 'freeze mem\n' >"$TEST_ROOT/power-state"
: >"$TEST_ROOT/wakealarm"
: >"$TEST_ROOT/calls"

PLUMOS_ROOT="$TEST_ROOT/plumos" \
PLUMOS_RUNTIME_ROOT="$TEST_ROOT/run" \
PLUMOS_BUSYBOX="$TEST_ROOT/busybox" \
PLUMOS_POWER_LOG_DIR="$TEST_ROOT/logs" \
PLUMOS_POWER_LOCK_DIR="$TEST_ROOT/run/power.lock" \
PLUMOS_POWER_STATE="$TEST_ROOT/power-state" \
PLUMOS_RTC_WAKEALARM="$TEST_ROOT/wakealarm" \
PLUMOS_ADBD_CONTROL="$TEST_ROOT/adbd" \
PLUMOS_DISPLAY_CONTROL="$TEST_ROOT/display" \
PLUMOS_VOLUME_CONTROL="$TEST_ROOT/volume" \
PLUMOS_RK817_RESUME_HELPER="$TEST_ROOT/rk817" \
PLUMOS_SLEEP_SETTLE_SEC=0 \
PLUMOS_TEST_CALLS="$TEST_ROOT/calls" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown" \
        --sleep --sleep-backend mem --wakeup-sec 5 --wait-sec 0

[ "$(cat "$TEST_ROOT/power-state")" = mem ]
[ "$(cat "$TEST_ROOT/wakealarm")" = +5 ]
grep -q '^rk817 arm$' "$TEST_ROOT/calls"
grep -q '^rk817 rearm$' "$TEST_ROOT/calls"
grep -q '^volume apply$' "$TEST_ROOT/calls"
grep -q '^display apply$' "$TEST_ROOT/calls"
grep -q '^stop$' "$TEST_ROOT/calls"
grep -q '^start$' "$TEST_ROOT/calls"
grep -q 'sleep=result-returned backend=mem' "$TEST_ROOT/logs/power.log"

cat >"$TEST_ROOT/menu" <<'EOF'
#!/bin/sh
state=$(ps -o state= -p "$PLUMOS_TEST_OWNER_PID" | sed 's/^[[:space:]]*//;s/^\(.\).*/\1/')
case "$state" in T|t) ;; *) exit 91 ;; esac
printf 'action=%s\n' "$PLUMOS_TEST_SELECTION" >"$PLUMOS_POWER_MENU_SELECTION"
EOF
cat >"$TEST_ROOT/safe" <<'EOF'
#!/bin/sh
state=$(ps -o state= -p "$PLUMOS_TEST_OWNER_PID" | sed 's/^[[:space:]]*//;s/^\(.\).*/\1/')
case "$state" in T|t) ;; *) exit 92 ;; esac
printf '%s\n' "$*" >>"$PLUMOS_TEST_SAFE_CALLS"
EOF
chmod 0755 "$TEST_ROOT/menu" "$TEST_ROOT/safe"

sleep 60 &
OWNER_PID=$!
: >"$TEST_ROOT/safe-calls"

PLUMOS_ROOT="$TEST_ROOT/plumos" \
PLUMOS_RUNTIME_ROOT="$TEST_ROOT/run" \
PLUMOS_POWER_MENU_LOG_DIR="$TEST_ROOT/logs" \
PLUMOS_POWER_MENU_LOCK_DIR="$TEST_ROOT/run/overlay.lock" \
PLUMOS_POWER_MENU_OVERLAY_UI="$TEST_ROOT/menu" \
PLUMOS_POWER_MENU_SAFE_SHUTDOWN="$TEST_ROOT/safe" \
PLUMOS_POWER_MENU_OVERLAY_PIDS="$OWNER_PID" \
PLUMOS_TEST_OWNER_PID="$OWNER_PID" \
PLUMOS_TEST_SELECTION=cancel \
PLUMOS_TEST_SAFE_CALLS="$TEST_ROOT/safe-calls" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-power-menu-overlay" open
kill -0 "$OWNER_PID"
case "$(ps -o state= -p "$OWNER_PID" | sed 's/^[[:space:]]*//;s/^\(.\).*/\1/')" in
    T|t) exit 1 ;;
esac
[ ! -s "$TEST_ROOT/safe-calls" ]

PLUMOS_ROOT="$TEST_ROOT/plumos" \
PLUMOS_RUNTIME_ROOT="$TEST_ROOT/run" \
PLUMOS_POWER_MENU_LOG_DIR="$TEST_ROOT/logs" \
PLUMOS_POWER_MENU_LOCK_DIR="$TEST_ROOT/run/overlay.lock" \
PLUMOS_POWER_MENU_OVERLAY_UI="$TEST_ROOT/menu" \
PLUMOS_POWER_MENU_SAFE_SHUTDOWN="$TEST_ROOT/safe" \
PLUMOS_POWER_MENU_OVERLAY_PIDS="$OWNER_PID" \
PLUMOS_TEST_OWNER_PID="$OWNER_PID" \
PLUMOS_TEST_SELECTION=sleep \
PLUMOS_TEST_SAFE_CALLS="$TEST_ROOT/safe-calls" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-power-menu-overlay" open
grep -q '^--sleep --sleep-backend mem --no-poweroff --no-hold-resume$' \
    "$TEST_ROOT/safe-calls"
kill -0 "$OWNER_PID"
case "$(ps -o state= -p "$OWNER_PID" | sed 's/^[[:space:]]*//;s/^\(.\).*/\1/')" in
    T|t) exit 1 ;;
esac

printf 'pixel2_power_menu_sleep=result-ok\n'
