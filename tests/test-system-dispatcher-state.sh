#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"
if [ "${1:-}" != --inside ]; then
    exec docker run --rm --platform linux/arm64 \
        -v "$ROOT_DIR:/work:ro" -w /work "$IMAGE" \
        ./tests/test-system-dispatcher-state.sh --inside
fi

work=$(mktemp -d /tmp/plumos-dispatcher-state.XXXXXX)
trap 'rm -rf "$work"' EXIT
state="$work/state"
slots="$work/system-slots"
mkdir -p "$state" "$slots"
printf 'slot-a\n' >"$slots/system-a.squashfs"
printf 'slot-b\n' >"$slots/system-b.squashfs"
(cd "$slots" && sha256sum system-a.squashfs >system-a.sha256)
(cd "$slots" && sha256sum system-b.squashfs >system-b.sha256)

run_dispatcher() {
    PLUMOS_DISPATCHER_TEST=1 \
    PLUMOS_DISPATCHER_BUSYBOX=/bin/busybox \
    PLUMOS_DISPATCHER_STATE_ROOT="$state" \
    PLUMOS_DISPATCHER_SYSTEM_ROOT="$slots" \
        /bin/busybox sh /work/rootfs/pixel2-dispatcher/init
}

printf 'a\n' >"$state/system-active"
output=$(run_dispatcher)
grep -q '^selected=a$' <<<"$output"
grep -q '^reason=active$' <<<"$output"

printf 'b\n' >"$state/system-pending"
output=$(run_dispatcher)
grep -q '^selected=b$' <<<"$output"
grep -q '^reason=pending$' <<<"$output"
grep -q '^b$' "$state/system-pending-attempted"

output=$(run_dispatcher)
grep -q '^selected=a$' <<<"$output"
grep -q '^reason=rollback$' <<<"$output"
test ! -e "$state/system-pending"
test ! -e "$state/system-pending-attempted"
grep -q 'system_rolled_back' "$state/last-result.json"

printf 'b\n' >"$state/system-pending"
printf 'corrupt\n' >"$slots/system-b.squashfs"
output=$(run_dispatcher)
grep -q '^selected=a$' <<<"$output"
grep -q '^reason=pending-invalid$' <<<"$output"
test ! -e "$state/system-pending"

printf 'corrupt\n' >"$slots/system-a.squashfs"
printf 'slot-b\n' >"$slots/system-b.squashfs"
output=$(run_dispatcher)
grep -q '^selected=b$' <<<"$output"
grep -q '^reason=active-invalid$' <<<"$output"
grep -q '^b$' "$state/system-active"

printf 'system_dispatcher_state=result-ok\n'
