#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
test -x "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/05-gpu"
grep -q 'modprobe panfrost' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/05-gpu"
grep -q '/dev/shm' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'ip link set lo up' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'ip addr add 127.0.0.1/8 dev lo' "$ROOT_DIR/rootfs/pixel2/sbin/init"
for script in \
    "$ROOT_DIR/scripts/build-system-rootfs.sh" \
    "$ROOT_DIR/scripts/build-adbd-overlay.sh" \
    "$ROOT_DIR/scripts/install-kernel-runtime.sh" \
    "$ROOT_DIR/scripts/build-rtl8821cu-pixel2.sh" \
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
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-adbd-watchdog" \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-usb-host-reenumerate" \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/20-usb-wifi" \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/30-ssh" \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/35-network-services"; do
    bash -n "$script"
done

python3 -m py_compile \
    "$ROOT_DIR/scripts/plumos-system-update.py" \
    "$ROOT_DIR/scripts/build-pixel2-update-package.py"

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
! grep -q '^USB_ONLINE=' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
test "$(sha256sum "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd" | awk '{print $1}')" = \
    6d18796073275d667889a9d2c5b9e2df2eae298003c2bbb94f2d937579c81d22
test "$(sha256sum "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-adbd-watchdog" | awk '{print $1}')" = \
    b67891ffd006701d96e82442491ec89eacd9866f65e077946e12d0fde908b876
grep -q 'result=skipped reason=adb-priority' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-usb-host-reenumerate"
grep -q 'result=disabled reason=adb-priority' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/20-usb-wifi"
test -x "$ROOT_DIR/tests/test-pixel2-usb-host-reenumerate.sh"
"$ROOT_DIR/tests/test-pixel2-usb-host-reenumerate.sh"
grep -q 'ff300000.usb' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-usb-host-reenumerate"
grep -q 'usb-upstream-online' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-usb-host-reenumerate"
! grep -q -- '-DPLUMOS_ADBD_LEGACY_FFS' "$ROOT_DIR/scripts/build-adbd-overlay.sh"
! grep -q -- '-DPLUMOS_ADBD_SYNC_FFS' "$ROOT_DIR/scripts/build-adbd-overlay.sh"
grep -q 'transport=nonblocking FunctionFS' \
    "$ROOT_DIR/scripts/build-adbd-overlay.sh"
! grep -q '0001-plumos-transport-state.patch' \
    "$ROOT_DIR/scripts/build-adbd-overlay.sh"
grep -q 'adb-serial' "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
! grep -q 'busybox.*uevent' "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
! grep -q 'watchdog-replug reason=transport-offline' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
! grep -q 'schedule_recovery' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
! grep -q 'action=watchdog-' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
! grep -Eq 'services\.conf|usb_mode|adb_enabled|recover|replug|status' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
grep -q 'event=daemon-missing action=restart' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-adbd-watchdog"
! grep -Eq 'UDC|usb_role|functionfs|transport|configured|not attached' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/15-adbd-watchdog"
grep -q 'plumos-network-services' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/35-network-services"
grep -q 'plumos-wifi-recovery' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-control"
grep -q '"$RECOVERY" sync' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/20-usb-wifi"
grep -q 'plumos-wifi-recovery" sync' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
grep -q 'busybox uevent' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-wifi-recovery"
grep -q 'owner=plumos-network-services' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/30-ssh"
! grep -q 'dropbear ' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/30-ssh"
grep -q 'start-enabled' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/35-network-services"
grep -q 'result=degraded' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/35-network-services"
grep -q ') &' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/35-network-services"
grep -q '/mnt/plumos/ssh/libexec/sftp-server' \
    "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q 'pixel2_usb_present' "$ROOT_DIR/scripts/pixel2-adb.sh"
grep -q 'PLUMOS_SYS' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'app-layer-selected' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
! grep -q 'sha256sum -c' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
grep -q 'system-booted' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/50-update-health"
grep -q 'plumos-system-update mark-healthy' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/50-update-health"
grep -q 'apply_pending_update' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q '"$updater" apply-pending' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'reason=no-pending-state' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'runtime-pending.json' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'runtime-transaction.json' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'verify-runtime' "$ROOT_DIR/scripts/plumos-system-update.py"
grep -q 'generate-pixel2-update-progress.py' \
    "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q 'update_finalize update_rollback update_error' \
    "$ROOT_DIR/scripts/verify-system-rootfs.sh"

