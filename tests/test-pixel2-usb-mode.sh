#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SERVICE="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-usb-mode.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/plumos/config/network" "$TMP/plumos/logs" "$TMP/card" \
    "$TMP/run"
cat >"$TMP/plumos/config/network/services.conf" <<'EOF'
ssh_enabled=1
adb_enabled=1
EOF
: >"$TMP/card/plumos-enable-adb"
cat >"$TMP/adbd-control" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >>"$PLUMOS_TEST_ADBD_CALLS"
case "$1" in
  status) printf '%s\n' 'state=stopped' ;;
esac
EOF
cat >"$TMP/wifi-recovery" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >>"$PLUMOS_TEST_WIFI_RECOVERY_CALLS"
EOF
cat >"$TMP/network-control" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >>"$PLUMOS_TEST_NETWORK_CONTROL_CALLS"
EOF
chmod +x "$TMP/adbd-control" "$TMP/wifi-recovery" "$TMP/network-control"

mode_env=(
    PLUMOS_ROOT="$TMP/plumos"
    PLUMOS_SDCARD_ROOT="$TMP/card"
    PLUMOS_RUNTIME_ROOT="$TMP/run"
    PLUMOS_ADBD_CONTROL="$TMP/adbd-control"
    PLUMOS_WIFI_RECOVERY="$TMP/wifi-recovery"
    PLUMOS_NETWORK_CONTROL="$TMP/network-control"
    PLUMOS_TEST_ADBD_CALLS="$TMP/adbd-calls"
    PLUMOS_TEST_WIFI_RECOVERY_CALLS="$TMP/wifi-recovery-calls"
    PLUMOS_TEST_NETWORK_CONTROL_CALLS="$TMP/network-control-calls"
)

env "${mode_env[@]}" "$SERVICE" usb-mode wifi >/dev/null
grep -Fxq stop "$TMP/adbd-calls"
grep -Fxq sync "$TMP/wifi-recovery-calls"
grep -Fxq 'usb_mode=wifi' "$TMP/plumos/config/network/services.conf"
grep -Fxq 'adb_enabled=0' "$TMP/plumos/config/network/services.conf"
grep -Fxq 'ssh_enabled=1' "$TMP/plumos/config/network/services.conf"
test ! -e "$TMP/card/plumos-enable-adb"
env "${mode_env[@]}" "$SERVICE" status adb >"$TMP/adb-wifi.status" || true
grep -Fxq 'enabled=0' "$TMP/adb-wifi.status"

env "${mode_env[@]}" "$SERVICE" usb-mode adb >/dev/null
grep -Fxq restart "$TMP/adbd-calls"
grep -Fxq stop "$TMP/wifi-recovery-calls"
grep -Fxq -- '--wifi off' "$TMP/network-control-calls"
grep -Fxq 'usb_mode=adb' "$TMP/plumos/config/network/services.conf"
grep -Fxq 'adb_enabled=1' "$TMP/plumos/config/network/services.conf"
test -f "$TMP/card/plumos-enable-adb"
env "${mode_env[@]}" "$SERVICE" status adb >"$TMP/adb-adb.status" || true
grep -Fxq 'enabled=1' "$TMP/adb-adb.status"

env "${mode_env[@]}" "$SERVICE" usb-mode off >/dev/null
test "$(grep -c '^stop$' "$TMP/adbd-calls")" -ge 2
test "$(grep -c '^stop$' "$TMP/wifi-recovery-calls")" -ge 2
grep -Fxq 'usb_mode=off' "$TMP/plumos/config/network/services.conf"
grep -Fxq 'adb_enabled=0' "$TMP/plumos/config/network/services.conf"
test ! -e "$TMP/card/plumos-enable-adb"

if env "${mode_env[@]}" "$SERVICE" usb-mode invalid >/dev/null 2>&1; then
    printf 'error: invalid USB mode was accepted\n' >&2
    exit 1
fi

printf 'pixel2_usb_mode=result-ok\n'
