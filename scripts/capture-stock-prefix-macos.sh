#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
DISK="${1:-}"
OUT="${2:-$ROOT_DIR/artifacts/vendor/pixel2-stock-source/rockchip-boot-prefix.bin}"

case "$DISK" in
    /dev/disk[0-9]*) ;;
    *)
        printf 'usage: %s /dev/diskN [output.bin]\n' "$0" >&2
        exit 2
        ;;
esac

[ ! -e "$OUT" ] || {
    printf 'error: refusing to overwrite existing file: %s\n' "$OUT" >&2
    exit 1
}

INFO="$(diskutil info "$DISK")"
printf '%s\n' "$INFO" | grep -q 'Removable Media:.*Removable' || {
    printf 'error: target is not reported as removable media: %s\n' "$DISK" >&2
    exit 1
}
printf '%s\n' "$INFO" | grep -q 'Protocol:.*Secure Digital' || {
    printf 'error: target is not reported as Secure Digital: %s\n' "$DISK" >&2
    exit 1
}

PARTITION_INFO="$(diskutil info "${DISK}s1")"
printf '%s\n' "$PARTITION_INFO" | grep -q 'Partition Offset:.*16777216 Bytes' || {
    printf 'error: partition 1 does not start at the expected 16 MiB boundary\n' >&2
    exit 1
}

mkdir -p "$(dirname -- "$OUT")"
TMP_OUT="$(mktemp "${OUT}.tmp.XXXXXX")"
RAW_DISK="${DISK/\/dev\/disk/\/dev\/rdisk}"

printf 'Reading the first 16 MiB from %s; the SD card is not written.\n' "$RAW_DISK"
sudo dd if="$RAW_DISK" of="$TMP_OUT" bs=1048576 count=16
sudo chown "$(id -u):$(id -g)" "$TMP_OUT"
[ "$(stat -f '%z' "$TMP_OUT")" -eq 16777216 ]
mv "$TMP_OUT" "$OUT"
shasum -a 256 "$OUT"
printf 'created: %s\n' "$OUT"

