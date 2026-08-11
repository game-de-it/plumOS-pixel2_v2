#!/usr/bin/env bash
set -euo pipefail

IMAGE="${1:-}"
[ -f "$IMAGE" ] || { printf 'usage: %s SYSTEM\n' "$0" >&2; exit 2; }

tmp="$(mktemp -d /tmp/plumos-pixel2-rootfs.XXXXXX)"
trap 'rm -rf "$tmp"' EXIT
unsquashfs -d "$tmp/rootfs" "$IMAGE" >/dev/null

test -x "$tmp/rootfs/sbin/init"
grep -q '^ID=plumos$' "$tmp/rootfs/etc/os-release"
grep -q '"device": "pixel2"' "$tmp/rootfs/usr/lib/plumos/system-manifest.json"
test -x "$tmp/rootfs/usr/lib/plumos/init.d/20-usb-wifi"
test -x "$tmp/rootfs/usr/lib/plumos/init.d/30-ssh"
test -x "$tmp/rootfs/lib/ld-linux-aarch64.so.1"

if [ "$(uname -m)" = aarch64 ]; then
    chroot "$tmp/rootfs" /usr/sbin/wpa_supplicant -v >/dev/null
    chroot "$tmp/rootfs" /usr/sbin/iw --version >/dev/null
    chroot "$tmp/rootfs" /usr/sbin/dropbear -V >/dev/null
    chroot "$tmp/rootfs" /usr/bin/kmod --version >/dev/null
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
