#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BUILD="$ROOT_DIR/scripts/build-retroarch.sh"
CFG="$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg"
MENU_LAUNCHER="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-menu-launch"

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    exit 1
}

for build_flag in \
    --enable-rgui \
    --enable-materialui \
    --enable-ozone \
    --enable-xmb \
    --enable-freetype; do
    grep -Fq -- "$build_flag" "$BUILD" ||
        fail "RetroArch build flag is missing: $build_flag"
done
grep -Fq 'HAVE_LANGEXTRA = 1' "$BUILD" ||
    fail 'RetroArch multi-language build gate is missing'
grep -Eq '^ASSETS_SOURCE_COMMIT=[0-9a-f]{40}$' "$BUILD" ||
    fail 'RetroArch assets source is not pinned'
for asset_tree in fonts glui ozone pkg rgui sounds xmb; do
    grep -Fq 'for asset_tree in fonts glui ozone pkg rgui sounds xmb' "$BUILD" ||
        fail "RetroArch managed asset tree is missing: $asset_tree"
done

grep -Fqx 'assets_directory = "/mnt/plumos/retroarch/assets"' "$CFG" ||
    fail 'RetroArch factory config does not use managed menu assets'
grep -Fqx 'menu_driver = "rgui"' "$CFG" ||
    fail 'RetroArch factory default menu must remain RGUI'
grep -Fqx 'user_language = "0"' "$CFG" ||
    fail 'RetroArch factory default language must remain English'

if grep -Eq '^[[:space:]]*(menu_driver|user_language)[[:space:]]*=' \
        "$MENU_LAUNCHER"; then
    fail 'RetroArch menu launcher overrides persistent menu or language choice'
fi

printf 'PASS: Pixel2 RetroArch menu and localization contract\n'
