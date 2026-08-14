#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TOOLS_IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"
PREFIX="${PLUMOS_PIXEL2_BOOT_PREFIX:-$ROOT_DIR/artifacts/vendor/pixel2-stock-source/rockchip-boot-prefix.bin}"

if [ "${1:-}" != --inside ]; then
    [ -f "$PREFIX" ] || {
        printf 'error: boot prefix missing: %s\n' "$PREFIX" >&2
        printf 'capture it read-only with: ./scripts/capture-stock-prefix-macos.sh /dev/disk4\n' >&2
        exit 2
    }
    case "$PREFIX" in
        "$ROOT_DIR"/*) ;;
        *) printf 'error: boot prefix must be under the repository\n' >&2; exit 2 ;;
    esac
    export PLUMOS_PIXEL2_BOOT_PREFIX="$PREFIX"
    exec "$ROOT_DIR/scripts/docker-build.sh" sd-image
fi

ROOT_DIR=/work
PREFIX="${PLUMOS_PIXEL2_BOOT_PREFIX:-/work/artifacts/vendor/pixel2-stock-source/rockchip-boot-prefix.bin}"
PREFIX_MANIFEST="$ROOT_DIR/artifacts/manifests/pixel2-stock-prefix.manifest"
STOCK_BOOT_DIR="$ROOT_DIR/artifacts/vendor/pixel2-stock/boot"
STOCK_BOOT_MANIFEST="$ROOT_DIR/artifacts/vendor/pixel2-stock/manifest.tsv"
BOOT_SPLASH="$ROOT_DIR/package/boot-assets-pixel2/oemsplash-1080.png"
SYSTEM_DIR="$ROOT_DIR/output/system-rootfs/pixel2/payload"
APP_DIR="$ROOT_DIR/output/app-layer/pixel2/plumos"
BOOT_HOOK_DIR="$ROOT_DIR/boot-hooks/pixel2"
OUT_DIR="$ROOT_DIR/output/image/pixel2"
VERSION="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}"
SOURCE_REF="$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || printf unknown)"
SOURCE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[ -n "$SOURCE_EPOCH" ] || SOURCE_EPOCH="$(git -C "$ROOT_DIR" show -s --format=%ct HEAD)"

case "$SOURCE_EPOCH" in
    ''|*[!0-9]*) printf 'error: invalid SOURCE_DATE_EPOCH\n' >&2; exit 2 ;;
esac
export SOURCE_DATE_EPOCH="$SOURCE_EPOCH"
[ "$(stat -c '%s' "$PREFIX")" -eq 16777216 ] || {
    printf 'error: Rockchip boot prefix must be exactly 16 MiB\n' >&2
    exit 2
}
[ -f "$PREFIX_MANIFEST" ] || {
    printf 'error: registered boot prefix manifest is missing\n' >&2
    exit 2
}
expected_prefix_sha=$(awk -F= '$1 == "capture_sha256" { print $2 }' \
    "$PREFIX_MANIFEST")
actual_prefix_sha=$(sha256sum "$PREFIX" | awk '{print $1}')
[ -n "$expected_prefix_sha" ] && [ "$actual_prefix_sha" = "$expected_prefix_sha" ] || {
    printf 'error: boot prefix does not match registered Pixel2 stock capture\n' >&2
    exit 2
}
for file in Image rk3326s-gkd-pixel2.dtb; do
    [ -f "$STOCK_BOOT_DIR/$file" ] || {
        printf 'error: stock boot artifact missing: %s\n' "$STOCK_BOOT_DIR/$file" >&2
        exit 2
    }
done
[ -f "$STOCK_BOOT_MANIFEST" ] || {
    printf 'error: stock boot artifact manifest missing\n' >&2
    exit 2
}
[ -f "$BOOT_SPLASH" ] || {
    printf 'error: Pixel2 boot splash missing: %s\n' "$BOOT_SPLASH" >&2
    exit 2
}
python3 "$ROOT_DIR/scripts/verify-pixel2-boot-splash.py" "$BOOT_SPLASH"
[ -f "$SYSTEM_DIR/SYSTEM" ] || {
    printf 'error: SYSTEM missing; run ./scripts/build-system-rootfs.sh first\n' >&2
    exit 2
}
[ -f "$APP_DIR/manifest.json" ] && [ -f "$APP_DIR/checksums.sha256" ] || {
    printf 'error: app layer missing; run ./scripts/docker-build.sh app-layer --strict\n' >&2
    exit 2
}
"$ROOT_DIR/scripts/verify-app-layer.sh" "$APP_DIR"

normalize_ext4_timestamps() {
    local source_root=$1 image=$2 epoch=$3 commands path relative field fake_time
    commands="$WORK/normalize-ext4.debugfs"
    : >"$commands"
    for path in '<2>' '<11>'; do
        for field in atime ctime mtime crtime; do
            printf 'set_inode_field %s %s @%s\n' "$path" "$field" "$epoch" >>"$commands"
        done
    done
    while IFS= read -r -d '' path; do
        relative=${path#"$source_root"/}
        case "$relative" in
            *'"'*) printf 'error: unsupported quote in app-layer path: %s\n' "$relative" >&2; exit 1 ;;
        esac
        for field in atime ctime mtime crtime; do
            printf 'set_inode_field "/%s" %s @%s\n' \
                "$relative" "$field" "$epoch" >>"$commands"
        done
    done < <(find "$source_root" -mindepth 1 -print0 | sort -z)
    fake_time=$(date -u -d "@$epoch" '+%Y-%m-%d %H:%M:%S')
    faketime "$fake_time" debugfs -w -f "$commands" "$image" >/dev/null 2>&1
}

# Compact first-boot seed layout, in 512-byte sectors.  The image deliberately
# ends with the 2 GiB p2 filesystem.  First boot grows p2 to exactly 8 GiB and
# creates p3 PLUMOS_USER through the physical card's final sector.
TOTAL_SECTORS=5275648
BOOT_START=32768
BOOT_SECTORS=1048576
SYS_START=1081344
SYS_SECTORS=4194304
IMAGE="$OUT_DIR/plumOS-Pixel2-$VERSION.img"
WORK="$(mktemp -d /tmp/plumos-pixel2-image.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR" "$WORK/boot" "$WORK/plumos-sys"
cp -a "$APP_DIR/." "$WORK/plumos-sys/"

truncate -s "$((TOTAL_SECTORS * 512))" "$IMAGE"
dd if="$PREFIX" of="$IMAGE" bs=1M count=16 conv=notrunc status=none
parted -s "$IMAGE" unit s mklabel msdos \
    mkpart primary fat32 "${BOOT_START}s" "$((BOOT_START + BOOT_SECTORS - 1))s" \
    mkpart primary ext4 "${SYS_START}s" "$((SYS_START + SYS_SECTORS - 1))s" \
    set 1 boot on 2> >(grep -v 'udevadm: not found' >&2)
# Keep the partition table deterministic while leaving the Rockchip payload at
# sectors 1..32767 untouched.
printf '\x50\x4c\x55\x4d' | dd of="$IMAGE" bs=1 seek=440 conv=notrunc status=none

truncate -s "$((BOOT_SECTORS * 512))" "$WORK/boot.fat"
truncate -s "$((SYS_SECTORS * 512))" "$WORK/plumos-sys.ext4"
SOURCE_DATE_EPOCH="$SOURCE_EPOCH" mkfs.vfat --invariant -F 32 -n PLUMOS_BOOT \
    -i 504C554D "$WORK/boot.fat" >/dev/null
E2FSPROGS_FAKE_TIME="$SOURCE_EPOCH" mkfs.ext4 -q -F -L PLUMOS_SYS \
    -U 504c554d-5354-4154-4500-000000000002 \
    -E lazy_itable_init=0,lazy_journal_init=0,hash_seed=504c554d-5354-4154-4500-000000000002 \
    -d "$WORK/plumos-sys" "$WORK/plumos-sys.ext4"
normalize_ext4_timestamps "$WORK/plumos-sys" "$WORK/plumos-sys.ext4" "$SOURCE_EPOCH"

install -m 0644 "$STOCK_BOOT_DIR/Image" "$WORK/boot/Image"
install -m 0644 "$STOCK_BOOT_DIR/rk3326s-gkd-pixel2.dtb" \
    "$WORK/boot/rk3326s-gkd-pixel2.dtb"
install -m 0644 "$BOOT_SPLASH" "$WORK/boot/oemsplash-1080.png"
cp -a "$SYSTEM_DIR/." "$WORK/boot/"
for hook in post-flash.sh post-sysroot.sh; do
    [ -f "$BOOT_HOOK_DIR/$hook" ] || {
        printf 'error: boot hook missing: %s/%s\n' "$BOOT_HOOK_DIR" "$hook" >&2
        exit 2
    }
    install -m 0755 "$BOOT_HOOK_DIR/$hook" "$WORK/boot/$hook"
done
prefix_sha=$actual_prefix_sha
stock_image_sha=$(sha256sum "$STOCK_BOOT_DIR/Image" | awk '{print $1}')
stock_dtb_sha=$(sha256sum "$STOCK_BOOT_DIR/rk3326s-gkd-pixel2.dtb" | awk '{print $1}')
boot_splash_sha=$(sha256sum "$BOOT_SPLASH" | awk '{print $1}')
cat >"$WORK/boot/plumos-image.manifest" <<EOF
format=plumos-pixel2-image-v1
device=pixel2
architecture=aarch64
version=$VERSION
source_ref=$SOURCE_REF
source_date_epoch=$SOURCE_EPOCH
boot_prefix_sha256=$prefix_sha
boot_substrate=stock-pixel2
stock_image_sha256=$stock_image_sha
stock_dtb_sha256=$stock_dtb_sha
boot_splash=oemsplash-1080.png
boot_splash_geometry=480x640
boot_splash_sha256=$boot_splash_sha
layout=compact-seed-v1,boot-prefix-16MiB,boot-fat-512MiB,plumos-sys-ext4-2048MiB,plumos-user-absent
first_boot_p2_target_mib=8192
first_boot_p3_label=PLUMOS_USER
first_boot_p3_extent=sector-17858560-to-card-end
minimum_card_bytes=15000000000
runtime_abi=plumos-pixel2-app-layer-v1
system_layout=fixed-dispatcher,system-a,system-b
stock_initramfs_hooks=post-flash.sh,post-sysroot.sh
EOF
find "$WORK/boot" -exec touch -h -d "@$SOURCE_EPOCH" {} +
MTOOLS_SKIP_CHECK=1 mcopy -m -o -i "$WORK/boot.fat" "$WORK/boot/"* ::/

dd if="$WORK/boot.fat" of="$IMAGE" bs=512 seek="$BOOT_START" conv=notrunc status=none
dd if="$WORK/plumos-sys.ext4" of="$IMAGE" bs=512 seek="$SYS_START" conv=notrunc status=none

boot_fs_sha=$(sha256sum "$WORK/boot.fat" | awk '{print $1}')
sys_fs_sha=$(sha256sum "$WORK/plumos-sys.ext4" | awk '{print $1}')
image_sha=$(sha256sum "$IMAGE" | awk '{print $1}')
image_size=$(stat -c '%s' "$IMAGE")
cat >"$OUT_DIR/image.manifest" <<EOF
format=plumos-pixel2-image-v1
file=$(basename "$IMAGE")
image_size=$image_size
image_sha256=$image_sha
boot_prefix_sha256=$prefix_sha
boot_filesystem_sha256=$boot_fs_sha
sys_filesystem_sha256=$sys_fs_sha
user_filesystem=created-on-first-boot
user_filesystem_label=PLUMOS_USER
source_ref=$SOURCE_REF
source_date_epoch=$SOURCE_EPOCH
EOF
(cd "$OUT_DIR" && sha256sum "$(basename "$IMAGE")" image.manifest >checksums.sha256)
"$ROOT_DIR/scripts/verify-sd-image.sh" "$IMAGE" "$PREFIX"
printf 'created: %s\n' "$IMAGE"
