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
    exec docker run --rm --platform linux/arm64 \
        -v "$ROOT_DIR:/work" -w /work "$TOOLS_IMAGE" \
        ./scripts/verify-sd-image.sh "/work/${IMAGE#"$ROOT_DIR"/}" \
        "/work/${PREFIX#"$ROOT_DIR"/}"
fi

[ "$(stat -c '%s' "$IMAGE")" -eq 2147483648 ]
layout=$(parted -m -s "$IMAGE" unit s print)
printf '%s\n' "$layout" | grep -q '^1:32768s:557055s:524288s:fat32::boot, lba;$'
printf '%s\n' "$layout" | grep -q '^2:557056s:1605631s:1048576s:ext4::;$'
printf '%s\n' "$layout" | grep -q '^3:1605632s:4194303s:2588672s:fat32::lba;$'

WORK="$(mktemp -d /tmp/plumos-pixel2-verify.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
dd if="$IMAGE" of="$WORK/image-prefix.bin" bs=512 skip=1 count=32767 status=none
dd if="$PREFIX" of="$WORK/source-prefix.bin" bs=512 skip=1 count=32767 status=none
cmp "$WORK/image-prefix.bin" "$WORK/source-prefix.bin"
dd if="$IMAGE" of="$WORK/boot.fat" bs=512 skip=32768 count=524288 status=none
dd if="$IMAGE" of="$WORK/state.ext4" bs=512 skip=557056 count=1048576 status=none
dd if="$IMAGE" of="$WORK/roms.fat" bs=512 skip=1605632 count=2588672 status=none
fsck.vfat -n "$WORK/boot.fat" >/dev/null
fsck.vfat -n "$WORK/roms.fat" >/dev/null
e2fsck -fn "$WORK/state.ext4" >/dev/null
for file in Image SYSTEM rk3326s-gkd-pixel2.dtb plumos-image.manifest; do
    MTOOLS_SKIP_CHECK=1 mcopy -i "$WORK/boot.fat" "::/$file" "$WORK/$file"
done
cmp "$WORK/Image" "$ROOT_DIR/output/kernel/pixel2/Image"
cmp "$WORK/rk3326s-gkd-pixel2.dtb" \
    "$ROOT_DIR/output/kernel/pixel2/rk3326s-gkd-pixel2.dtb"
cmp "$WORK/SYSTEM" "$ROOT_DIR/output/system-rootfs/pixel2/payload/SYSTEM"
"$ROOT_DIR/scripts/verify-system-rootfs.sh" "$WORK/SYSTEM"
if strings "$WORK/Image" "$WORK/rk3326s-gkd-pixel2.dtb" \
    "$WORK/plumos-image.manifest" | grep -Eiq '(rocknix|emuelec|batocera|knulli)'; then
    printf 'error: foreign distribution identity in boot payload\n' >&2
    exit 1
fi
printf 'sd_image=result-ok image=%s\n' "$IMAGE"
