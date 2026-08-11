#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
ROOTFS_DIR="${1:-}"
SOURCE_DIR="$ROOT_DIR/vendor/plumos-frontend/src"
VENDOR_DIR="$ROOT_DIR/vendor/plumos-frontend"
SEED_DST="$ROOTFS_DIR/usr/share/plumos/frontend-seed"

[ -d "$ROOTFS_DIR" ] || {
    printf 'usage: %s ROOTFS_DIR\n' "$0" >&2
    exit 2
}

mkdir -p "$ROOTFS_DIR/usr/bin" "$SEED_DST" \
    "$ROOTFS_DIR/usr/share/licenses/plumos-frontend"
cp -a "$VENDOR_DIR/seed/." "$SEED_DST/"
install -m 0644 "$VENDOR_DIR/LICENSE" \
    "$ROOTFS_DIR/usr/share/licenses/plumos-frontend/LICENSE"
install -m 0644 "$VENDOR_DIR/SOURCE.manifest" \
    "$ROOTFS_DIR/usr/share/licenses/plumos-frontend/SOURCE.manifest"

common_flags=(-std=gnu99 -Os -pipe -Wall -Wextra -D_GNU_SOURCE)
png_cflags=$(pkg-config --cflags libpng)
png_libs=$(pkg-config --libs libpng)
freetype_cflags=$(pkg-config --cflags freetype2)
freetype_libs=$(pkg-config --libs freetype2)
drm_cflags=$(pkg-config --cflags libdrm)
drm_libs=$(pkg-config --libs libdrm)

# shellcheck disable=SC2086
gcc "${common_flags[@]}" $png_cflags $freetype_cflags $drm_cflags \
    -DPLUMOS_ENABLE_FBDEV_RENDERER=1 \
    -DPLUMOS_FBDEV_ENABLE_PNG=1 \
    -DPLUMOS_FBDEV_ENABLE_FREETYPE=1 \
    -DPLUMOS_FBDEV_ENABLE_DRM=1 \
    "$SOURCE_DIR/plumos_controller_ui.c" \
    -o "$ROOTFS_DIR/usr/bin/plumos-frontend-pixel2" \
    $png_libs $freetype_libs $drm_libs
gcc "${common_flags[@]}" "$SOURCE_DIR/plumos_library_scan.c" \
    -o "$ROOTFS_DIR/usr/bin/plumos-library-scan"
strip "$ROOTFS_DIR/usr/bin/plumos-frontend-pixel2" \
    "$ROOTFS_DIR/usr/bin/plumos-library-scan" 2>/dev/null || true
chmod 0755 "$ROOTFS_DIR/usr/bin/plumos-frontend-pixel2" \
    "$ROOTFS_DIR/usr/bin/plumos-library-scan"

copy_dependencies() {
    ldd "$1" 2>/dev/null | awk '
        /=> \// { print $3 }
        /^\// { print $1 }
    ' | while IFS= read -r library; do
        [ -f "$library" ] || continue
        mkdir -p "$ROOTFS_DIR${library%/*}"
        cp -L "$library" "$ROOTFS_DIR$library"
    done
}
copy_dependencies "$ROOTFS_DIR/usr/bin/plumos-frontend-pixel2"
copy_dependencies "$ROOTFS_DIR/usr/bin/plumos-library-scan"

printf 'frontend-rootfs=result-ok target=pixel2 renderer=drm-fbdev\n'
