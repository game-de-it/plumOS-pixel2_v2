#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
EXPERIMENT_DIR="$ROOT_DIR/experiments/linux-6.12"
bash -n "$EXPERIMENT_DIR/build-kernel.sh"
sh -n "$EXPERIMENT_DIR/initramfs/init"
grep -q 'PLUMOS_ENABLE_EXPERIMENTAL_LINUX_6_12' "$EXPERIMENT_DIR/build-kernel.sh"
grep -q 'output/experimental/linux-6.12/pixel2' "$EXPERIMENT_DIR/build-kernel.sh"
grep -q 'CONFIG_INITRAMFS_SOURCE' "$EXPERIMENT_DIR/build-kernel.sh"
grep -q 'compatible = "gpio-keys"' "$EXPERIMENT_DIR/build-kernel.sh"
grep -q 'plumos,generic-dsi' "$EXPERIMENT_DIR/build-kernel.sh"
grep -q "set_button_label('button-a', 'RK_PD1', 'BTN_SOUTH', 'A')" \
    "$EXPERIMENT_DIR/build-kernel.sh"
grep -q "set_button_label('button-b', 'RK_PD2', 'BTN_EAST', 'B')" \
    "$EXPERIMENT_DIR/build-kernel.sh"
grep -q 'INITRAMFS/dev/console' "$EXPERIMENT_DIR/build-kernel.sh"
grep -q 'reason=mountpoint-missing' "$EXPERIMENT_DIR/initramfs/init"
if grep -Eiq '(rocknix|emuelec|batocera|knulli)' \
    "$EXPERIMENT_DIR/initramfs/init"; then
    exit 1
fi
if rg -n 'experiments/linux-6\.12|build-kernel\.sh' \
    "$ROOT_DIR/scripts/docker-build.sh" "$ROOT_DIR/scripts/build-sd-image.sh" \
    "$ROOT_DIR/scripts/build-system-rootfs.sh" >/dev/null; then
    printf 'error: production build references experimental Linux 6.12\n' >&2
    exit 1
fi
printf 'experimental_linux_6_12=result-ok release_dependency=none\n'