progress_dir=$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-progress.XXXXXX")
trap 'rm -rf "$progress_dir"' EXIT
python3 "$ROOT_DIR/scripts/generate-pixel2-update-progress.py" \
    --output-dir "$progress_dir"
for frame in prepare resize userdata verify start error update_verify \
    update_runtime update_system update_finalize update_rollback update_error; do
    test -f "$progress_dir/$frame.raw"
    [ "$(stat -f '%z' "$progress_dir/$frame.raw" 2>/dev/null || \
        stat -c '%s' "$progress_dir/$frame.raw")" -eq $((480 * 640 * 4)) ]
done
grep -q 'build-pixel2-update-package.py' "$ROOT_DIR/scripts/docker-build.sh"

! grep -q '\$bb mountpoint' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'ROOTFS_DIR/dev/pts' "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q 'ROOTFS_DIR/mnt/plumos' "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q 'mnt/plumos-user' "$ROOT_DIR/scripts/verify-system-rootfs.sh"
grep -q 'user-data=first-boot-provisioner' \
    "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'ROOTFS_DIR/flash' "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q 'usr/lib/systemd/systemd' "$ROOT_DIR/scripts/verify-system-rootfs.sh"
grep -q 'is_mounted /storage' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'umount /.plumos-dispatcher-old' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q 'stock-handoff.log' "$ROOT_DIR/rootfs/pixel2/usr/lib/systemd/systemd"
grep -q 'mount-source storage=stock-initramfs' "$ROOT_DIR/rootfs/pixel2/sbin/init"
grep -q '5.10.198' "$ROOT_DIR/scripts/install-kernel-runtime.sh"
grep -q '^rtlwifi/rtl8188eufw.bin$' \
    "$ROOT_DIR/scripts/install-kernel-runtime.sh"
grep -q 'build-rtl8821cu-pixel2.sh' \
    "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q -- '--verify-output' "$ROOT_DIR/scripts/build-system-rootfs.sh"
grep -q 'kernel-modules) exec ./scripts/build-rtl8821cu-pixel2.sh' \
    "$ROOT_DIR/scripts/docker-build.sh"
grep -q '96c65c58b544241178638e810b333dcc9aa26b91' \
    "$ROOT_DIR/scripts/build-rtl8821cu-pixel2.sh"
grep -q 'EXPECTED_STOCK_SRCVERSION=33E331B2DEB16477EAAB1D6' \
    "$ROOT_DIR/scripts/build-rtl8821cu-pixel2.sh"
grep -q -- '--disable DEBUG_SPINLOCK' \
    "$ROOT_DIR/scripts/build-rtl8821cu-pixel2.sh"
grep -q -- '--disable FTRACE' \
    "$ROOT_DIR/scripts/build-rtl8821cu-pixel2.sh"
grep -q -- '--disable KALLSYMS' \
    "$ROOT_DIR/scripts/build-rtl8821cu-pixel2.sh"
grep -q -- '--disable FUNCTION_TRACER' \
    "$ROOT_DIR/scripts/build-rtl8821cu-pixel2.sh"
grep -q 'stock_module_struct_size' \
    "$ROOT_DIR/scripts/build-rtl8821cu-pixel2.sh"
grep -q 'stock_module_exit_offset' \
    "$ROOT_DIR/scripts/build-rtl8821cu-pixel2.sh"
grep -q 'lib/modules/\$release/extra/8821cu.ko' \
    "$ROOT_DIR/scripts/install-kernel-runtime.sh"
grep -q 'modinfo -b.*8821cu' \
    "$ROOT_DIR/scripts/verify-system-rootfs.sh"
grep -q 'lib/firmware/rtlwifi/rtl8188eufw.bin' \
    "$ROOT_DIR/scripts/verify-system-rootfs.sh"
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
