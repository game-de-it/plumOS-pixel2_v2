#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

for script in \
    scripts/build-tools-image.sh \
    scripts/capture-stock-boot-artifacts.sh \
    scripts/capture-stock-prefix-macos.sh \
    scripts/verify-stock-boot-artifacts.sh; do
    bash -n "$ROOT_DIR/$script"
done

grep -q 'analysis-only-not-copied' "$ROOT_DIR/scripts/capture-stock-boot-artifacts.sh"
! grep -q 'cp .*SYSTEM' "$ROOT_DIR/scripts/capture-stock-boot-artifacts.sh"
grep -q 'Removable Media:' "$ROOT_DIR/scripts/capture-stock-prefix-macos.sh"
grep -q '16777216 Bytes' "$ROOT_DIR/scripts/capture-stock-prefix-macos.sh"
grep -Fq 'RAW_DISK="/dev/r${DISK#/dev/}"' \
    "$ROOT_DIR/scripts/capture-stock-prefix-macos.sh"
test_disk=/dev/disk4
test "/dev/r${test_disk#/dev/}" = /dev/rdisk4

printf 'stock_capture_scripts=result-ok\n'
