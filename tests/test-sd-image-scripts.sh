#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
bash -n "$ROOT_DIR/scripts/build-sd-image.sh"
bash -n "$ROOT_DIR/scripts/verify-sd-image.sh"
bash -n "$ROOT_DIR/scripts/build-pixel2-boot-dtb.sh"
bash -n "$ROOT_DIR/rootfs/pixel2/usr/sbin/plumos-first-boot-provision"
bash -n "$ROOT_DIR/tests/test-pixel2-first-boot-provision.sh"
test -x "$ROOT_DIR/tests/test-pixel2-boot-dtb-diff.sh"
"$ROOT_DIR/tests/test-pixel2-boot-dtb-diff.sh"
python3 "$ROOT_DIR/scripts/verify-pixel2-boot-splash.py" \
    "$ROOT_DIR/package/boot-assets-pixel2/oemsplash-1080.png"
grep -q 'BOOT_START=32768' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'refusing\|boot prefix missing' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'cmp.*image-prefix' "$ROOT_DIR/scripts/verify-sd-image.sh"
grep -q 'capture_sha256' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'PLUMOS_SYS' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'normalize_ext4_timestamps' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'TOTAL_SECTORS=5275648' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'plumos-user-absent' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'test-pixel2-first-boot-provision.sh' "$ROOT_DIR/scripts/verify-sd-image.sh"
grep -q 'set_inode_field' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'faketime.*debugfs' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'verify-app-layer.sh' "$ROOT_DIR/scripts/verify-sd-image.sh"
grep -q 'artifacts/vendor/pixel2-stock/boot/Image' \
    "$ROOT_DIR/scripts/verify-sd-image.sh"
grep -q 'output/boot/pixel2/rk3326s-gkd-pixel2.dtb' \
    "$ROOT_DIR/scripts/verify-sd-image.sh"
grep -q 'runtime_dtb_policy=stock-plus-otg-vbus-supply' \
    "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'vbus-supply' "$ROOT_DIR/scripts/build-pixel2-boot-dtb.sh"
grep -q 'STOCK_BOOT_DIR' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'boot_substrate=stock-pixel2' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'post-sysroot.sh' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'stock_initramfs_hooks' "$ROOT_DIR/scripts/verify-sd-image.sh"
grep -q 'oemsplash-1080.png' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'verify-pixel2-boot-splash.py' "$ROOT_DIR/scripts/verify-sd-image.sh"
grep -q '/usr/bin/ply-image /flash/oemsplash-1080.png' \
    "$ROOT_DIR/boot-hooks/pixel2/post-flash.sh"
grep -q 'boot-splash result=plumos' \
    "$ROOT_DIR/boot-hooks/pixel2/post-flash.sh"
printf 'sd_image_scripts=result-ok\n'
