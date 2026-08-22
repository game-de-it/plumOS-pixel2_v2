#!/bin/sh
set -u

prefix=${1:-}
sleep_seconds=${2:-3}
[ -n "$prefix" ] || exit 2
case "$sleep_seconds" in ''|*[!0-9]*|0) exit 2 ;; esac

root=${PLUMOS_ROOT:-/mnt/plumos}
runtime=${PLUMOS_RUNTIME_ROOT:-/run/plumos}
power_log=$root/logs/power.log
overlay_log=$root/logs/power-menu-overlay.log

power_start=$(wc -l <"$power_log" 2>/dev/null || echo 0)
overlay_start=$(wc -l <"$overlay_log" 2>/dev/null || echo 0)

PLUMOS_ROOT="$root" \
PLUMOS_RUNTIME_ROOT="$runtime" \
PLUMOS_POWER_MENU_OVERLAY_UI=/tmp/pixel2-sleep-auto-select \
PLUMOS_SLEEP_WAKEUP_SEC="$sleep_seconds" \
PLUMOS_SLEEP_SETTLE_SEC=1 \
    "$root/bin/plumos-power-menu-overlay" >"$prefix.sleep-command.log" 2>&1
rc=$?

power_end=$(wc -l <"$power_log" 2>/dev/null || echo 0)
overlay_end=$(wc -l <"$overlay_log" 2>/dev/null || echo 0)
sed -n "$((power_start + 1)),${power_end}p" "$power_log" \
    >"$prefix.power.log" 2>/dev/null || true
sed -n "$((overlay_start + 1)),${overlay_end}p" "$overlay_log" \
    >"$prefix.overlay.log" 2>/dev/null || true
printf '%s\n' "$rc" >"$prefix.sleep-rc"
printf 'done\n' >"$prefix.sleep-done"
exit "$rc"
