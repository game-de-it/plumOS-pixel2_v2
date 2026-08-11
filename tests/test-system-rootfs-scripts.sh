#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
for script in \
    "$ROOT_DIR/scripts/build-system-rootfs.sh" \
    "$ROOT_DIR/scripts/build-adbd-overlay.sh" \
    "$ROOT_DIR/scripts/install-kernel-runtime.sh" \
    "$ROOT_DIR/scripts/verify-system-rootfs.sh" \
    "$ROOT_DIR/rootfs/pixel2/sbin/init" \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd" \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/20-usb-wifi" \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/30-ssh"; do
    bash -n "$script"
done

! grep -q '\$bb mountpoint' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'ROOTFS_DIR/dev/pts' "$ROOT_DIR/scripts/build-system-rootfs.sh"

if grep -R -E -i '(rocknix|emuelec|batocera|knulli)' \
    "$ROOT_DIR/rootfs/pixel2" >/dev/null; then
    printf 'error: foreign distribution identity in source overlay\n' >&2
    exit 1
fi
printf 'system_rootfs_scripts=result-ok\n'
