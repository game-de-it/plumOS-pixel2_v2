#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BUILD="$ROOT_DIR/scripts/build-retroarch.sh"
CFG="$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg"
MENU_LAUNCHER="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-menu-launch"
APP_LAYER_VERIFY="$ROOT_DIR/scripts/verify-app-layer.sh"
GL_MENU_PATCH="$ROOT_DIR/patches/retroarch/018-pixel2-gl-graphical-menu-rotation.patch"
GL_FONT_PATCH="$ROOT_DIR/patches/retroarch/019-pixel2-gl-menu-font-coordinates.patch"
GL_LAYOUT_PATCH="$ROOT_DIR/patches/retroarch/020-pixel2-gl-menu-logical-layout.patch"
GL_FONT_POSITION_PATCH="$ROOT_DIR/patches/retroarch/021-pixel2-gl-menu-font-position.patch"

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
grep -Fqx 'menu_scale_factor = "1.500000"' "$CFG" ||
    fail 'RetroArch graphical menus do not use the Pixel2 readable scale'
grep -Fqx 'ozone_font_scale = "1"' "$CFG" ||
    fail 'RetroArch Ozone global font scaling is not enabled'
grep -Fqx 'ozone_font_scale_factor_global = "1.350000"' "$CFG" ||
    fail 'RetroArch Ozone font scale is not readable on Pixel2'

if grep -Eq '^[[:space:]]*(menu_driver|user_language)[[:space:]]*=[[:space:]]*"' \
        "$MENU_LAUNCHER"; then
    fail 'RetroArch menu launcher overrides persistent menu or language choice'
fi
for contract in \
    'glui|ozone|xmb)' \
    'video_driver=gl' \
    'PLUMOS_GL_MENU_ROTATION=display'; do
    grep -Fq "$contract" "$MENU_LAUNCHER" ||
        fail "Graphical RetroArch menu launch contract is missing: $contract"
done
for contract in \
    'gl2_menu_display_rotation' \
    'gl2_rotate_menu_rect' \
    'rotated_menu_video_info' \
    'gl2_raster_font_draw_vertices'; do
    grep -Fq "$contract" "$GL_MENU_PATCH" ||
        fail "Graphical RetroArch menu rotation patch is missing: $contract"
done
for contract in \
    'gl2_menu_font_width' \
    'gl2_menu_font_height' \
    'window_width' \
    'window_height'; do
    grep -Fq "$contract" "$GL_FONT_PATCH" ||
        fail "Graphical RetroArch menu font-coordinate patch is missing: $contract"
done
for contract in \
    'menu_render_width  = video_st->width' \
    'menu_render_width  = video_st->height' \
    'PLUMOS_GL_MENU_ROTATION' \
    'menu->driver_ctx->render'; do
    grep -Fq "$contract" "$GL_LAYOUT_PATCH" ||
        fail "Graphical RetroArch logical menu layout patch is missing: $contract"
done
for contract in \
    'x *= (float)gl->vp.width / (float)gl2_menu_font_width(gl)' \
    'y  = 1.0f - ((1.0f - y)' \
    'gl->vp.height / (float)gl2_menu_font_height(gl)'; do
    grep -Fq "$contract" "$GL_FONT_POSITION_PATCH" ||
        fail "Graphical RetroArch font-position patch is missing: $contract"
done
for font_pattern in \
    'retroarch/assets/rgui/font/bitmap10x10_*.bin' \
    'retroarch/assets/rgui/font/bitmap6x10_*.bin'; do
    grep -Fq "$font_pattern" "$APP_LAYER_VERIFY" ||
        fail "RGUI language font is rejected as ROM content: $font_pattern"
done

printf 'PASS: Pixel2 RetroArch menu and localization contract\n'
