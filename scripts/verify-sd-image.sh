#!/usr/bin/env bash
set -euo pipefail

IMAGE="${1:-}"
PREFIX="${2:-}"
[ -f "$IMAGE" ] && [ -f "$PREFIX" ] || {
    printf 'usage: %s IMAGE BOOT_PREFIX\n' "$0" >&2
    exit 2
}
ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

if ! command -v parted >/dev/null 2>&1; then
    IMAGE="$(CDPATH= cd -- "$(dirname -- "$IMAGE")" && pwd)/$(basename -- "$IMAGE")"
    PREFIX="$(CDPATH= cd -- "$(dirname -- "$PREFIX")" && pwd)/$(basename -- "$PREFIX")"
    case "$IMAGE:$PREFIX" in
        "$ROOT_DIR"/*:"$ROOT_DIR"/*) ;;
        *) printf 'error: inputs must be under repository for Docker verification\n' >&2; exit 2 ;;
    esac
    TOOLS_IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"
    docker image inspect "$TOOLS_IMAGE" >/dev/null 2>&1 || \
        "$ROOT_DIR/scripts/build-tools-image.sh"
    container="$(
        docker create --platform linux/arm64 \
            -v "$ROOT_DIR:/work" -w /work "$TOOLS_IMAGE" \
            ./scripts/verify-sd-image.sh "/work/${IMAGE#"$ROOT_DIR"/}" \
            "/work/${PREFIX#"$ROOT_DIR"/}"
    )"
    cleanup_container() {
        [ -n "${container:-}" ] || return 0
        docker rm -f "$container" >/dev/null 2>&1 || true
    }
    trap cleanup_container INT TERM HUP
    docker start "$container" >/dev/null
    docker logs -f "$container"
    rc="$(docker wait "$container")"
    docker rm "$container" >/dev/null 2>&1 || true
    trap - INT TERM HUP
    exit "$rc"
fi

[ "$(stat -c '%s' "$IMAGE")" -eq 2701131776 ]
layout=$(parted -m -s "$IMAGE" unit s print)
printf '%s\n' "$layout" | grep -q '^1:32768s:1081343s:1048576s:fat32::boot, lba;$'
printf '%s\n' "$layout" | grep -q '^2:1081344s:5275647s:4194304s:ext4::;$'
[ "$(printf '%s\n' "$layout" | grep -c '^[0-9][0-9]*:')" -eq 2 ]

WORK="$(mktemp -d /tmp/plumos-pixel2-verify.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
dd if="$IMAGE" of="$WORK/image-prefix.bin" bs=512 skip=1 count=32767 status=none
dd if="$PREFIX" of="$WORK/source-prefix.bin" bs=512 skip=1 count=32767 status=none
cmp "$WORK/image-prefix.bin" "$WORK/source-prefix.bin"
dd if="$IMAGE" of="$WORK/boot.fat" bs=512 skip=32768 count=1048576 status=none
dd if="$IMAGE" of="$WORK/plumos-sys.ext4" bs=512 skip=1081344 count=4194304 status=none
fsck.vfat -n "$WORK/boot.fat" >/dev/null
e2fsck -fn "$WORK/plumos-sys.ext4" >/dev/null
[ "$(blkid -s LABEL -o value "$WORK/plumos-sys.ext4")" = PLUMOS_SYS ]
for file in Image SYSTEM rk3326s-gkd-pixel2.dtb oemsplash-1080.png plumos-image.manifest \
    post-flash.sh post-sysroot.sh; do
    MTOOLS_SKIP_CHECK=1 mcopy -i "$WORK/boot.fat" "::/$file" "$WORK/$file"
done
for slot in a b; do
    for suffix in squashfs sha256 manifest; do
        MTOOLS_SKIP_CHECK=1 mcopy -i "$WORK/boot.fat" \
            "::/system-slots/system-$slot.$suffix" "$WORK/system-$slot.$suffix"
    done
done
cmp "$WORK/Image" "$ROOT_DIR/artifacts/vendor/pixel2-stock/boot/Image"
cmp "$WORK/rk3326s-gkd-pixel2.dtb" \
    "$ROOT_DIR/artifacts/vendor/pixel2-stock/boot/rk3326s-gkd-pixel2.dtb"
python3 "$ROOT_DIR/scripts/verify-pixel2-boot-splash.py" \
    "$WORK/oemsplash-1080.png"
cmp "$WORK/oemsplash-1080.png" \
    "$ROOT_DIR/package/boot-assets-pixel2/oemsplash-1080.png"
grep -q '^boot_substrate=stock-pixel2$' "$WORK/plumos-image.manifest"
grep -q '^boot_splash=oemsplash-1080.png$' "$WORK/plumos-image.manifest"
grep -q '^boot_splash_geometry=480x640$' "$WORK/plumos-image.manifest"
expected_splash_sha=$(awk -F= '$1 == "boot_splash_sha256" { print $2 }' \
    "$WORK/plumos-image.manifest")
actual_splash_sha=$(sha256sum "$WORK/oemsplash-1080.png" | awk '{print $1}')
[ "$actual_splash_sha" = "$expected_splash_sha" ]
grep -q '^system_layout=fixed-dispatcher,system-a,system-b$' \
    "$WORK/plumos-image.manifest"
grep -q '^stock_initramfs_hooks=post-flash.sh,post-sysroot.sh$' \
    "$WORK/plumos-image.manifest"
grep -q '^layout=compact-seed-v1,.*plumos-user-absent$' \
    "$WORK/plumos-image.manifest"
grep -q '^first_boot_p2_target_mib=8192$' "$WORK/plumos-image.manifest"
grep -q '^first_boot_p3_label=PLUMOS_USER$' "$WORK/plumos-image.manifest"
grep -q '/usr/bin/ply-image /flash/oemsplash-1080.png' "$WORK/post-flash.sh"
grep -q 'boot-splash result=plumos' "$WORK/post-flash.sh"
grep -q 'post-sysroot' "$WORK/post-sysroot.sh"
cmp "$WORK/SYSTEM" "$ROOT_DIR/output/system-rootfs/pixel2/payload/SYSTEM"
"$ROOT_DIR/scripts/verify-system-dispatcher.sh" "$WORK/SYSTEM"
for slot in a b; do
    cmp "$WORK/system-$slot.squashfs" \
        "$ROOT_DIR/output/system-rootfs/pixel2/payload/system-slots/system-$slot.squashfs"
    expected=$(awk '{print $1}' "$WORK/system-$slot.sha256")
    actual=$(sha256sum "$WORK/system-$slot.squashfs" | awk '{print $1}')
    [ "$actual" = "$expected" ]
    "$ROOT_DIR/scripts/verify-system-rootfs.sh" "$WORK/system-$slot.squashfs"
done
mkdir -p "$WORK/app-layer"
debugfs -R "rdump / $WORK/app-layer" "$WORK/plumos-sys.ext4" >/dev/null 2>&1
"$ROOT_DIR/scripts/verify-app-layer.sh" "$WORK/app-layer"
"$ROOT_DIR/tests/test-pixel2-first-boot-provision.sh" "$IMAGE"
if strings "$WORK/Image" "$WORK/rk3326s-gkd-pixel2.dtb" \
    "$WORK/plumos-image.manifest" | grep -Eiq '(rock[n]ix|emuel[e]c|batocer[a]|knull[i])'; then
    printf 'error: foreign distribution identity in boot payload\n' >&2
    exit 1
fi
printf 'sd_image=result-ok image=%s\n' "$IMAGE"
