#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
VERIFY="$ROOT_DIR/scripts/verify-pixel2-boot-dtb-diff.py"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-dtb-diff.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

printf '%s\n' \
    '/dts-v1/;' \
    '' \
    '/ {' \
    $'\tusb@ff300000 {' \
    $'\t\tdr_mode = "otg";' \
    $'\t};' \
    '};' >"$TMP/stock.dts"
printf '%s\n' \
    '/dts-v1/;' \
    '' \
    '/ {' \
    $'\tusb@ff300000 {' \
    $'\t\tvbus-supply = <0xfe>;' \
    $'\t\tdr_mode = "otg";' \
    $'\t};' \
    '};' >"$TMP/patched.dts"

python3 "$VERIFY" --stock-dts "$TMP/stock.dts" \
    --patched-dts "$TMP/patched.dts" --phandle fe

cp "$TMP/patched.dts" "$TMP/invalid.dts"
sed -i.bak '/dr_mode/a\
\	\	status = "okay";' "$TMP/invalid.dts"
if python3 "$VERIFY" --stock-dts "$TMP/stock.dts" \
    --patched-dts "$TMP/invalid.dts" --phandle fe >/dev/null 2>&1; then
    printf 'error: DTB diff gate accepted an unrelated property\n' >&2
    exit 1
fi

printf 'pixel2_boot_dtb_diff_test=result-ok\n'
