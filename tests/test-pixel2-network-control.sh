#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CONTROL="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-control"
BOOT_SERVICE="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/20-usb-wifi"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-network.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT

plumos="$tmp/plumos"
usb="$tmp/usb"
net="$tmp/net"
modules="$tmp/modules"
run="$tmp/run"
mkdir -p "$plumos/bin" "$plumos/config" "$plumos/logs" \
    "$usb/1-1" "$net" "$modules" "$run" "$tmp/user"
printf '0bda\n' >"$usb/1-1/idVendor"
printf '8179\n' >"$usb/1-1/idProduct"
printf '%s\n' \
    'alias usb:v0BDAp8179d*dc*dsc*dp*ic*isc*ip*in* r8188eu' \
    >"$modules/modules.alias"

cat >"$plumos/bin/modprobe" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >>"$TEST_COMMAND_LOG"
mkdir -p "$TEST_NET_SYSFS_ROOT/wlan0/wireless"
EOF
cat >"$plumos/bin/wpa_supplicant" <<'EOF'
#!/bin/sh
pidfile=
while [ "$#" -gt 0 ]; do
    case "$1" in -P) pidfile=$2; shift 2 ;; *) shift ;; esac
done
[ -z "$pidfile" ] || printf '%s\n' $$ >"$pidfile"
exit 0
EOF
cat >"$plumos/bin/wpa_cli" <<'EOF'
#!/bin/sh
case "$*" in
    *' ping')
        ping_count=0
        [ ! -r "$TEST_WPA_PING_COUNT_FILE" ] || \
            ping_count="$(cat "$TEST_WPA_PING_COUNT_FILE")"
        ping_count=$((ping_count + 1))
        printf '%s\n' "$ping_count" >"$TEST_WPA_PING_COUNT_FILE"
        if [ "$ping_count" -ge "${FAKE_WPA_PING_READY_AFTER:-1}" ]; then
            printf 'PONG\n'
        else
            printf 'FAIL\n'
        fi
        ;;
    *' status')
        printf 'wpa_state=%s\n' "${FAKE_WPA_STATE:-COMPLETED}"
        ;;
    *' signal_poll')
        printf 'RSSI=-42\nLINKSPEED=72\nFREQUENCY=2412\n'
        ;;
    *' scan_results')
        printf 'bssid / frequency / signal level / flags / ssid\n'
        printf '00:11:22:33:44:55\t2412\t-42\t[WPA2-PSK-CCMP][ESS]\tPixel2 Test\n'
        ;;
    *' scan') printf 'OK\n' ;;
esac
EOF
cat >"$plumos/bin/ip" <<'EOF'
#!/bin/sh
case "$*" in
    'addr show dev wlan0')
        [ ! -s "$TEST_IP_FILE" ] || printf '    inet %s/24 scope global wlan0\n' "$(cat "$TEST_IP_FILE")"
        ;;
    'addr flush dev wlan0') rm -f "$TEST_IP_FILE" ;;
    'route show default') printf 'default via 192.0.2.1 dev wlan0\n' ;;
esac
exit 0
EOF
cat >"$plumos/bin/udhcpc" <<'EOF'
#!/bin/sh
printf '192.0.2.20\n' >"$TEST_IP_FILE"
exit 0
EOF
cat >"$plumos/bin/timeout" <<'EOF'
#!/bin/sh
shift
exec "$@"
EOF
cat >"$plumos/bin/ping" <<'EOF'
#!/bin/sh
exit 0
EOF
cat >"$plumos/bin/sleep" <<'EOF'
#!/bin/sh
exit 0
EOF
chmod 0755 "$plumos/bin/"*

export TEST_NET_SYSFS_ROOT="$net"
export TEST_IP_FILE="$tmp/ip"
export TEST_COMMAND_LOG="$tmp/commands.log"
export TEST_WPA_PING_COUNT_FILE="$tmp/wpa-ping-count"

run_control() {
    env \
        PLUMOS_ROOT="$plumos" \
        PLUMOS_SDCARD_ROOT="$tmp/user" \
        PLUMOS_RUNTIME_ROOT="$run" \
        PLUMOS_USB_SYSFS_ROOT="$usb" \
        PLUMOS_NET_SYSFS_ROOT="$net" \
        PLUMOS_MODULES_DIR="$modules" \
        PLUMOS_ALT_MODULES_DIR="$tmp/no-alt-modules" \
        PLUMOS_ROOT_WPA_CONFIG="$tmp/root-wpa.conf" \
        PLUMOS_WIFI_IFACE_WAIT_SECONDS=1 \
        PLUMOS_WPA_WAIT_SECONDS=1 \
        PLUMOS_DHCP_WAIT_SECONDS=1 \
        PLUMOS_WIFI_SCAN_WAIT_SECONDS=1 \
        PLUMOS_UDHCPC_SCRIPT="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-udhcpc-script" \
        "$CONTROL" "$@"
}

scan_output="$(FAKE_WPA_PING_READY_AFTER=3 run_control --scan)"
grep -Fq $'network\tsecured\t-42\tPixel2 Test' <<<"$scan_output"
grep -Fxq r8188eu "$TEST_COMMAND_LOG"
[ "$(cat "$TEST_WPA_PING_COUNT_FILE")" -ge 3 ]
rm -f "$TEST_WPA_PING_COUNT_FILE"

connect_file="$tmp/connect"
printf 'Cafe "Five"\ncorrect horse battery staple\n' >"$connect_file"
connect_output="$(run_control --connect-file "$connect_file")"
grep -Fxq 'result=connected' <<<"$connect_output"
grep -Fxq 'ip=192.0.2.20' <<<"$connect_output"
grep -Fq 'ssid="Cafe \"Five\""' "$plumos/config/wpa_supplicant.conf"
grep -Fq 'psk="correct horse battery staple"' "$plumos/config/wpa_supplicant.conf"
grep -Fxq 'wpa_state=COMPLETED' "$run/network-control/wpa_status.txt"

cp "$plumos/config/wpa_supplicant.conf" "$tmp/known-good.conf"
if FAKE_WPA_STATE=SCANNING run_control --connect-file "$connect_file" \
    >"$tmp/failed-connect.out" 2>&1; then
    printf 'error: incomplete association unexpectedly succeeded\n' >&2
    exit 1
fi
cmp "$tmp/known-good.conf" "$plumos/config/wpa_supplicant.conf"
grep -Fxq 'stage=wpa_completed' "$tmp/failed-connect.out"

mkdir -p "$tmp/boot/config/system" "$tmp/boot/logs"
printf 'network={}\n' >"$tmp/boot/wpa.conf"
printf '{"wifi_enabled": false}\n' >"$tmp/boot/config/system/settings.json"
PLUMOS_WIFI_CONFIG="$tmp/boot/wpa.conf" \
PLUMOS_SYSTEM_SETTINGS_JSON="$tmp/boot/config/system/settings.json" \
PLUMOS_WIFI_LOG="$tmp/boot/logs/wifi.log" \
PLUMOS_NETWORK_CONTROL="$tmp/should-not-run" \
    "$BOOT_SERVICE" start
grep -Fq 'result=disabled reason=saved-off' "$tmp/boot/logs/wifi.log"

printf 'pixel2_network_control=result-ok\n'
