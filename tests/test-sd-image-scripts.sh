#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
bash -n "$ROOT_DIR/scripts/build-sd-image.sh"
bash -n "$ROOT_DIR/scripts/verify-sd-image.sh"
grep -q 'BOOT_START=32768' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'refusing\|boot prefix missing' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'cmp.*image-prefix' "$ROOT_DIR/scripts/verify-sd-image.sh"
grep -q 'capture_sha256' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'PLUMOS_SYS' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'normalize_ext4_timestamps' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'set_inode_field' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'faketime.*debugfs' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'verify-app-layer.sh' "$ROOT_DIR/scripts/verify-sd-image.sh"
grep -q 'artifacts/vendor/pixel2-stock/boot/Image' \
    "$ROOT_DIR/scripts/verify-sd-image.sh"
grep -q 'STOCK_BOOT_DIR' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'boot_substrate=stock-pixel2' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'post-sysroot.sh' "$ROOT_DIR/scripts/build-sd-image.sh"
grep -q 'stock_initramfs_hooks' "$ROOT_DIR/scripts/verify-sd-image.sh"
printf 'sd_image_scripts=result-ok\n'
