#!/bin/sh
set -u

PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/plumos}"
RUNTIME_ROOT="${PLUMOS_RUNTIME_ROOT:-/run/plumos}"
RUN_DIR="${PLUMOS_SSH_RUN_DIR:-$RUNTIME_ROOT/ssh}"
STATE_DIR="${PLUMOS_SSH_STATE_DIR:-$PLUMOS_ROOT/config/ssh}"
ROOT_HOME="${PLUMOS_ROOT_HOME:-/root}"
AUTHORIZED_KEYS="${PLUMOS_SSH_AUTHORIZED_KEYS:-$ROOT_HOME/.ssh/authorized_keys}"
HOST_KEY="$STATE_DIR/dropbear_ed25519_host_key"
PID_FILE="$RUN_DIR/dropbear.pid"
LOG_DIR="$PLUMOS_ROOT/logs"
LOG_FILE="$LOG_DIR/ssh.log"
PORT="${PLUMOS_SSH_PORT:-22}"
DROPBEAR="${PLUMOS_DROPBEAR:-/usr/sbin/dropbear}"
DROPBEARKEY="${PLUMOS_DROPBEARKEY:-/usr/bin/dropbearkey}"
PASSWORD_CONTROL="${PLUMOS_SSH_PASSWORD_CONTROL:-$PLUMOS_ROOT/bin/plumos-ssh-password}"

mkdir -p "$RUN_DIR" "$STATE_DIR" "$LOG_DIR" "$ROOT_HOME/.ssh"
chmod 0700 "$STATE_DIR" "$ROOT_HOME/.ssh" 2>/dev/null || true
[ ! -e "$AUTHORIZED_KEYS" ] || chmod 0600 "$AUTHORIZED_KEYS" 2>/dev/null || true

log() {
  printf '%s service=ssh %s\n' \
    "$(date -u '+%Y-%m-%dT%H:%M:%SZ' 2>/dev/null || true)" "$*" >>"$LOG_FILE"
}

pid_running() {
  [ -s "$PID_FILE" ] || return 1
  pid="$(sed -n '1p' "$PID_FILE" 2>/dev/null | tr -d '[:space:]')"
  case "$pid" in ''|*[!0-9]*) return 1 ;; esac
  [ "$(awk '{ print $3 }' "/proc/$pid/stat" 2>/dev/null || true)" != Z ] || return 1
  cmdline="$(tr '\000' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true)"
  case "$cmdline" in *dropbear*) return 0 ;; esac
  return 1
}

if pid_running; then
  log "result=already-running pid=$(cat "$PID_FILE") port=$PORT"
  exit 0
fi
rm -f "$PID_FILE"

[ -x "$DROPBEAR" ] || {
  log "result=failed reason=dropbear-missing path=$DROPBEAR"
  exit 1
}
[ -x "$DROPBEARKEY" ] || {
  log "result=failed reason=dropbearkey-missing path=$DROPBEARKEY"
  exit 1
}
[ -x "$PASSWORD_CONTROL" ] || {
  log "result=failed reason=password-control-missing path=$PASSWORD_CONTROL"
  exit 1
}

if ! "$PASSWORD_CONTROL" ensure-default >>"$LOG_FILE" 2>&1; then
  log "result=failed reason=password-setup"
  exit 1
fi

if [ ! -s "$HOST_KEY" ]; then
  legacy_key="$PLUMOS_ROOT/ssh/dropbear_ed25519_host_key"
  if [ -s "$legacy_key" ]; then
    cp "$legacy_key" "$HOST_KEY" || exit 1
    chmod 0600 "$HOST_KEY" 2>/dev/null || true
    log "result=migrated-host-key source=$legacy_key"
  else
    "$DROPBEARKEY" -t ed25519 -f "$HOST_KEY" >>"$LOG_FILE" 2>&1 || {
      log "result=failed reason=host-key-generation"
      exit 1
    }
  fi
fi

log "result=starting port=$PORT auth=password,pubkey"
"$DROPBEAR" -E -P "$PID_FILE" -r "$HOST_KEY" -p "$PORT" \
  -K 30 -I 300 >>"$LOG_FILE" 2>&1
rc=$?
sleep 1
if [ "$rc" -eq 0 ] && pid_running; then
  log "result=started pid=$(cat "$PID_FILE") port=$PORT"
  exit 0
fi
log "result=failed reason=listener rc=$rc"
exit 1
