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
    docker image inspect "$TOOLS_IMAGE" >/dev/null 2>&1 || \
        "$ROOT_DIR/scripts/build-tools-image.sh"
    exec docker run --rm --platform linux/arm64 \
        -e SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-}" \
        -e PLUMOS_PIXEL2_VERSION="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}" \
        -e PLUMOS_PIXEL2_BOOT_PREFIX="/work/${PREFIX#"$ROOT_DIR"/}" \
        -v "$ROOT_DIR:/work" -w /work "$TOOLS_IMAGE" \
        ./scripts/build-sd-image.sh --inside
fi

ROOT_DIR=/work
PREFIX="${PLUMOS_PIXEL2_BOOT_PREFIX:-/work/artifacts/vendor/pixel2-stock-source/rockchip-boot-prefix.bin}"
PREFIX_MANIFEST="$ROOT_DIR/artifacts/manifests/pixel2-stock-prefix.manifest"
KERNEL_DIR="$ROOT_DIR/output/kernel/pixel2"
SYSTEM_DIR="$ROOT_DIR/output/system-rootfs/pixel2/payload"
APP_DIR="$ROOT_DIR/output/app-layer/pixel2/plumos"
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
    [ -f "$KERNEL_DIR/$file" ] || {
        printf 'error: kernel output missing; run ./scripts/build-kernel.sh first\n' >&2
        exit 2
    }
done
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
    local source_root=$1 image=$2 epoch=$3 commands path relative field
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
    debugfs -w -f "$commands" "$image" >/dev/null 2>&1
}

# Fixed 2 GiB layout, in 512-byte sectors.
TOTAL_SECTORS=4194304
BOOT_START=32768
BOOT_SECTORS=524288
SYS_START=557056
SYS_SECTORS=1048576
USER_START=1605632
USER_SECTORS=2588672
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
    mkpart primary fat32 "${USER_START}s" "$((USER_START + USER_SECTORS - 1))s" \
    set 1 boot on 2> >(grep -v 'udevadm: not found' >&2)
# Keep the partition table deterministic while leaving the Rockchip payload at
# sectors 1..32767 untouched.
printf '\x50\x4c\x55\x4d' | dd of="$IMAGE" bs=1 seek=440 conv=notrunc status=none

truncate -s "$((BOOT_SECTORS * 512))" "$WORK/boot.fat"
truncate -s "$((SYS_SECTORS * 512))" "$WORK/plumos-sys.ext4"
truncate -s "$((USER_SECTORS * 512))" "$WORK/plumos-user.fat"
SOURCE_DATE_EPOCH="$SOURCE_EPOCH" mkfs.vfat --invariant -F 32 -n PLUMOS_BOOT \
    -i 504C554D "$WORK/boot.fat" >/dev/null
E2FSPROGS_FAKE_TIME="$SOURCE_EPOCH" mkfs.ext4 -q -F -L PLUMOS_SYS \
    -U 504c554d-5354-4154-4500-000000000002 \
    -E lazy_itable_init=0,lazy_journal_init=0,hash_seed=504c554d-5354-4154-4500-000000000002 \
    -d "$WORK/plumos-sys" "$WORK/plumos-sys.ext4"
normalize_ext4_timestamps "$WORK/plumos-sys" "$WORK/plumos-sys.ext4" "$SOURCE_EPOCH"
SOURCE_DATE_EPOCH="$SOURCE_EPOCH" mkfs.vfat --invariant -F 32 -n PLUMOS_USER \
    -i 504C0003 "$WORK/plumos-user.fat" >/dev/null
for directory in roms bios Images Themes Screenshots Music updates imports exports plumos-logs; do
    MTOOLS_SKIP_CHECK=1 mmd -i "$WORK/plumos-user.fat" "::/$directory"
done

install -m 0644 "$KERNEL_DIR/Image" "$WORK/boot/Image"
install -m 0644 "$KERNEL_DIR/rk3326s-gkd-pixel2.dtb" \
    "$WORK/boot/rk3326s-gkd-pixel2.dtb"
install -m 0644 "$SYSTEM_DIR/SYSTEM" "$WORK/boot/SYSTEM"
prefix_sha=$actual_prefix_sha
cat >"$WORK/boot/plumos-image.manifest" <<EOF
format=plumos-pixel2-image-v1
device=pixel2
architecture=aarch64
version=$VERSION
source_ref=$SOURCE_REF
source_date_epoch=$SOURCE_EPOCH
boot_prefix_sha256=$prefix_sha
layout=boot-prefix-16MiB,boot-fat-256MiB,plumos-sys-ext4-512MiB,plumos-user-fat32-remainder
runtime_abi=plumos-pixel2-app-layer-v1
EOF
find "$WORK/boot" -exec touch -h -d "@$SOURCE_EPOCH" {} +
MTOOLS_SKIP_CHECK=1 mcopy -m -o -i "$WORK/boot.fat" "$WORK/boot/"* ::/

dd if="$WORK/boot.fat" of="$IMAGE" bs=512 seek="$BOOT_START" conv=notrunc status=none
dd if="$WORK/plumos-sys.ext4" of="$IMAGE" bs=512 seek="$SYS_START" conv=notrunc status=none
dd if="$WORK/plumos-user.fat" of="$IMAGE" bs=512 seek="$USER_START" conv=notrunc status=none

boot_fs_sha=$(sha256sum "$WORK/boot.fat" | awk '{print $1}')
sys_fs_sha=$(sha256sum "$WORK/plumos-sys.ext4" | awk '{print $1}')
user_fs_sha=$(sha256sum "$WORK/plumos-user.fat" | awk '{print $1}')
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
user_filesystem_sha256=$user_fs_sha
source_ref=$SOURCE_REF
source_date_epoch=$SOURCE_EPOCH
EOF
(cd "$OUT_DIR" && sha256sum "$(basename "$IMAGE")" image.manifest >checksums.sha256)
"$ROOT_DIR/scripts/verify-sd-image.sh" "$IMAGE" "$PREFIX"
printf 'created: %s\n' "$IMAGE"
