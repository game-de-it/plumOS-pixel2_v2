#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
ROLE_CONTROL="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/plumos-pixel2-usb-role"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-usb-role.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

cat >"$TMP/devmem" <<'EOF'
#!/bin/sh
set -eu
case "$1" in
  0xff30000c)
    if [ "$#" -eq 3 ]; then
      printf '%s\n' "$3" >"$PLUMOS_TEST_GUSBCFG"
    else
      cat "$PLUMOS_TEST_GUSBCFG"
    fi
    ;;
  0xff300014)
    value=$(cat "$PLUMOS_TEST_GUSBCFG")
    if [ $((value & 0x40000000)) -ne 0 ]; then
      printf '%s\n' 0x00000000
    else
      printf '%s\n' 0x00000001
    fi
    ;;
  *) exit 2 ;;
esac
EOF
chmod +x "$TMP/devmem"
printf '%s\n' 0x20001400 >"$TMP/gusbcfg"

role_env=(
  PLUMOS_PIXEL2_USB_DEVMEM="$TMP/devmem"
  PLUMOS_PIXEL2_USB_POLL_ATTEMPTS=2
  PLUMOS_PIXEL2_USB_POLL_DELAY_US=0
  PLUMOS_TEST_GUSBCFG="$TMP/gusbcfg"
)

env "${role_env[@]}" "$ROLE_CONTROL" device >"$TMP/device.out"
grep -q 'requested=device mode=device' "$TMP/device.out"
test "$(cat "$TMP/gusbcfg")" = 0x40001400

env "${role_env[@]}" "$ROLE_CONTROL" status >"$TMP/status.out"
grep -Fxq 'mode=device' "$TMP/status.out"
grep -Fxq 'forced=device' "$TMP/status.out"

env "${role_env[@]}" "$ROLE_CONTROL" auto >"$TMP/auto.out"
grep -q 'requested=auto mode=host' "$TMP/auto.out"
test "$(cat "$TMP/gusbcfg")" = 0x00001400

printf '%s\n' 'pixel2_usb_role=result-ok device-force=1 auto-release=1'
