#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if [ "${1:-}" != --inside ]; then
    exec "$ROOT_DIR/scripts/docker-build.sh" frontend "$@"
fi
shift

ROOT_DIR=/work
OUT_ROOT="$ROOT_DIR/output/frontend/pixel2"
PLUMOS_DIR="$OUT_ROOT/plumos"
BIN_DIR="$PLUMOS_DIR/bin"
LIB_DIR="$PLUMOS_DIR/frontend/lib"
COMPONENT_DIR="$PLUMOS_DIR/components/frontend"
SOURCE_DIR="$ROOT_DIR/vendor/plumos-frontend/src"
VERSION="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}"
SOURCE_REF="$(git -C "$ROOT_DIR" rev-parse --short HEAD)"
SOURCE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[ -n "$SOURCE_EPOCH" ] || SOURCE_EPOCH="$(git -C "$ROOT_DIR" show -s --format=%ct HEAD)"

rm -rf "$OUT_ROOT"
mkdir -p "$BIN_DIR" "$LIB_DIR" "$COMPONENT_DIR" \
    "$PLUMOS_DIR/licenses/plumos-frontend"
cp -a "$ROOT_DIR/vendor/plumos-frontend/seed/." "$PLUMOS_DIR/"
install -m 0644 "$ROOT_DIR/package/frontend-pixel2/systems.json" \
    "$PLUMOS_DIR/config/frontend/systems.json"
install -m 0644 "$ROOT_DIR/package/frontend-pixel2/menus.json" \
    "$PLUMOS_DIR/config/frontend/menus.json"
install -m 0644 "$ROOT_DIR/package/frontend-pixel2/apps.json" \
    "$PLUMOS_DIR/config/frontend/apps.json"
install -m 0644 "$ROOT_DIR/vendor/plumos-frontend/LICENSE" \
    "$PLUMOS_DIR/licenses/plumos-frontend/LICENSE"
install -m 0644 "$ROOT_DIR/vendor/plumos-frontend/SOURCE.manifest" \
    "$PLUMOS_DIR/licenses/plumos-frontend/SOURCE.manifest"

common=(-std=gnu99 -Os -pipe -Wall -Wextra -D_GNU_SOURCE)
png_cflags=$(pkg-config --cflags libpng)
png_libs=$(pkg-config --libs libpng)
freetype_cflags=$(pkg-config --cflags freetype2)
freetype_libs=$(pkg-config --libs freetype2)
drm_cflags=$(pkg-config --cflags libdrm)
drm_libs=$(pkg-config --libs libdrm)
# shellcheck disable=SC2086
gcc "${common[@]}" $png_cflags $freetype_cflags $drm_cflags \
    -DPLUMOS_ENABLE_FBDEV_RENDERER=1 \
    -DPLUMOS_FBDEV_ENABLE_PNG=1 \
    -DPLUMOS_FBDEV_ENABLE_FREETYPE=1 \
    -DPLUMOS_FBDEV_ENABLE_DRM=1 \
    "$SOURCE_DIR/plumos_controller_ui.c" -o "$BIN_DIR/plumos-frontend-pixel2" \
    $png_libs $freetype_libs $drm_libs
gcc "${common[@]}" "$SOURCE_DIR/plumos_library_scan.c" \
    -o "$BIN_DIR/plumos-library-scan"
gcc "${common[@]}" "$SOURCE_DIR/plumos_text_ui.c" \
    -o "$BIN_DIR/plumos-text-ui"
gcc "${common[@]}" "$SOURCE_DIR/plumos_frontend.c" \
    -o "$BIN_DIR/plumos-frontend-diagnostics"
gcc "${common[@]}" "$SOURCE_DIR/plumos_pixel2_hardware_keys.c" \
    -o "$BIN_DIR/plumos-hardware-keys"
strip "$BIN_DIR"/* 2>/dev/null || true
chmod 0755 "$BIN_DIR"/*

copied=' '
for binary in "$BIN_DIR"/*; do
    ldd "$binary" 2>/dev/null | awk '/=> \// {print $3} /^\// {print $1}'
done | sort -u | while IFS= read -r library; do
    [ -f "$library" ] || continue
    base=${library##*/}
    case "$base" in libc.so.*|libm.so.*|libdl.so.*|libpthread.so.*|librt.so.*|ld-linux-*.so.*) continue ;; esac
    install -m 0644 "$library" "$LIB_DIR/$base"
done

cat >"$COMPONENT_DIR/manifest.json" <<EOF
{
  "name": "plumOS Pixel2 frontend",
  "component": "frontend",
  "device": "pixel2",
  "architecture": "aarch64",
  "version": "$VERSION",
  "source_ref": "$SOURCE_REF",
  "source_date_epoch": $SOURCE_EPOCH,
  "renderer": "drm-fbdev-ccw",
  "input": "gkd-pixel2-joypad",
  "hardware_key_daemon": "bin/plumos-hardware-keys",
  "resolver": "bin/plumos-text-ui",
  "catalog": "config/frontend/systems.json"
}
EOF
(
    cd "$PLUMOS_DIR"
    find bin config factory-defaults frontend share themes licenses/plumos-frontend \
        -type f -print | sort | while IFS= read -r file; do sha256sum "$file"; done
    sha256sum components/frontend/manifest.json
) >"$COMPONENT_DIR/checksums.sha256"
printf 'frontend_component=result-ok output=%s\n' "$PLUMOS_DIR"
