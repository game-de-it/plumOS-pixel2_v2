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

mode_env=(
    PLUMOS_ROOT="$TMP/plumos"
    PLUMOS_SDCARD_ROOT="$TMP/card"
    PLUMOS_RUNTIME_ROOT="$TMP/run"
    PLUMOS_ADBD_CONTROL="$TMP/no-adbd-control"
)

env "${mode_env[@]}" "$SERVICE" usb-mode wifi >/dev/null
grep -Fxq 'usb_mode=wifi' "$TMP/plumos/config/network/services.conf"
grep -Fxq 'adb_enabled=0' "$TMP/plumos/config/network/services.conf"
grep -Fxq 'ssh_enabled=1' "$TMP/plumos/config/network/services.conf"
test ! -e "$TMP/card/plumos-enable-adb"
env "${mode_env[@]}" "$SERVICE" status adb >"$TMP/adb-wifi.status" || true
grep -Fxq 'enabled=0' "$TMP/adb-wifi.status"

env "${mode_env[@]}" "$SERVICE" usb-mode adb >/dev/null
grep -Fxq 'usb_mode=adb' "$TMP/plumos/config/network/services.conf"
grep -Fxq 'adb_enabled=1' "$TMP/plumos/config/network/services.conf"
test -f "$TMP/card/plumos-enable-adb"
env "${mode_env[@]}" "$SERVICE" status adb >"$TMP/adb-adb.status" || true
grep -Fxq 'enabled=1' "$TMP/adb-adb.status"

env "${mode_env[@]}" "$SERVICE" usb-mode off >/dev/null
grep -Fxq 'usb_mode=off' "$TMP/plumos/config/network/services.conf"
grep -Fxq 'adb_enabled=0' "$TMP/plumos/config/network/services.conf"
test ! -e "$TMP/card/plumos-enable-adb"

if env "${mode_env[@]}" "$SERVICE" usb-mode invalid >/dev/null 2>&1; then
    printf 'error: invalid USB mode was accepted\n' >&2
    exit 1
fi

printf 'pixel2_usb_mode=result-ok\n'
