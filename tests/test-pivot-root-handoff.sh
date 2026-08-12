#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"
if [ "${1:-}" != --inside ]; then
    exec docker run --rm --privileged --platform linux/arm64 \
        -v "$ROOT_DIR:/work:ro" -w /work "$IMAGE" \
        ./tests/test-pivot-root-handoff.sh --inside
fi

mkdir -p /newroot
mount -t tmpfs tmpfs /newroot
mkdir -p /newroot/.plumos-dispatcher-old /newroot/bin /newroot/sbin \
    /newroot/proc
cp /bin/busybox /newroot/bin/busybox
ln -s busybox /newroot/bin/sh
cp /work/tests/fixtures/pivot-root-init /newroot/sbin/init
chmod 0755 /newroot/sbin/init
mount -t proc proc /newroot/proc
cd /newroot
/bin/busybox pivot_root . .plumos-dispatcher-old
cd /
exec /sbin/init
