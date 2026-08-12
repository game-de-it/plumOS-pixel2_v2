#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
for script in \
    "$ROOT_DIR/scripts/build-system-rootfs.sh" \
    "$ROOT_DIR/scripts/build-adbd-overlay.sh" \
    "$ROOT_DIR/scripts/install-kernel-runtime.sh" \
    "$ROOT_DIR/scripts/install-frontend-rootfs.sh" \
    "$ROOT_DIR/scripts/docker-build.sh" \
    "$ROOT_DIR/scripts/build-frontend-component.sh" \
    "$ROOT_DIR/scripts/build-retroarch.sh" \
    "$ROOT_DIR/scripts/build-libretro-cores.sh" \
    "$ROOT_DIR/scripts/build-app-layer.sh" \
    "$ROOT_DIR/scripts/verify-app-layer.sh" \
    "$ROOT_DIR/scripts/verify-system-rootfs.sh" \
    "$ROOT_DIR/scripts/verify-system-dispatcher.sh" \
    "$ROOT_DIR/rootfs/pixel2-dispatcher/init" \
    "$ROOT_DIR/rootfs/pixel2/sbin/init" \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/systemd/systemd" \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd" \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/20-usb-wifi" \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/30-ssh"; do
    bash -n "$script"
done

bash -n "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
bash -n "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/50-update-health"
bash -n "$ROOT_DIR/rootfs/pixel2/usr/bin/plumos-diagnostics"
grep -q 'PLUMOS_DEVICE_ID=pixel2' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
grep -q 'PLUMOS_FBDEV_ROTATION=ccw' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
grep -q 'east-confirm' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
grep -q 'input-map.env' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
grep -q 'pixel2_joypad' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
grep -q 'physical_yres' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h"
grep -q 'usb_role' "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
grep -q 'transport=nonblocking FunctionFS' "$ROOT_DIR/scripts/build-adbd-overlay.sh"
! grep -q 'PLUMOS_ADBD_LEGACY_FFS' "$ROOT_DIR/scripts/build-adbd-overlay.sh"
grep -q 'adb-serial' "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
grep -q 'recover_adbd' "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
grep -q 'adb_opted_in' "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
grep -q 'explicit-opt-in-required' "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
grep -q '/mnt/plumos-user/plumos-enable-adb' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
grep -q 'pixel2_usb_present' "$ROOT_DIR/scripts/pixel2-adb.sh"
grep -q 'PLUMOS_SYS' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'app-layer-verified' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
grep -q 'system-booted' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/50-update-health"

! grep -q '\$bb mountpoint' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'ROOTFS_DIR/dev/pts' "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q 'ROOTFS_DIR/mnt/plumos' "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q 'mnt/plumos-user' "$ROOT_DIR/scripts/verify-system-rootfs.sh"
grep -q 'ROOTFS_DIR/flash' "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q 'usr/lib/systemd/systemd' "$ROOT_DIR/scripts/verify-system-rootfs.sh"
grep -q 'is_mounted /storage' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'umount /.plumos-dispatcher-old' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'stock-handoff.log' "$ROOT_DIR/rootfs/pixel2/usr/lib/systemd/systemd"
grep -q 'mount-source storage=stock-initramfs' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q '5.10.198' "$ROOT_DIR/scripts/install-kernel-runtime.sh"
grep -q 'stock-pixel2' "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q 'stock-pixel2' "$ROOT_DIR/scripts/verify-system-rootfs.sh"
grep -q 'system-pending-attempted' "$ROOT_DIR/rootfs/pixel2-dispatcher/init"
grep -q 'system_rolled_back' "$ROOT_DIR/rootfs/pixel2-dispatcher/init"
grep -q 'pivot_root . .plumos-dispatcher-old' \
    "$ROOT_DIR/rootfs/pixel2-dispatcher/init"
! grep -q 'switch_root /newroot' "$ROOT_DIR/rootfs/pixel2-dispatcher/init"
grep -q 'for mountpoint in sys flash storage' \
    "$ROOT_DIR/rootfs/pixel2-dispatcher/init"
grep -q 'mount --move /dev /newroot/dev' \
    "$ROOT_DIR/rootfs/pixel2-dispatcher/init"
grep -q 'mount --move /proc /newroot/proc' \
    "$ROOT_DIR/rootfs/pixel2-dispatcher/init"
! grep -q 'for mountpoint in dev ' "$ROOT_DIR/rootfs/pixel2-dispatcher/init"
grep -q 'system-dispatcher.log' "$ROOT_DIR/rootfs/pixel2-dispatcher/init"
grep -q 'PLUMOS_DISPATCHER_TEST' "$ROOT_DIR/rootfs/pixel2-dispatcher/init"
grep -q 'fixed-dispatcher,system-a,system-b' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'DISPATCHER_DIR/usr/lib/systemd' "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q 'DISPATCHER_DIR/dev/pts' "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q 'DISPATCHER_DIR/storage' "$ROOT_DIR/scripts/build-system-rootfs.sh"

if grep -R -E -i '(rocknix|emuelec|batocera|knulli)' \
    "$ROOT_DIR/rootfs/pixel2" >/dev/null; then
    printf 'error: foreign distribution identity in source overlay\n' >&2
    exit 1
fi
printf 'system_rootfs_scripts=result-ok\n'
