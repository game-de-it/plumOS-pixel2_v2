#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SHIM="$ROOT_DIR/package/portmaster-pixel2/plumos/apps/portmaster/adapter/shims/pgrep"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/plumos-portmaster-pgrep.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/proc/101" "$TMP/proc/202"
printf 'love.aarch64\0--game\0' >"$TMP/proc/101/cmdline"
printf 'love.aarch64\n' >"$TMP/proc/101/comm"
printf 'plumos-frontend-pixel2\0--renderer\0fbdev\0' \
  >"$TMP/proc/202/cmdline"
printf 'plumos-frontend-pixel2\n' >"$TMP/proc/202/comm"

result="$(PLUMOS_PORTMASTER_PROC_ROOT="$TMP/proc" "$SHIM" -f 'love[.]aarch64')"
[ "$result" = 101 ]
result="$(PLUMOS_PORTMASTER_PROC_ROOT="$TMP/proc" "$SHIM" '^plumos-frontend')"
[ "$result" = 202 ]
if PLUMOS_PORTMASTER_PROC_ROOT="$TMP/proc" "$SHIM" -f missing >/dev/null; then
  printf 'error: pgrep shim matched an absent process\n' >&2
  exit 1
fi
if PLUMOS_PORTMASTER_PROC_ROOT="$TMP/proc" "$SHIM" -x love >/dev/null 2>&1; then
  printf 'error: pgrep shim accepted an unsupported option\n' >&2
  exit 1
fi

grep -q 'ADAPTER_DIR}/shims:${PLUMOS_ROOT}/bin' \
  "$ROOT_DIR/package/portmaster-pixel2/plumos/bin/plumos-portmaster-launch"
printf 'portmaster_pgrep=result-ok\n'
