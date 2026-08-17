#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TOOLS_IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"

if [[ "${1:-}" != --inside ]]; then
    docker image inspect "$TOOLS_IMAGE" >/dev/null 2>&1 || \
        "$ROOT_DIR/scripts/build-tools-image.sh"
    exec docker run --rm --platform linux/arm64 \
        -v "$ROOT_DIR:/work" -w /work "$TOOLS_IMAGE" \
        ./scripts/build-pixel2-boot-dtb.sh --inside
fi

ROOT_DIR=/work
STOCK_DTB="$ROOT_DIR/artifacts/vendor/pixel2-stock/boot/rk3326s-gkd-pixel2.dtb"
STOCK_MANIFEST="$ROOT_DIR/artifacts/vendor/pixel2-stock/manifest.tsv"
OUT_DIR="$ROOT_DIR/output/boot/pixel2"
OUT_DTB="$OUT_DIR/rk3326s-gkd-pixel2.dtb"
USB_NODE=/usb@ff300000
REGULATOR_NODE=/i2c@ff180000/pmic@20/regulators/OTG_SWITCH

for tool in fdtget fdtput dtc python3 sha256sum; do
    command -v "$tool" >/dev/null 2>&1 || {
        printf 'error: required DTB tool missing: %s\n' "$tool" >&2
        exit 2
    }
done
[ -f "$STOCK_DTB" ] && [ -f "$STOCK_MANIFEST" ] || {
    printf 'error: registered stock Pixel2 DTB input is missing\n' >&2
    exit 2
}
expected_sha="$(awk -F '\t' \
    '$1 == "file" && $2 == "boot/rk3326s-gkd-pixel2.dtb" { print $4 }' \
    "$STOCK_MANIFEST")"
actual_sha="$(sha256sum "$STOCK_DTB" | awk '{print $1}')"
[ -n "$expected_sha" ] && [ "$actual_sha" = "$expected_sha" ] || {
    printf 'error: stock Pixel2 DTB does not match its provenance manifest\n' >&2
    exit 1
}
if fdtget "$STOCK_DTB" "$USB_NODE" vbus-supply >/dev/null 2>&1; then
    printf 'error: stock Pixel2 DTB unexpectedly already owns DWC2 VBUS\n' >&2
    exit 1
fi
phandle="$(fdtget -t x "$STOCK_DTB" "$REGULATOR_NODE" phandle)"
case "$phandle" in
    ''|*[!0-9a-fA-F]*) printf 'error: invalid OTG_SWITCH phandle: %s\n' "$phandle" >&2; exit 1 ;;
esac

tmp="$(mktemp -d /tmp/plumos-pixel2-boot-dtb.XXXXXX)"
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$OUT_DIR"
cp "$STOCK_DTB" "$tmp/patched.dtb"
fdtput -t x "$tmp/patched.dtb" "$USB_NODE" vbus-supply "0x$phandle"
[ "$(fdtget -t x "$tmp/patched.dtb" "$USB_NODE" vbus-supply)" = "$phandle" ]
dtc -q -I dtb -O dts -o "$tmp/stock.dts" "$STOCK_DTB"
dtc -q -I dtb -O dts -o "$tmp/patched.dts" "$tmp/patched.dtb"
python3 "$ROOT_DIR/scripts/verify-pixel2-boot-dtb-diff.py" \
    --stock-dts "$tmp/stock.dts" --patched-dts "$tmp/patched.dts" \
    --phandle "$phandle"
install -m 0644 "$tmp/patched.dtb" "$OUT_DTB"

patched_sha="$(sha256sum "$OUT_DTB" | awk '{print $1}')"
cat >"$OUT_DIR/manifest.txt" <<EOF
format=plumos-pixel2-boot-dtb-v1
device=pixel2
kernel_abi=5.10.198
source=stock-runtime-dtb
source_sha256=$actual_sha
patch=usb@ff300000:vbus-supply:OTG_SWITCH
otg_switch_phandle=0x$phandle
output_sha256=$patched_sha
EOF
printf 'pixel2_boot_dtb=result-ok source_sha256=%s output_sha256=%s phandle=0x%s\n' \
    "$actual_sha" "$patched_sha" "$phandle"
