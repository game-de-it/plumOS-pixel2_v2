#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SHIM="$ROOT_DIR/package/portmaster-pixel2/plumos/apps/portmaster/adapter/shims/df"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/plumos-portmaster-df.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

cat >"$TMP/busybox" <<'EOF'
#!/bin/sh
printf '%s\n' "$@"
EOF
chmod 0755 "$TMP/busybox"

actual="$(PLUMOS_BUSYBOX="$TMP/busybox" "$SHIM" -PT /roms/ports/Test.sh)"
expected=$'df\n-PT\n/roms/ports/Test.sh'
[ "$actual" = "$expected" ] || {
  printf 'error: PortMaster df shim did not delegate to BusyBox df\n' >&2
  exit 1
}

printf 'portmaster_df=result-ok\n'
