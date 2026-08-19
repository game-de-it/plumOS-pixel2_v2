#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
RECOVERY_SOURCE="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-wifi-recovery"
UEVENT_SOURCE="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-wifi-uevent"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-wifi-recovery.XXXXXX")"
root="$tmp/plumos"
run="$tmp/run"

cleanup() {
  if [ -x "$root/bin/plumos-wifi-recovery" ]; then
    PLUMOS_ROOT="$root" PLUMOS_RUNTIME_ROOT="$run" \
      PLUMOS_BUSYBOX="$root/network/bin/busybox" \
      "$root/bin/plumos-wifi-recovery" stop >/dev/null 2>&1 || true
  fi
  rm -rf "$tmp"
}
trap cleanup EXIT

mkdir -p "$root/bin" "$root/network/bin" "$root/config/system" \
  "$root/config/network" "$root/logs" "$run"
cp "$RECOVERY_SOURCE" "$root/bin/plumos-wifi-recovery"
cp "$UEVENT_SOURCE" "$root/bin/plumos-wifi-uevent"
chmod 0755 "$root/bin/plumos-wifi-recovery" "$root/bin/plumos-wifi-uevent"

cat >"$root/network/bin/busybox" <<'EOF'
#!/bin/sh
case "${1:-}" in
  --list) printf 'uevent\nusleep\n' ;;
  uevent)
    mkdir -p "$PLUMOS_PROC_ROOT/$$"
    printf '%s\000%s\000%s\000' "$0" uevent "$2" \
      >"$PLUMOS_PROC_ROOT/$$/cmdline"
    trap 'rm -rf "$PLUMOS_PROC_ROOT/$$"; exit 0' HUP INT TERM
    while :; do sleep 10; done
    ;;
  usleep) sleep 0.05 ;;
  *) exit 1 ;;
esac
EOF
cat >"$root/bin/plumos-network-control" <<'EOF'
#!/bin/sh
case "$*" in
  '--wifi status')
    if [ -e "$TEST_RECOVERY_CONNECTED" ]; then
      printf 'wifi=on\nip=192.0.2.120\n'
    else
      printf 'wifi=off\nip=none\n'
    fi
    ;;
  '--wifi on')
    printf '%s\n' "$*" >>"$TEST_RECOVERY_CALLS"
    : >"$TEST_RECOVERY_CONNECTED"
    printf 'result=connected\nip=192.0.2.120\n'
    ;;
  *) exit 2 ;;
esac
EOF
chmod 0755 "$root/network/bin/busybox" "$root/bin/plumos-network-control"

printf 'network={}\n' >"$root/config/wpa_supplicant.conf"
printf '{"wifi_enabled": true}\n' >"$root/config/system/settings.json"
export TEST_RECOVERY_CALLS="$tmp/recovery-calls"
export TEST_RECOVERY_CONNECTED="$tmp/recovery-connected"

recovery_env=(
  PLUMOS_ROOT="$root"
  PLUMOS_SDCARD_ROOT="$tmp/user"
  PLUMOS_RUNTIME_ROOT="$run"
  PLUMOS_PROC_ROOT="$tmp/proc"
  PLUMOS_BUSYBOX="$root/network/bin/busybox"
  PLUMOS_WIFI_RECOVERY_SETTLE_SECONDS=0
)

env "${recovery_env[@]}" "$root/bin/plumos-wifi-recovery" start
status="$(env "${recovery_env[@]}" "$root/bin/plumos-wifi-recovery" status)"
grep -Fxq 'wifi_requested=1' <<<"$status"
grep -Fxq 'monitor_running=1' <<<"$status"
grep -Fxq 'mode=kernel_uevent' <<<"$status"

env "${recovery_env[@]}" ACTION=add SUBSYSTEM=net INTERFACE=wlan0 \
  "$root/bin/plumos-wifi-uevent"
grep -Fxq -- '--wifi on' "$TEST_RECOVERY_CALLS"
[ "$(wc -l <"$TEST_RECOVERY_CALLS" | tr -d ' ')" -eq 1 ]

env "${recovery_env[@]}" ACTION=add SUBSYSTEM=net INTERFACE=eth0 \
  "$root/bin/plumos-wifi-uevent"
env "${recovery_env[@]}" ACTION=remove SUBSYSTEM=net INTERFACE=wlan0 \
  "$root/bin/plumos-wifi-uevent"
[ "$(wc -l <"$TEST_RECOVERY_CALLS" | tr -d ' ')" -eq 1 ]

rm -f "$TEST_RECOVERY_CONNECTED"
env "${recovery_env[@]}" ACTION=add SUBSYSTEM=usb PRODUCT=0bda/1a2b/200 \
  "$root/bin/plumos-wifi-uevent"
[ "$(wc -l <"$TEST_RECOVERY_CALLS" | tr -d ' ')" -eq 2 ]

# Pixel2's tested RTL8821CU adapter enumerates directly as c820. Both direct
# aliases must load the driver without relying on a later wlan uevent.
rm -f "$TEST_RECOVERY_CONNECTED"
env "${recovery_env[@]}" ACTION=add SUBSYSTEM=usb PRODUCT=0bda/c811/200 \
  "$root/bin/plumos-wifi-uevent"
rm -f "$TEST_RECOVERY_CONNECTED"
env "${recovery_env[@]}" ACTION=add SUBSYSTEM=usb PRODUCT=0BDA/C820/200 \
  "$root/bin/plumos-wifi-uevent"
[ "$(wc -l <"$TEST_RECOVERY_CALLS" | tr -d ' ')" -eq 4 ]

# Queued USB/net add events after DHCP must not repeat service and DHCP work.
env "${recovery_env[@]}" ACTION=add SUBSYSTEM=net INTERFACE=wlan0 \
  "$root/bin/plumos-wifi-uevent"
[ "$(wc -l <"$TEST_RECOVERY_CALLS" | tr -d ' ')" -eq 4 ]
grep -q 'recover_skipped reason=already_connected' \
  "$root/logs/wifi-recovery.log"

env "${recovery_env[@]}" ACTION=add SUBSYSTEM=usb PRODUCT=0bda/8179/200 \
  "$root/bin/plumos-wifi-uevent"
[ "$(wc -l <"$TEST_RECOVERY_CALLS" | tr -d ' ')" -eq 4 ]

printf 'pixel2_wifi_recovery=result-ok\n'
