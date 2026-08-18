#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SERVICE="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-usb-host-reenumerate"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-usb-host.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/usb" "$TMP/dwc2" "$TMP/platform" "$TMP/config/system" \
    "$TMP/log"
printf 'usb_mode=wifi\nadb_enabled=0\n' >"$TMP/config/services.conf"
printf 'network={}\n' >"$TMP/config/wpa_supplicant.conf"
printf '{"wifi_enabled": true}\n' >"$TMP/config/system/settings.json"
printf '0\n' >"$TMP/usb-online"
: >"$TMP/dwc2/bind"
: >"$TMP/dwc2/unbind"
ln -s "$TMP/dwc2" "$TMP/platform/driver"

service_env=(
    PLUMOS_USB_HOST_USB_ONLINE="$TMP/usb-online"
    PLUMOS_USB_HOST_DEVICES_ROOT="$TMP/usb"
    PLUMOS_USB_HOST_DWC2_DRIVER="$TMP/dwc2"
    PLUMOS_USB_HOST_PLATFORM_DEVICE="$TMP/platform"
    PLUMOS_USB_HOST_DEVICE_ID=ff300000.usb
    PLUMOS_USB_HOST_WPA_CONFIG="$TMP/config/wpa_supplicant.conf"
    PLUMOS_USB_HOST_SETTINGS="$TMP/config/system/settings.json"
    PLUMOS_USB_HOST_SERVICES_CONF="$TMP/config/services.conf"
    PLUMOS_USB_HOST_ADB_MARKER="$TMP/config/plumos-enable-adb"
    PLUMOS_USB_HOST_LOG="$TMP/log/service.log"
    PLUMOS_USB_HOST_RESET_DELAY=0
    PLUMOS_USB_HOST_ENUMERATION_WAIT=0
)

env "${service_env[@]}" "$SERVICE" worker
grep -Fxq 'ff300000.usb' "$TMP/dwc2/unbind"
grep -Fxq 'ff300000.usb' "$TMP/dwc2/bind"
grep -q 'result=reset-complete downstream=absent' "$TMP/log/service.log"

: >"$TMP/dwc2/bind"
: >"$TMP/dwc2/unbind"
printf '1\n' >"$TMP/usb-online"
env "${service_env[@]}" "$SERVICE" worker
test ! -s "$TMP/dwc2/bind"
test ! -s "$TMP/dwc2/unbind"
grep -q 'result=skipped reason=usb-upstream-online' "$TMP/log/service.log"

printf '0\n' >"$TMP/usb-online"
mkdir -p "$TMP/usb/1-1"
printf '0bda\n' >"$TMP/usb/1-1/idVendor"
env "${service_env[@]}" "$SERVICE" worker
test ! -s "$TMP/dwc2/bind"
test ! -s "$TMP/dwc2/unbind"
grep -q 'result=skipped reason=downstream-already-enumerated' \
    "$TMP/log/service.log"

rm -rf "$TMP/usb/1-1"
printf '{"wifi_enabled": false}\n' >"$TMP/config/system/settings.json"
env "${service_env[@]}" "$SERVICE" worker
grep -Fxq 'ff300000.usb' "$TMP/dwc2/bind"
grep -Fxq 'ff300000.usb' "$TMP/dwc2/unbind"
grep -q 'result=reset-complete downstream=absent' "$TMP/log/service.log"

# Explicit Off owns neither side and must not reset the shared controller even
# if legacy Wi-Fi settings still say ON.
printf 'usb_mode=off\nadb_enabled=0\n' >"$TMP/config/services.conf"
printf '{"wifi_enabled": true}\n' >"$TMP/config/system/settings.json"
: >"$TMP/dwc2/bind"
: >"$TMP/dwc2/unbind"
env "${service_env[@]}" "$SERVICE" worker
test ! -s "$TMP/dwc2/bind"
test ! -s "$TMP/dwc2/unbind"
grep -q 'result=skipped reason=wifi-not-requested' "$TMP/log/service.log"

# ADB ON is authoritative: saved Wi-Fi must never reset DWC2 underneath the
# bound FunctionFS gadget.
printf 'usb_mode=adb\nadb_enabled=1\n' >"$TMP/config/services.conf"
printf '{"wifi_enabled": true}\n' >"$TMP/config/system/settings.json"
printf 'network={}\n' >"$TMP/config/wpa_supplicant.conf"
: >"$TMP/dwc2/bind"
: >"$TMP/dwc2/unbind"
env "${service_env[@]}" "$SERVICE" worker
test ! -s "$TMP/dwc2/bind"
test ! -s "$TMP/dwc2/unbind"
grep -q 'result=skipped reason=adb-priority' "$TMP/log/service.log"

printf 'pixel2_usb_host_reenumerate=result-ok\n'
