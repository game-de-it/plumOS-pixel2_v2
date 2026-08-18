#!/bin/bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
DEVICE="${1:-}"
EXT4FUSE="${PLUMOS_EXT4FUSE:-$(command -v ext4fuse || true)}"

usage() {
    printf 'Usage: %s /dev/diskNsM\n' "$0" >&2
}

case "$DEVICE" in
    /dev/disk[0-9]*s[0-9]*) ;;
    *) usage; exit 2 ;;
esac
[ -e "$DEVICE" ] || { printf 'error: device not found: %s\n' "$DEVICE" >&2; exit 1; }
[ -n "$EXT4FUSE" ] && [ -x "$EXT4FUSE" ] || {
    printf 'error: ext4fuse is not installed\n' >&2
    exit 1
}

INFO_PLIST="$(mktemp "${TMPDIR:-/tmp}/plumos-pixel2-disk-info.XXXXXX")"
MOUNT_DIR="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-runtime-ro.XXXXXX")"
CAPTURE_PARENT="$ROOT_DIR/output/live/$(date '+%Y-%m-%d')-pixel2-state-capture"
mkdir -p "$CAPTURE_PARENT"
CAPTURE_DIR="$(mktemp -d "$CAPTURE_PARENT/capture.XXXXXX")"
MOUNTED=0

cleanup() {
    if [ "$MOUNTED" -eq 1 ]; then
        sudo /sbin/umount "$MOUNT_DIR" >/dev/null 2>&1 || true
    fi
    /bin/rm -f "$INFO_PLIST"
    rmdir "$MOUNT_DIR" 2>/dev/null || true
}
trap cleanup EXIT INT TERM HUP

/usr/sbin/diskutil info -plist "$DEVICE" >"$INFO_PLIST"
CONTENT="$(/usr/bin/plutil -extract Content raw -o - "$INFO_PLIST")"
SIZE="$(/usr/bin/plutil -extract Size raw -o - "$INFO_PLIST")"
PARENT="$(/usr/bin/plutil -extract ParentWholeDisk raw -o - "$INFO_PLIST")"
[ "$CONTENT" = Linux ] || {
    printf 'error: refusing non-Linux partition: content=%s\n' "$CONTENT" >&2
    exit 1
}
case "$SIZE" in ''|*[!0-9]*) printf 'error: invalid partition size\n' >&2; exit 1 ;; esac
[ "$SIZE" -ge 1073741824 ] && [ "$SIZE" -le 34359738368 ] || {
    printf 'error: refusing unexpected Linux partition size: %s\n' "$SIZE" >&2
    exit 1
}

printf 'Pixel2 Runtime partition: %s parent=%s size=%s bytes\n' \
    "$DEVICE" "$PARENT" "$SIZE"
printf 'Requesting administrator access for a read-only ext4 mount.\n'
sudo -v
sudo "$EXT4FUSE" "$DEVICE" "$MOUNT_DIR" -o ro,allow_other
MOUNTED=1

copy_capture_path() {
    relative="$1"
    source="$MOUNT_DIR/$relative"
    destination="$CAPTURE_DIR/runtime/$relative"
    sudo test -e "$source" || return 0
    mkdir -p "$(dirname -- "$destination")"
    sudo /bin/cp -R -p "$source" "$destination"
}

for relative in \
    VERSION COMPAT_VENDOR RUNTIME_ABI manifest.json checksums.sha256 \
    config/network/services.conf config/system/settings.json \
    update-state logs; do
    copy_capture_path "$relative"
done

if [ -d /Volumes/PLUMOS_BOOT/system-slots ]; then
    mkdir -p "$CAPTURE_DIR/boot"
    for name in system-a.manifest system-a.manifest.json system-a.manifest.sig \
        system-a.sha256 system-b.manifest system-b.manifest.json \
        system-b.manifest.sig system-b.sha256; do
        [ -f "/Volumes/PLUMOS_BOOT/system-slots/$name" ] || continue
        /bin/cp -p "/Volumes/PLUMOS_BOOT/system-slots/$name" "$CAPTURE_DIR/boot/$name"
    done
fi

sudo /usr/sbin/chown -R "$(id -u):$(id -g)" "$CAPTURE_DIR"
(
    cd "$CAPTURE_DIR"
    find . -type f ! -name CAPTURE.sha256 -print0 | LC_ALL=C sort -z |
        xargs -0 shasum -a 256 >CAPTURE.sha256
)
sync
sudo /sbin/umount "$MOUNT_DIR"
MOUNTED=0
rmdir "$MOUNT_DIR"

printf 'capture=result-ok output=%s\n' "$CAPTURE_DIR"
