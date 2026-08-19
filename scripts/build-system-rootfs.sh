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
DISPATCHER_DIR="$OUT_DIR/dispatcher"
VERSION="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}"
SOURCE_REF="$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || printf unknown)"
SOURCE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[ -n "$SOURCE_EPOCH" ] || SOURCE_EPOCH="$(git -C "$ROOT_DIR" show -s --format=%ct HEAD)"

case "$SOURCE_EPOCH" in
    ''|*[!0-9]*) printf 'error: invalid SOURCE_DATE_EPOCH\n' >&2; exit 2 ;;
esac

"$ROOT_DIR/tests/test-pixel2-update.sh"

rm -rf "$OUT_DIR"
mkdir -p "$ROOTFS_DIR" "$PAYLOAD_DIR/system-slots" "$DISPATCHER_DIR/bin" \
    "$DISPATCHER_DIR/sbin" "$DISPATCHER_DIR/usr/lib/systemd" \
    "$DISPATCHER_DIR/dev/pts" "$DISPATCHER_DIR/proc" "$DISPATCHER_DIR/sys" \
    "$DISPATCHER_DIR/flash" "$DISPATCHER_DIR/storage" "$DISPATCHER_DIR/newroot" \
    "$ROOTFS_DIR/dev/pts" "$ROOTFS_DIR/proc" "$ROOTFS_DIR/sys" \
    "$ROOTFS_DIR/run" "$ROOTFS_DIR/tmp" "$ROOTFS_DIR/boot" \
    "$ROOTFS_DIR/.plumos-dispatcher-old" \
    "$ROOTFS_DIR/flash" "$ROOTFS_DIR/storage" \
    "$ROOTFS_DIR/state" "$ROOTFS_DIR/roms" "$ROOTFS_DIR/root" \
    "$ROOTFS_DIR/mnt/plumos" "$ROOTFS_DIR/mnt/plumos-user"
cp -a "$ROOT_DIR/rootfs/pixel2/." "$ROOTFS_DIR/"
"$ROOT_DIR/scripts/build-adbd-overlay.sh" --inside "$ROOTFS_DIR"
if ! "$ROOT_DIR/scripts/build-rtl8821cu-pixel2.sh" --inside --verify-output; then
    "$ROOT_DIR/scripts/build-rtl8821cu-pixel2.sh" --inside
fi
"$ROOT_DIR/scripts/install-kernel-runtime.sh" "$ROOTFS_DIR"
"$ROOT_DIR/scripts/install-frontend-rootfs.sh" "$ROOTFS_DIR"
chmod 0755 "$ROOTFS_DIR/sbin/init" "$ROOTFS_DIR/usr/lib/systemd/systemd" \
    "$ROOTFS_DIR/usr/lib/plumos/init.d/"*
chmod 0755 "$ROOTFS_DIR/usr/lib/plumos/plumos-pixel2-usb-role"
chmod 0755 "$ROOTFS_DIR/usr/bin/plumos-diagnostics"
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
for applet in basename cat chmod chown cttyhack cut date dirname env grep \
    hostname ip kill ln logger ls mkdir mdev mount mv rm sed \
    setsid sleep sync touch tr udhcpc umount; do
    ln -s /bin/busybox "$ROOTFS_DIR/bin/$applet"
done
ln -s /bin/busybox "$ROOTFS_DIR/usr/bin/env"
mkdir -p "$ROOTFS_DIR/usr/lib"
ln -s /mnt/plumos/ssh/libexec/sftp-server "$ROOTFS_DIR/usr/lib/sftp-server"
for binary in ip iw wpa_supplicant wpa_cli dropbear dropbearkey kmod python3 openssl \
    blkid sfdisk partx resize2fs mkfs.fat fsck.fat; do
    copy_elf "$binary"
done
cp -a /usr/lib/python3.11 "$ROOTFS_DIR/usr/lib/"
find /usr/lib/python3.11/lib-dynload -type f -name '*.so' -print0 | \
    while IFS= read -r -d '' extension; do
        ldd "$extension" 2>/dev/null | awk '
            /=> \// { print $3 }
            /^\// { print $1 }
        ' | while IFS= read -r lib; do
            [ -f "$lib" ] || continue
            mkdir -p "$ROOTFS_DIR${lib%/*}"
            cp -L "$lib" "$ROOTFS_DIR$lib"
        done
    done
install -D -m 0755 "$ROOT_DIR/scripts/plumos-system-update.py" \
    "$ROOTFS_DIR/usr/sbin/plumos-system-update"
install -D -m 0644 "$ROOT_DIR/package/system-pixel2/plumos-update-public.pem" \
    "$ROOTFS_DIR/etc/plumos-update-public.pem"
progress_dir="$OUT_DIR/update-progress"
python3 "$ROOT_DIR/scripts/generate-pixel2-update-progress.py" \
    --output-dir "$progress_dir"
