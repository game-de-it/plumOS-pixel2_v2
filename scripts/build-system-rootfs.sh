#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"
OUT_DIR="${PLUMOS_PIXEL2_SYSTEM_OUT:-$ROOT_DIR/output/system-rootfs/pixel2}"

if [ "${1:-}" != --inside ]; then
    docker image inspect "$IMAGE" >/dev/null 2>&1 || "$ROOT_DIR/scripts/build-tools-image.sh"
    exec docker run --rm --platform linux/arm64 \
        -e SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-}" \
        -e PLUMOS_PIXEL2_VERSION="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}" \
        -v "$ROOT_DIR:/work" -w /work "$IMAGE" \
        ./scripts/build-system-rootfs.sh --inside
fi

ROOT_DIR=/work
OUT_DIR=/work/output/system-rootfs/pixel2
ROOTFS_DIR="$OUT_DIR/rootfs"
PAYLOAD_DIR="$OUT_DIR/payload"
VERSION="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}"
SOURCE_REF="$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || printf unknown)"
SOURCE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[ -n "$SOURCE_EPOCH" ] || SOURCE_EPOCH="$(git -C "$ROOT_DIR" show -s --format=%ct HEAD)"

case "$SOURCE_EPOCH" in
    ''|*[!0-9]*) printf 'error: invalid SOURCE_DATE_EPOCH\n' >&2; exit 2 ;;
esac

rm -rf "$OUT_DIR"
mkdir -p "$ROOTFS_DIR" "$PAYLOAD_DIR"
cp -a "$ROOT_DIR/rootfs/pixel2/." "$ROOTFS_DIR/"
"$ROOT_DIR/scripts/build-adbd-overlay.sh" --inside "$ROOTFS_DIR"
"$ROOT_DIR/scripts/install-kernel-runtime.sh" "$ROOTFS_DIR"
chmod 0755 "$ROOTFS_DIR/sbin/init" "$ROOTFS_DIR/usr/lib/plumos/init.d/"*
chmod 0600 "$ROOTFS_DIR/etc/shadow"

copy_elf() {
    src=$(command -v "$1")
    dst="$ROOTFS_DIR$src"
    mkdir -p "${dst%/*}"
    cp -L "$src" "$dst"
    ldd "$src" 2>/dev/null | awk '
        /=> \// { print $3 }
        /^\// { print $1 }
    ' | while IFS= read -r lib; do
        [ -f "$lib" ] || continue
        mkdir -p "$ROOTFS_DIR${lib%/*}"
        cp -L "$lib" "$ROOTFS_DIR$lib"
    done
}

install -D -m 0755 /bin/busybox "$ROOTFS_DIR/bin/busybox"
ln -s busybox "$ROOTFS_DIR/bin/sh"
for applet in basename blkid cat chmod chown cttyhack cut date dirname grep \
    hostname ip kill ln logger ls mkdir mdev mount mountpoint mv rm sed \
    setsid sleep sync touch tr udhcpc umount; do
    ln -s /bin/busybox "$ROOTFS_DIR/bin/$applet"
done
for binary in ip iw wpa_supplicant wpa_cli dropbear dropbearkey kmod; do
    copy_elf "$binary"
done
install -D -m 0755 /lib/aarch64-linux-gnu/ld-linux-aarch64.so.1 \
    "$ROOTFS_DIR/lib/ld-linux-aarch64.so.1"
mkdir -p "$ROOTFS_DIR/sbin"
ln -s /usr/bin/kmod "$ROOTFS_DIR/sbin/modprobe"
ln -s /usr/bin/kmod "$ROOTFS_DIR/sbin/depmod"
ln -s /usr/bin/kmod "$ROOTFS_DIR/sbin/modinfo"

mkdir -p "$ROOTFS_DIR/usr/share/licenses/debian"
for package in busybox-static kmod iproute2 iw wpasupplicant dropbear-bin; do
    install -m 0644 "/usr/share/doc/$package/copyright" \
        "$ROOTFS_DIR/usr/share/licenses/debian/$package-copyright"
done

sed -i "s/VERSION_ID=.*/VERSION_ID=\"$VERSION\"/" "$ROOTFS_DIR/etc/os-release"
sed -i "s/PRETTY_NAME=.*/PRETTY_NAME=\"plumOS Pixel2 $VERSION\"/" \
    "$ROOTFS_DIR/etc/os-release"
KERNEL_RELEASE=$(find "$ROOTFS_DIR/lib/modules" -mindepth 1 -maxdepth 1 \
    -type d -printf '%f\n')
cat >"$ROOTFS_DIR/usr/lib/plumos/system-manifest.json" <<EOF
{
  "name": "plumOS Pixel2 System",
  "device": "pixel2",
  "architecture": "aarch64",
  "version": "$VERSION",
  "runtime_abi": "plumos-pixel2-v1",
  "kernel_release": "$KERNEL_RELEASE",
  "firmware_origin": "captured-stock-kernel-overlay",
  "source_ref": "$SOURCE_REF",
  "source_date_epoch": $SOURCE_EPOCH,
  "rootfs": "squashfs"
}
EOF

find "$ROOTFS_DIR" -exec touch -h -d "@$SOURCE_EPOCH" {} +
env -u SOURCE_DATE_EPOCH mksquashfs "$ROOTFS_DIR" "$PAYLOAD_DIR/SYSTEM" -noappend -all-root \
    -no-xattrs -comp xz -mkfs-time "$SOURCE_EPOCH" -all-time "$SOURCE_EPOCH" \
    >/dev/null

size=$(stat -c '%s' "$PAYLOAD_DIR/SYSTEM")
sha=$(sha256sum "$PAYLOAD_DIR/SYSTEM" | awk '{print $1}')
cat >"$PAYLOAD_DIR/SYSTEM.manifest" <<EOF
format=plumos-pixel2-system-v1
device=pixel2
architecture=aarch64
version=$VERSION
runtime_abi=plumos-pixel2-v1
source_ref=$SOURCE_REF
source_date_epoch=$SOURCE_EPOCH
image_size=$size
image_sha256=$sha
EOF
(cd "$PAYLOAD_DIR" && sha256sum SYSTEM SYSTEM.manifest >checksums.sha256)
"$ROOT_DIR/scripts/verify-system-rootfs.sh" "$PAYLOAD_DIR/SYSTEM"
printf 'created: %s\n' "$PAYLOAD_DIR/SYSTEM"
