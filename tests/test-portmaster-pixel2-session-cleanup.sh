#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CLEANUP="$ROOT_DIR/package/portmaster-pixel2/plumos/bin/plumos-portmaster-session-cleanup"
work="$(mktemp -d /tmp/plumos-portmaster-session-test.XXXXXX)"
trap 'rm -rf "$work"' EXIT

mkdir -p "$work/proc/101" "$work/proc/102" "$work/proc/103" \
  "$work/proc/104" "$work/proc/900"
printf 'Name:\tport-a\nPPid:\t1\n' > "$work/proc/101/status"
printf 'Name:\tport-b\nPPid:\t1\n' > "$work/proc/102/status"
printf 'Name:\tother\nPPid:\t1\n' > "$work/proc/103/status"
printf 'Name:\thelper\nPPid:\t900\n' > "$work/proc/104/status"
printf 'Name:\tcleanup\nPPid:\t1\n' > "$work/proc/900/status"
printf 'PATH=/bin\0PLUMOS_PORTMASTER_SESSION_ID=test-session\0' > "$work/proc/101/environ"
printf 'PLUMOS_PORTMASTER_SESSION_ID=test-session\0' > "$work/proc/102/environ"
printf 'PLUMOS_PORTMASTER_SESSION_ID=other-session\0' > "$work/proc/103/environ"
printf 'PLUMOS_PORTMASTER_SESSION_ID=test-session\0' > "$work/proc/104/environ"
printf 'PLUMOS_PORTMASTER_SESSION_ID=test-session\0' > "$work/proc/900/environ"

cat > "$work/kill" <<'EOF'
#!/bin/sh
printf '%s %s session=%s\n' "$1" "$2" "${PLUMOS_PORTMASTER_SESSION_ID-unset}" \
  >> "$FAKE_KILL_LOG"
rm -rf "$FAKE_PROC_ROOT/$2"
EOF
chmod 0755 "$work/kill"

FAKE_PROC_ROOT="$work/proc" \
FAKE_KILL_LOG="$work/kills.log" \
PLUMOS_PORTMASTER_SESSION_ID=test-session \
PLUMOS_PORTMASTER_PROC_ROOT="$work/proc" \
PLUMOS_PORTMASTER_KILL_BIN="$work/kill" \
PLUMOS_PORTMASTER_SLEEP_BIN=true \
PLUMOS_PORTMASTER_SESSION_LOG="$work/session.log" \
PLUMOS_PORTMASTER_CLEANUP_PID=900 \
  /bin/sh "$CLEANUP"

grep -q '^-TERM 101 session=cleanup:test-session$' "$work/kills.log"
grep -q '^-TERM 102 session=cleanup:test-session$' "$work/kills.log"
! grep -q '103' "$work/kills.log"
! grep -q '104' "$work/kills.log"
! grep -q '900' "$work/kills.log"
test -d "$work/proc/103"
test -d "$work/proc/104"
grep -q 'result=clean' "$work/session.log"

printf 'portmaster_pixel2_session_cleanup=result-ok\n'
