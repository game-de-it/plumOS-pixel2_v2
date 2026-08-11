#!/usr/bin/env bash
set -euo pipefail

IMAGE="${1:-}"
[ -f "$IMAGE" ] || { printf 'usage: %s SYSTEM\n' "$0" >&2; exit 2; }
ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

if ! command -v unsquashfs >/dev/null 2>&1; then
    IMAGE_PATH="$(CDPATH= cd -- "$(dirname -- "$IMAGE")" && pwd)/$(basename -- "$IMAGE")"
    case "$IMAGE_PATH" in
        "$ROOT_DIR"/*) ;;
        *) printf 'error: image must be under repository for Docker verification\n' >&2; exit 2 ;;
    esac
    relative=${IMAGE_PATH#"$ROOT_DIR"/}
    TOOL_IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"
    docker image inspect "$TOOL_IMAGE" >/dev/null 2>&1 || \
        "$ROOT_DIR/scripts/build-tools-image.sh"
    exec docker run --rm --platform linux/arm64 \
        -v "$ROOT_DIR:/work" -w /work "$TOOL_IMAGE" \
        ./scripts/verify-system-rootfs.sh "/work/$relative"
fi

tmp="$(mktemp -d /tmp/plumos-pixel2-rootfs.XXXXXX)"
trap 'rm -rf "$tmp"' EXIT
unsquashfs -d "$tmp/rootfs" "$IMAGE" >/dev/null

test -x "$tmp/rootfs/sbin/init"
grep -q '^ID=plumos$' "$tmp/rootfs/etc/os-release"
grep -q '"device": "pixel2"' "$tmp/rootfs/usr/lib/plumos/system-manifest.json"
test -x "$tmp/rootfs/usr/lib/plumos/init.d/20-usb-wifi"
test -x "$tmp/rootfs/usr/lib/plumos/init.d/30-ssh"
test -x "$tmp/rootfs/usr/lib/plumos/init.d/10-adbd"
test -x "$tmp/rootfs/usr/sbin/adbd"
test -x "$tmp/rootfs/usr/lib/plumos/adbd/adbd.bin"
test -x "$tmp/rootfs/lib/ld-linux-aarch64.so.1"
for directory in dev dev/pts proc sys run tmp boot state roms root; do
    test -d "$tmp/rootfs/$directory" || {
        printf 'error: required rootfs mountpoint missing: /%s\n' "$directory" >&2
        exit 1
    }
done
test -f "$tmp/rootfs/usr/lib/plumos/kernel-runtime.sha256"
release=$(find "$tmp/rootfs/lib/modules" -mindepth 1 -maxdepth 1 -type d \
    -printf '%f\n')
case "$release" in
    *-plumos-pixel2) ;;
    *) printf 'error: unexpected kernel module ABI: %s\n' "$release" >&2; exit 1 ;;
esac
test -f "$tmp/rootfs/lib/firmware/regulatory.db"
test -f "$tmp/rootfs/lib/firmware/ath9k_htc/htc_9271-1.4.0.fw"
test -f "$tmp/rootfs/lib/firmware/mt7601u.bin"
test -f "$tmp/rootfs/lib/firmware/rtlwifi/rtl8192cufw.bin"
(cd "$tmp/rootfs" && sha256sum -c usr/lib/plumos/kernel-runtime.sha256 >/dev/null)

if [ "$(uname -m)" = aarch64 ]; then
    chroot "$tmp/rootfs" /usr/sbin/wpa_supplicant -v >/dev/null
    chroot "$tmp/rootfs" /usr/sbin/iw --version >/dev/null
    chroot "$tmp/rootfs" /usr/sbin/dropbear -V >/dev/null
    chroot "$tmp/rootfs" /usr/bin/kmod --version >/dev/null
    first_module=$(find "$tmp/rootfs/lib/modules/$release" -name '*.ko' -print -quit)
    test -n "$first_module"
    case "$(chroot "$tmp/rootfs" /sbin/modinfo -F vermagic \
        "${first_module#"$tmp/rootfs"}")" in
        "$release "*) ;;
        *) printf 'error: kernel module vermagic mismatch\n' >&2; exit 1 ;;
    esac
    chroot "$tmp/rootfs" /lib/ld-linux-aarch64.so.1 \
        --library-path /usr/lib/plumos/adbd/lib:/lib/aarch64-linux-gnu \
        --list /usr/lib/plumos/adbd/adbd.bin >/dev/null
fi

if find "$tmp/rootfs" -print | grep -Eiq '(rocknix|emuelec|batocera|knulli)'; then
    printf 'error: foreign distribution name in rootfs path\n' >&2
    exit 1
fi
if LC_ALL=C grep -R -a -E -i -n '(rocknix|emuelec|batocera|knulli)' \
    "$tmp/rootfs" >/dev/null; then
    printf 'error: foreign distribution identity in rootfs content\n' >&2
    exit 1
fi

printf 'system_rootfs=result-ok image=%s\n' "$IMAGE"
