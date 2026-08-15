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

cat >"$TEST_ROOT/rk817" <<'EOF'
#!/bin/sh
printf '%s %s\n' "${0##*/}" "$*" >>"$PLUMOS_TEST_CALLS"
printf 'rk817_alsa %s\n' "${ALSA_CONFIG_PATH:-missing}" >>"$PLUMOS_TEST_CALLS"
EOF

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
grep -q "^rk817_alsa $TEST_ROOT/plumos/config/alsa/alsa.conf$" \
    "$TEST_ROOT/calls"
grep -q '^volume apply$' "$TEST_ROOT/calls"
grep -q '^display apply$' "$TEST_ROOT/calls"
grep -q '^stop$' "$TEST_ROOT/calls"
grep -q '^start$' "$TEST_ROOT/calls"
grep -q 'sleep=result-returned backend=mem' "$TEST_ROOT/logs/power.log"

printf '42\n' >"$TEST_ROOT/backlight"
printf '0\n' >"$TEST_ROOT/backlight-power"
printf '0\n' >"$TEST_ROOT/fb-blank"
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
PLUMOS_PIXEL2_BACKLIGHT="$TEST_ROOT/backlight" \
PLUMOS_PIXEL2_BACKLIGHT_POWER="$TEST_ROOT/backlight-power" \
PLUMOS_PIXEL2_FB_BLANK="$TEST_ROOT/fb-blank" \
PLUMOS_FORCE_SOFTWARE_SLEEP=1 \
PLUMOS_SLEEP_SETTLE_SEC=0 \
PLUMOS_TEST_CALLS="$TEST_ROOT/calls" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown" \
        --sleep --sleep-backend mem --wakeup-sec 1 --wait-sec 0
[ "$(cat "$TEST_ROOT/backlight")" = 42 ]
[ "$(cat "$TEST_ROOT/backlight-power")" = 0 ]
[ "$(cat "$TEST_ROOT/fb-blank")" = 0 ]
[ ! -e "$TEST_ROOT/run/software-sleep" ]
grep -q 'sleep=display-blank brightness=42 bl_power=0' \
    "$TEST_ROOT/logs/power.log"
grep -q 'sleep=display-unblank brightness=42 bl_power=0' \
    "$TEST_ROOT/logs/power.log"
grep -q 'sleep=software-enter reason=kernel-unavailable wakeup_sec=1' \
    "$TEST_ROOT/logs/power.log"
grep -q 'sleep=software-wake reason=timeout seconds=1' \
    "$TEST_ROOT/logs/power.log"
grep -q 'sleep=result-returned backend=mem kernel_sleep=0' \
    "$TEST_ROOT/logs/power.log"

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

grep -q 'wake_software_sleep' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q 'USB_POWER_EVENT_GUARD_MS 1500' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q 'ADB_USB_RESTART_DELAY_MS 2000' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q '/sys/class/power_supply/usb/online' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q 'PLUMOS_ADBD_CONTROL' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q 'control, "restart"' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q 'consume_power_wake_suppression' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'write_power_overlay_selection(ui, "handled")' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'SYS_pidfd_getfd' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q 'drmDropMaster(owner_drm_fd)' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q 'drmSetMaster(owner_drm_fd)' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q '^  handled)' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-power-menu-overlay"
grep -q 'plumos_fbdev_present_black(&ui->fbdev_renderer)' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'static int plumos_fbdev_present_black' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h"
grep -q 'static int plumos_fbdev_set_display_power' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h"
grep -q 'DRM_MODE_DPMS_OFF' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h"
grep -q 'drmModeSetCrtc(r->drm_fd, r->drm_crtc_id, 0' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h"
