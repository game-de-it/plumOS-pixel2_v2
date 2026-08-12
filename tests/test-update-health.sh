#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"
if [ "${1:-}" != --inside ]; then
    exec docker run --rm --platform linux/arm64 \
        -v "$ROOT_DIR:/work:ro" -w /work "$IMAGE" \
        ./tests/test-update-health.sh --inside
fi
work=$(mktemp -d /tmp/plumos-update-health.XXXXXX)
trap 'rm -rf "$work"' EXIT
state="$work/state"
mkdir -p "$state"
helper="$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/50-update-health"

printf 'a\n' >"$state/system-active"
printf 'b\n' >"$state/system-pending"
printf 'b\n' >"$state/system-pending-attempted"
printf 'a\n' >"$state/system-booted"
if PLUMOS_UPDATE_BUSYBOX=/bin/busybox PLUMOS_UPDATE_STATE_ROOT="$state" PLUMOS_UPDATE_HEALTH_LOG="$work/log" \
    "$helper" promote; then
    printf 'error: mismatched booted slot was promoted\n' >&2
    exit 1
fi
grep -q '^a$' "$state/system-active"
test -e "$state/system-pending"

printf 'b\n' >"$state/system-booted"
PLUMOS_UPDATE_BUSYBOX=/bin/busybox PLUMOS_UPDATE_STATE_ROOT="$state" PLUMOS_UPDATE_HEALTH_LOG="$work/log" \
    "$helper" promote
grep -q '^b$' "$state/system-active"
test ! -e "$state/system-pending"
test ! -e "$state/system-pending-attempted"
grep -q 'system_healthy' "$state/last-result.json"

printf 'update_health=result-ok\n'
