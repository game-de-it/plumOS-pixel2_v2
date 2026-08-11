#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
ROOTFS_DIR="${1:-}"
KERNEL_OUT="$ROOT_DIR/output/kernel/pixel2"
MODULES_SRC="$KERNEL_OUT/modules/lib/modules"
FIRMWARE_SRC="$ROOT_DIR/artifacts/vendor/pixel2-stock/kernel/extracted/usr/lib/kernel-overlays/base/lib/firmware"

[ -d "$ROOTFS_DIR" ] || {
    printf 'usage: %s ROOTFS_DIR\n' "$0" >&2
    exit 2
}
[ -f "$KERNEL_OUT/Image" ] && [ -d "$MODULES_SRC" ] || {
    printf 'error: Pixel2 kernel output missing; run ./scripts/build-kernel.sh first\n' >&2
    exit 2
}
[ -d "$FIRMWARE_SRC" ] || {
    printf 'error: captured stock firmware missing; run ./scripts/capture-stock-boot.sh first\n' >&2
    exit 2
}

mapfile -t releases < <(find "$MODULES_SRC" -mindepth 1 -maxdepth 1 -type d -printf '%f\n')
[ "${#releases[@]}" -eq 1 ] || {
    printf 'error: expected exactly one kernel module release\n' >&2
    exit 2
}
release=${releases[0]}
case "$release" in
    *-plumos-pixel2) ;;
    *) printf 'error: unexpected kernel module ABI: %s\n' "$release" >&2; exit 2 ;;
esac

mkdir -p "$ROOTFS_DIR/lib/modules" "$ROOTFS_DIR/lib/firmware" \
    "$ROOTFS_DIR/usr/lib/plumos"
cp -a "$MODULES_SRC/$release" "$ROOTFS_DIR/lib/modules/"

# Minimal firmware set for the USB Wi-Fi families enabled by the Pixel2 kernel.
# These files are captured independently from the stock kernel overlay; no stock
# executable, service, configuration, or userspace library is copied.
while IFS= read -r relative; do
    [ -n "$relative" ] || continue
    [ -f "$FIRMWARE_SRC/$relative" ] || {
        printf 'error: required USB Wi-Fi firmware missing: %s\n' "$relative" >&2
        exit 2
    }
    install -D -m 0644 "$FIRMWARE_SRC/$relative" \
        "$ROOTFS_DIR/lib/firmware/$relative"
done <<'EOF'
regulatory.db
regulatory.db.p7s
ath9k_htc/htc_7010-1.4.0.fw
ath9k_htc/htc_9271-1.4.0.fw
carl9170-1.fw
mt7601u.bin
mediatek/mt7610u.bin
mediatek/mt7662u.bin
mediatek/mt7662u_rom_patch.bin
rt2870.bin
rt73.bin
rtlwifi/rtl8192cufw.bin
rtlwifi/rtl8192cufw_A.bin
rtlwifi/rtl8192cufw_B.bin
EOF

MANIFEST="$ROOTFS_DIR/usr/lib/plumos/kernel-runtime.sha256"
(
    cd "$ROOTFS_DIR"
    find "lib/modules/$release" lib/firmware -type f -print0 | \
        LC_ALL=C sort -z | xargs -0 sha256sum
) >"$MANIFEST"
printf 'kernel-runtime=result-ok release=%s firmware_source=stock-kernel-overlay\n' \
    "$release"
