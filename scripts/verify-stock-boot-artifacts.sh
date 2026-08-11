#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT_DIR="${1:-$ROOT_DIR/artifacts/vendor/pixel2-stock}"
MANIFEST="$OUT_DIR/manifest.tsv"

for path in \
    "$OUT_DIR/boot/Image" \
    "$OUT_DIR/boot/rk3326s-gkd-pixel2.dtb" \
    "$OUT_DIR/boot/rk3326s-gkd-pixel2-uboot.dtb" \
    "$OUT_DIR/kernel/extracted/usr/lib/kernel-overlays/base/lib/modules" \
    "$OUT_DIR/kernel/extracted/usr/lib/kernel-overlays/base/lib/firmware" \
    "$MANIFEST"; do
    [ -e "$path" ] || {
        printf 'error: missing captured artifact: %s\n' "$path" >&2
        exit 1
    }
done

[ ! -e "$OUT_DIR/SYSTEM" ]
[ ! -e "$OUT_DIR/kernel/extracted/usr/lib/systemd" ]
[ ! -e "$OUT_DIR/kernel/extracted/usr/bin" ]
[ -L "$OUT_DIR/kernel/extracted/usr/lib/firmware" ]
[ "$(readlink "$OUT_DIR/kernel/extracted/usr/lib/firmware")" = /var/lib/firmware ]
grep -q $'^stock_system_policy\tanalysis-only-not-copied$' "$MANIFEST"

files="$(awk -F '\t' '$1 == "file" { count++ } END { print count + 0 }' "$MANIFEST")"
[ "$files" -gt 3 ] || {
    printf 'error: manifest contains too few files: %s\n' "$files" >&2
    exit 1
}

if [ -e "$OUT_DIR/boot/rockchip-boot-prefix.bin" ]; then
    size="$(stat -f '%z' "$OUT_DIR/boot/rockchip-boot-prefix.bin" 2>/dev/null || stat -c '%s' "$OUT_DIR/boot/rockchip-boot-prefix.bin")"
    [ "$size" -eq 16777216 ]
fi

if [ -e "$OUT_DIR/boot/rockchip-boot-prefix.bin" ]; then
    prefix=present
else
    prefix=missing
fi
printf 'vendor_boot=result-ok files=%s prefix=%s\n' "$files" "$prefix"
