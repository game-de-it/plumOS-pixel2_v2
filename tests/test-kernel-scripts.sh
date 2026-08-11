#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
bash -n "$ROOT_DIR/scripts/build-kernel.sh"
sh -n "$ROOT_DIR/initramfs/pixel2/init"
grep -q 'CONFIG_INITRAMFS_SOURCE' "$ROOT_DIR/scripts/build-kernel.sh"
grep -q 'compatible = "gpio-keys"' "$ROOT_DIR/scripts/build-kernel.sh"
grep -q 'plumos,generic-dsi' "$ROOT_DIR/scripts/build-kernel.sh"
grep -q "set_button_label('button-a', 'RK_PD1', 'BTN_SOUTH', 'A')" \
    "$ROOT_DIR/scripts/build-kernel.sh"
grep -q "set_button_label('button-b', 'RK_PD2', 'BTN_EAST', 'B')" \
    "$ROOT_DIR/scripts/build-kernel.sh"
grep -q 'INITRAMFS/dev/console' "$ROOT_DIR/scripts/build-kernel.sh"
grep -q 'reason=mountpoint-missing' "$ROOT_DIR/initramfs/pixel2/init"
if grep -Eiq '(rocknix|emuelec|batocera|knulli)' \
    "$ROOT_DIR/initramfs/pixel2/init"; then
    exit 1
fi
printf 'kernel_scripts=result-ok\n'
