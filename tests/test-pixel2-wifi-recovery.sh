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
  "$root/config/network" "$root/logs" "$run" "$tmp/system"
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
    if [ "${TEST_RECOVERY_FAIL_ONCE:-0}" = 1 ] &&
        [ ! -e "$TEST_RECOVERY_FAIL_MARKER" ]; then
      : >"$TEST_RECOVERY_FAIL_MARKER"
      printf 'result=failed\nstage=wpa_completed\n'
      exit 1
    fi
    : >"$TEST_RECOVERY_CONNECTED"
    printf 'result=connected\nip=192.0.2.120\n'
    ;;
  *) exit 2 ;;
esac
EOF
chmod 0755 "$root/network/bin/busybox" "$root/bin/plumos-network-control"
cat >"$tmp/system/usb-host-control" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >>"$TEST_USB_ROLE_CALLS"
EOF
chmod 0755 "$tmp/system/usb-host-control"

printf 'network={}\n' >"$root/config/wpa_supplicant.conf"
printf '{"wifi_enabled": true}\n' >"$root/config/system/settings.json"
export TEST_RECOVERY_CALLS="$tmp/recovery-calls"
export TEST_RECOVERY_CONNECTED="$tmp/recovery-connected"
export TEST_USB_ROLE_CALLS="$tmp/usb-role-calls"
export TEST_RECOVERY_FAIL_MARKER="$tmp/recovery-fail-once"

recovery_env=(
  PLUMOS_ROOT="$root"
  PLUMOS_SDCARD_ROOT="$tmp/user"
  PLUMOS_RUNTIME_ROOT="$run"
  PLUMOS_PROC_ROOT="$tmp/proc"
  PLUMOS_BUSYBOX="$root/network/bin/busybox"
  PLUMOS_WIFI_RECOVERY_SETTLE_SECONDS=0
  PLUMOS_PIXEL2_USB_HOST_CONTROL="$tmp/system/usb-host-control"
  PLUMOS_PIXEL2_USB_HOST_TRANSITION_MARKER="$tmp/usb-host-probe-active"
  PLUMOS_PIXEL2_USB_HOST_SLEEP_MARKER="$tmp/sleep-usb-recovery"
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
test ! -e "$TEST_USB_ROLE_CALLS"

env "${recovery_env[@]}" ACTION=remove SUBSYSTEM=usb DEVTYPE=usb_device \
  PRODUCT=0bda/c811/200 "$root/bin/plumos-wifi-uevent"
grep -Fxq release "$TEST_USB_ROLE_CALLS"

before_role_calls="$(wc -l <"$TEST_USB_ROLE_CALLS" | tr -d ' ')"
: >"$tmp/usb-host-probe-active"
env "${recovery_env[@]}" ACTION=remove SUBSYSTEM=usb DEVTYPE=usb_device \
  PRODUCT=0bda/c820/200 "$root/bin/plumos-wifi-uevent"
[ "$(wc -l <"$TEST_USB_ROLE_CALLS" | tr -d ' ')" -eq "$before_role_calls" ]
rm -f "$tmp/usb-host-probe-active"

before_role_calls="$(wc -l <"$TEST_USB_ROLE_CALLS" | tr -d ' ')"
: >"$tmp/sleep-usb-recovery"
env "${recovery_env[@]}" ACTION=remove SUBSYSTEM=usb DEVTYPE=usb_device \
  PRODUCT=0bda/c820/200 "$root/bin/plumos-wifi-uevent"
[ "$(wc -l <"$TEST_USB_ROLE_CALLS" | tr -d ' ')" -eq "$before_role_calls" ]
rm -f "$tmp/sleep-usb-recovery"

env "${recovery_env[@]}" ACTION=remove SUBSYSTEM=usb DEVTYPE=usb_device \
  PRODUCT=0bda/1a2b/200 "$root/bin/plumos-wifi-uevent"
grep -Fxq release-later "$TEST_USB_ROLE_CALLS"

before_role_calls="$(wc -l <"$TEST_USB_ROLE_CALLS" | tr -d ' ')"
env "${recovery_env[@]}" ACTION=remove SUBSYSTEM=usb DEVTYPE=usb_device \
  PRODUCT=1d6b/0002/510 "$root/bin/plumos-wifi-uevent"
[ "$(wc -l <"$TEST_USB_ROLE_CALLS" | tr -d ' ')" -eq "$before_role_calls" ]

env "${recovery_env[@]}" ACTION=change SUBSYSTEM=power_supply \
  POWER_SUPPLY_NAME=usb "$root/bin/plumos-wifi-uevent"
env "${recovery_env[@]}" ACTION=change SUBSYSTEM=power_supply \
  POWER_SUPPLY_NAME=battery "$root/bin/plumos-wifi-uevent"
env "${recovery_env[@]}" ACTION=change SUBSYSTEM=extcon \
  "$root/bin/plumos-wifi-uevent"
[ "$(grep -c '^reconcile$' "$TEST_USB_ROLE_CALLS")" -eq 2 ]

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

# A boot/sync attempt may reach wlan0 before the UGREEN c811 association is
# ready. One failed attempt gets exactly one delayed follow-up; it must not
# become a recurring monitor.
env "${recovery_env[@]}" "$root/bin/plumos-wifi-recovery" stop
rm -f "$TEST_RECOVERY_CONNECTED" "$TEST_RECOVERY_CALLS" \
  "$TEST_RECOVERY_FAIL_MARKER"
env "${recovery_env[@]}" TEST_RECOVERY_FAIL_ONCE=1 \
  PLUMOS_WIFI_RECOVERY_INITIAL_RETRY_SECONDS=0 \
  "$root/bin/plumos-wifi-recovery" sync
retry_wait=0
while [ "$retry_wait" -lt 30 ]; do
  [ -e "$TEST_RECOVERY_CONNECTED" ] && break
  sleep 0.1
  retry_wait=$((retry_wait + 1))
done
[ -e "$TEST_RECOVERY_CONNECTED" ]
[ "$(wc -l <"$TEST_RECOVERY_CALLS" | tr -d ' ')" -eq 2 ]
grep -q 'initial_recovery_retry_scheduled delay=0' \
  "$root/logs/wifi-recovery.log"

before_unrelated_calls="$(wc -l <"$TEST_RECOVERY_CALLS" | tr -d ' ')"
env "${recovery_env[@]}" ACTION=add SUBSYSTEM=usb PRODUCT=0bda/8179/200 \
  "$root/bin/plumos-wifi-uevent"
[ "$(wc -l <"$TEST_RECOVERY_CALLS" | tr -d ' ')" -eq "$before_unrelated_calls" ]

printf 'pixel2_wifi_recovery=result-ok\n'
