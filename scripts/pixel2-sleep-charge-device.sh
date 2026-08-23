#!/bin/sh
# Record Pixel2 charging telemetry around the production Sleep path.
# This runs on the device and survives loss of the only USB Wi-Fi connection.
set -eu

mode=${1:-}
duration=${2:-90}
output_root=${3:-/mnt/plumos-user/.plumos-validation/sleep-charging}

case "$mode" in
    plugged|postplug|observe-plugged|observe-postplug) ;;
    *) printf 'usage: %s plugged|postplug|observe-plugged|observe-postplug [seconds] [output-root]\n' "$0" >&2; exit 2 ;;
esac
case "$duration" in ''|*[!0-9]*|0) exit 2 ;; esac

root=${PLUMOS_ROOT:-/mnt/plumos}
runtime_root=${PLUMOS_RUNTIME_ROOT:-/run/plumos}
battery=/sys/class/power_supply/battery
usb=/sys/class/power_supply/usb
ac=/sys/class/power_supply/ac
otg_mode=/sys/devices/platform/ff2c0000.syscon/ff2c0000.syscon:usb2-phy@100/otg_mode
selector=${PLUMOS_SLEEP_CHARGE_SELECTOR:-/tmp/pixel2-sleep-auto-select}
overlay=$root/bin/plumos-power-menu-overlay
stamp=$(date '+%Y%m%d-%H%M%S' 2>/dev/null || echo unknown)
output=$output_root/$stamp-$mode
samples=$output/samples.tsv
sampler_marker=$output/sampler.running

mkdir -p "$output"

read_value() {
    path=$1
    [ -r "$path" ] || { printf 'NA'; return; }
    tr -d '\r\n' <"$path" 2>/dev/null || printf 'NA'
}

interrupt_count() {
    pattern=$1
    awk -v pattern="$pattern" '$0 ~ pattern { total = 0; for (i = 2; i <= NF; i++) { if ($i !~ /^[0-9]+$/) break; total += $i } } END { print total + 0 }' /proc/interrupts
}

snapshot() {
    phase=$1
    now=$(date '+%Y-%m-%dT%H:%M:%S%z' 2>/dev/null || echo unknown)
    uptime=$(cut -d' ' -f1 /proc/uptime 2>/dev/null || echo NA)
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$now" "$uptime" "$phase" \
        "$(read_value "$battery/status")" \
        "$(read_value "$battery/capacity")" \
        "$(read_value "$battery/current_now")" \
        "$(read_value "$battery/voltage_now")" \
        "$(read_value "$battery/time_to_full_now")" \
        "$(read_value "$usb/online")" \
        "$(read_value "$ac/online")" \
        "$(read_value "$otg_mode")" \
        "$(interrupt_count 'rk817_plug_in')" \
        "$(interrupt_count 'rk817_plug_out')" \
        "$(interrupt_count 'rockchip_usb2phy_bvalid')" \
        "$(cat /sys/power/wakeup_count 2>/dev/null || echo NA)" \
        >>"$samples"
}

wait_for_charger() {
    elapsed=0
    while [ "$elapsed" -lt 180 ]; do
        status=$(read_value "$battery/status")
        current=$(read_value "$battery/current_now")
        case "$current" in ''|NA|*[!0-9-]*) current=0 ;; esac
        if [ "$status" = Charging ] || [ "$current" -gt 0 ]; then
            return 0
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done
    return 1
}

wait_for_wifi_removal() {
    elapsed=0
    while [ "$elapsed" -lt 180 ]; do
        [ -e /sys/class/net/wlan0 ] || return 0
        sleep 1
        elapsed=$((elapsed + 1))
    done
    return 1
}

printf 'time\tuptime\tphase\tstatus\tcapacity\tcurrent_ua\tvoltage_uv\ttime_to_full_min\tusb_online\tac_online\totg_mode\tplug_in_irq\tplug_out_irq\tbvalid_irq\twakeup_count\n' >"$samples"
printf '%s\n' "$mode" >"$output/mode"
printf '%s\n' "$duration" >"$output/duration"
cat /etc/plumos-system-version >"$output/system-version" 2>/dev/null || true
cp "$root/manifest.json" "$output/runtime-manifest.json" 2>/dev/null || true
dmesg >"$output/dmesg-before.log" 2>&1 || true
snapshot armed

case "$mode" in
    observe-*)
        : >"$sampler_marker"
        # Invoked indirectly by the EXIT/signal trap below.
        # shellcheck disable=SC2317
        finish_observer() {
            rm -f "$sampler_marker"
            snapshot observer-stopped
            dmesg >"$output/dmesg-after.log" 2>&1 || true
            printf 'result=observation-complete output=%s\n' "$output" >"$output/result"
            sync
        }
        trap finish_observer EXIT HUP INT TERM
        elapsed=0
        while [ -e "$sampler_marker" ] && [ "$elapsed" -lt "$duration" ]; do
            snapshot sample
            sleep 2
            elapsed=$((elapsed + 2))
        done
        exit 0
        ;;
esac

if [ "$mode" = plugged ]; then
    if ! wait_for_charger; then
        snapshot charger-timeout
        printf 'result=charger-timeout\n' >"$output/result"
        exit 1
    fi
    snapshot charger-detected
    sleep 5
else
    if ! wait_for_wifi_removal; then
        snapshot wifi-removal-timeout
        printf 'result=wifi-removal-timeout\n' >"$output/result"
        exit 1
    fi
    snapshot wifi-removed
    sleep 5
fi

: >"$sampler_marker"
(
    while [ -e "$sampler_marker" ]; do
        snapshot sample
        sleep 2
    done
) &
sampler_pid=$!

snapshot sleep-request
sleep_started=$(cut -d' ' -f1 /proc/uptime 2>/dev/null || echo 0)
set +e
PLUMOS_ROOT="$root" \
PLUMOS_RUNTIME_ROOT="$runtime_root" \
PLUMOS_POWER_MENU_OVERLAY_UI="$selector" \
PLUMOS_POWER_MENU_LOG_DIR="$output" \
PLUMOS_POWER_MENU_OVERLAY_LOG="$output/power-menu-overlay.log" \
PLUMOS_POWER_LOG_DIR="$output" \
PLUMOS_SLEEP_WAKEUP_SEC="$duration" \
    "$overlay" >"$output/overlay-command.log" 2>&1
sleep_rc=$?
set -e
sleep_finished=$(cut -d' ' -f1 /proc/uptime 2>/dev/null || echo 0)
snapshot sleep-returned
sleep 10
snapshot settled
rm -f "$sampler_marker"
wait "$sampler_pid" 2>/dev/null || true
dmesg >"$output/dmesg-after.log" 2>&1 || true
printf '%s\n' "$sleep_rc" >"$output/sleep-rc"
printf '%s\n' "$sleep_started" >"$output/sleep-start-uptime"
printf '%s\n' "$sleep_finished" >"$output/sleep-finish-uptime"
printf 'result=%s sleep_rc=%s output=%s\n' \
    "$( [ "$sleep_rc" -eq 0 ] && printf complete || printf failed )" \
    "$sleep_rc" "$output" >"$output/result"
sync
printf '%s\n' "$output"