for frame in "$progress_dir"/*.raw; do
    install -D -m 0644 "$frame" \
        "$ROOTFS_DIR/usr/share/plumos/update-progress/$(basename "$frame")"
done
printf '%s\n' 'plumos-pixel2-v1' >"$ROOTFS_DIR/etc/plumos-system-abi"
printf '%s\n' "$VERSION" >"$ROOTFS_DIR/etc/plumos-system-version"
install -D -m 0755 /lib/aarch64-linux-gnu/ld-linux-aarch64.so.1 \
    "$ROOTFS_DIR/lib/ld-linux-aarch64.so.1"
mkdir -p "$ROOTFS_DIR/sbin"
ln -s /usr/bin/kmod "$ROOTFS_DIR/sbin/modprobe"
ln -s /usr/bin/kmod "$ROOTFS_DIR/sbin/depmod"
ln -s /usr/bin/kmod "$ROOTFS_DIR/sbin/modinfo"

mkdir -p "$ROOTFS_DIR/usr/share/licenses/debian"
for package in busybox-static kmod iproute2 iw wpasupplicant dropbear-bin \
    python3.11-minimal openssl fdisk util-linux e2fsprogs dosfstools; do
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
  "runtime_abi": "plumos-pixel2-app-layer-v1",
  "kernel_release": "$KERNEL_RELEASE",
  "boot_substrate": "stock-pixel2",
  "firmware_origin": "stock-kernel-overlay",
  "source_ref": "$SOURCE_REF",
  "source_date_epoch": $SOURCE_EPOCH,
  "rootfs": "squashfs"
}
EOF

find "$ROOTFS_DIR" -exec touch -h -d "@$SOURCE_EPOCH" {} +
env -u SOURCE_DATE_EPOCH mksquashfs "$ROOTFS_DIR" "$PAYLOAD_DIR/system-slots/system-a.squashfs" -noappend -all-root \
    -no-xattrs -comp xz -mkfs-time "$SOURCE_EPOCH" -all-time "$SOURCE_EPOCH" \
    >/dev/null

size=$(stat -c '%s' "$PAYLOAD_DIR/system-slots/system-a.squashfs")
sha=$(sha256sum "$PAYLOAD_DIR/system-slots/system-a.squashfs" | awk '{print $1}')
cat >"$PAYLOAD_DIR/system-slots/system-a.manifest" <<EOF
format=plumos-pixel2-system-v1
device=pixel2
architecture=aarch64
version=$VERSION
runtime_abi=plumos-pixel2-app-layer-v1
source_ref=$SOURCE_REF
source_date_epoch=$SOURCE_EPOCH
image_size=$size
image_sha256=$sha
EOF
cp -a "$PAYLOAD_DIR/system-slots/system-a.squashfs" \
    "$PAYLOAD_DIR/system-slots/system-b.squashfs"
cp -a "$PAYLOAD_DIR/system-slots/system-a.manifest" \
    "$PAYLOAD_DIR/system-slots/system-b.manifest"
printf '%s  system-a.squashfs\n' "$sha" >"$PAYLOAD_DIR/system-slots/system-a.sha256"
printf '%s  system-b.squashfs\n' "$sha" >"$PAYLOAD_DIR/system-slots/system-b.sha256"

install -m 0755 "$ROOT_DIR/rootfs/pixel2-dispatcher/init" "$DISPATCHER_DIR/init"
install -m 0755 /bin/busybox "$DISPATCHER_DIR/bin/busybox"
ln -s busybox "$DISPATCHER_DIR/bin/sh"
ln -s /init "$DISPATCHER_DIR/sbin/init"
ln -s /init "$DISPATCHER_DIR/usr/lib/systemd/systemd"
find "$DISPATCHER_DIR" -exec touch -h -d "@$SOURCE_EPOCH" {} +
env -u SOURCE_DATE_EPOCH mksquashfs "$DISPATCHER_DIR" "$PAYLOAD_DIR/SYSTEM" \
    -noappend -all-root -no-xattrs -comp xz -mkfs-time "$SOURCE_EPOCH" \
    -all-time "$SOURCE_EPOCH" >/dev/null

dispatcher_size=$(stat -c '%s' "$PAYLOAD_DIR/SYSTEM")
dispatcher_sha=$(sha256sum "$PAYLOAD_DIR/SYSTEM" | awk '{print $1}')
cat >"$PAYLOAD_DIR/SYSTEM.manifest" <<EOF
format=plumos-pixel2-system-dispatcher-v1
device=pixel2
architecture=aarch64
version=$VERSION
source_ref=$SOURCE_REF
source_date_epoch=$SOURCE_EPOCH
image_size=$dispatcher_size
image_sha256=$dispatcher_sha
slot_a_sha256=$sha
slot_b_sha256=$sha
EOF
(cd "$PAYLOAD_DIR" && find SYSTEM system-slots -type f -print0 | sort -z | \
    xargs -0 sha256sum >checksums.sha256)
"$ROOT_DIR/scripts/verify-system-dispatcher.sh" "$PAYLOAD_DIR/SYSTEM"
"$ROOT_DIR/scripts/verify-system-rootfs.sh" "$PAYLOAD_DIR/system-slots/system-a.squashfs"
"$ROOT_DIR/scripts/verify-system-rootfs.sh" "$PAYLOAD_DIR/system-slots/system-b.squashfs"
printf 'created: %s with System A/B slots\n' "$PAYLOAD_DIR/SYSTEM"
