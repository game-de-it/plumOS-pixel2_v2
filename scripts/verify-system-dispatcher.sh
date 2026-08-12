#!/usr/bin/env bash
set -euo pipefail

IMAGE="${1:-}"
[ -f "$IMAGE" ] || { printf 'usage: %s SYSTEM\n' "$0" >&2; exit 2; }
ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

if ! command -v unsquashfs >/dev/null 2>&1; then
    IMAGE_PATH="$(CDPATH= cd -- "$(dirname -- "$IMAGE")" && pwd)/$(basename -- "$IMAGE")"
    case "$IMAGE_PATH" in "$ROOT_DIR"/*) ;; *) exit 2 ;; esac
    relative=${IMAGE_PATH#"$ROOT_DIR"/}
    TOOL_IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"
    exec docker run --rm --platform linux/arm64 \
        -v "$ROOT_DIR:/work" -w /work "$TOOL_IMAGE" \
        ./scripts/verify-system-dispatcher.sh "/work/$relative"
fi

tmp="$(mktemp -d /tmp/plumos-pixel2-dispatcher.XXXXXX)"
trap 'rm -rf "$tmp"' EXIT
unsquashfs -d "$tmp/rootfs" "$IMAGE" >/dev/null
test -x "$tmp/rootfs/init"
test -x "$tmp/rootfs/bin/busybox"
test -L "$tmp/rootfs/bin/sh"
grep -q '/flash/system-slots' "$tmp/rootfs/init"
grep -q 'system-pending-attempted' "$tmp/rootfs/init"
grep -q 'system_rolled_back' "$tmp/rootfs/init"
grep -q 'sha256sum' "$tmp/rootfs/init"
grep -q 'switch_root /newroot /sbin/init' "$tmp/rootfs/init"
if LC_ALL=C grep -R -a -E -i -n '(rocknix|emuelec|batocera|knulli|miyoo|v90s)' \
    "$tmp/rootfs" >/dev/null; then
    printf 'error: foreign device or distribution identity in dispatcher\n' >&2
    exit 1
fi
printf 'system_dispatcher=result-ok image=%s\n' "$IMAGE"
