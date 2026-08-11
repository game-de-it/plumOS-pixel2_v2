#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SOURCE_ROOT="${PLUMOS_PIXEL2_STOCK_BOOT_ROOT:-/Volumes/EMUELEC}"
OUT_DIR="${PLUMOS_PIXEL2_VENDOR_OUT:-$ROOT_DIR/artifacts/vendor/pixel2-stock}"
EXPECTED="$ROOT_DIR/configs/stock-boot.expected.sha256"
TOOLS_IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"
TOOLS_PLATFORM="${PLUMOS_PIXEL2_TOOLS_PLATFORM:-linux/arm64}"
PREFIX_SOURCE="${PLUMOS_PIXEL2_BOOT_PREFIX:-}"

sha256_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    else
        shasum -a 256 "$1" | awk '{print $1}'
    fi
}

expected_hash() {
    awk -v name="$1" '$2 == name { print $1 }' "$EXPECTED"
}

verify_source() {
    local path="$1"
    local name="$2"
    local expected actual
    expected="$(expected_hash "$name")"
    [ -n "$expected" ] || {
        printf 'error: no expected hash for %s\n' "$name" >&2
        exit 1
    }
    actual="$(sha256_file "$path")"
    [ "$actual" = "$expected" ] || {
        printf 'error: source hash mismatch for %s\nexpected=%s\nactual=%s\n' \
            "$name" "$expected" "$actual" >&2
        exit 1
    }
}

for name in Image SYSTEM rk3326s-gkd-pixel2.dtb; do
    [ -f "$SOURCE_ROOT/$name" ] || {
        printf 'error: missing stock artifact: %s/%s\n' "$SOURCE_ROOT" "$name" >&2
        exit 1
    }
done

UBOOT_DTB="$SOURCE_ROOT/.backup/rk3326s-gkd-pixel2-uboot.dtb"
[ -f "$UBOOT_DTB" ] || {
    printf 'error: missing U-Boot DTB backup: %s\n' "$UBOOT_DTB" >&2
    exit 1
}

verify_source "$SOURCE_ROOT/Image" Image
verify_source "$SOURCE_ROOT/SYSTEM" SYSTEM
verify_source "$SOURCE_ROOT/rk3326s-gkd-pixel2.dtb" rk3326s-gkd-pixel2.dtb
verify_source "$UBOOT_DTB" rk3326s-gkd-pixel2-uboot.dtb

mkdir -p "$OUT_DIR/boot" "$OUT_DIR/kernel"
cp -p "$SOURCE_ROOT/Image" "$OUT_DIR/boot/Image"
cp -p "$SOURCE_ROOT/rk3326s-gkd-pixel2.dtb" "$OUT_DIR/boot/"
cp -p "$UBOOT_DTB" "$OUT_DIR/boot/rk3326s-gkd-pixel2-uboot.dtb"

if [ -n "$PREFIX_SOURCE" ]; then
    [ -f "$PREFIX_SOURCE" ] || {
        printf 'error: boot prefix does not exist: %s\n' "$PREFIX_SOURCE" >&2
        exit 1
    }
    prefix_size="$(stat -f '%z' "$PREFIX_SOURCE" 2>/dev/null || stat -c '%s' "$PREFIX_SOURCE")"
    [ "$prefix_size" -eq 16777216 ] || {
        printf 'error: boot prefix must be exactly 16777216 bytes, got %s\n' "$prefix_size" >&2
        exit 1
    }
    cp -p "$PREFIX_SOURCE" "$OUT_DIR/boot/rockchip-boot-prefix.bin"
fi

docker image inspect "$TOOLS_IMAGE" >/dev/null 2>&1 || "$ROOT_DIR/scripts/build-tools-image.sh"
docker run --rm \
    --platform "$TOOLS_PLATFORM" \
    --user "$(id -u):$(id -g)" \
    -v "$SOURCE_ROOT:/source:ro" \
    -v "$OUT_DIR/kernel:/out" \
    "$TOOLS_IMAGE" \
    sh -eu -c '
        unsquashfs -f -no-progress -d /out/extracted /source/SYSTEM \
            usr/lib/kernel-overlays/base/lib/modules \
            usr/lib/kernel-overlays/base/lib/firmware \
            usr/lib/firmware >/dev/null
        test -d /out/extracted/usr/lib/kernel-overlays/base/lib/modules
        test -d /out/extracted/usr/lib/kernel-overlays/base/lib/firmware
    '

[ ! -e "$OUT_DIR/SYSTEM" ] || {
    printf 'error: stock SYSTEM must never be copied into vendor output\n' >&2
    exit 1
}

SOURCE_REF="$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || printf unknown)"
MANIFEST="$OUT_DIR/manifest.tsv"
{
    printf 'format\tplumos-pixel2-vendor-boot-v1\n'
    printf 'device\tpixel2\n'
    printf 'architecture\taarch64\n'
    printf 'kernel_version\t5.10.198\n'
    printf 'source_ref\t%s\n' "$SOURCE_REF"
    printf 'stock_system_policy\tanalysis-only-not-copied\n'
    find "$OUT_DIR/boot" "$OUT_DIR/kernel/extracted" -type f -print | \
        LC_ALL=C sort | while IFS= read -r path; do
            rel="${path#"$OUT_DIR/"}"
            size="$(stat -f '%z' "$path" 2>/dev/null || stat -c '%s' "$path")"
            printf 'file\t%s\t%s\t%s\n' "$rel" "$size" "$(sha256_file "$path")"
        done
    printf 'analysis_input\tSYSTEM\t%s\n' "$(expected_hash SYSTEM)"
} >"$MANIFEST"

"$ROOT_DIR/scripts/verify-stock-boot-artifacts.sh" "$OUT_DIR"
printf 'created: %s\n' "$OUT_DIR"
