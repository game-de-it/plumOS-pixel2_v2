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
SCRAPER_LIB_DIR="$PLUMOS_DIR/scraper/lib"
COMPONENT_DIR="$PLUMOS_DIR/components/frontend"
SOURCE_DIR="$ROOT_DIR/vendor/plumos-frontend/src"
VERSION="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}"
SOURCE_REF="$(git -C "$ROOT_DIR" rev-parse --short HEAD)"
SOURCE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[ -n "$SOURCE_EPOCH" ] || SOURCE_EPOCH="$(git -C "$ROOT_DIR" show -s --format=%ct HEAD)"

rm -rf "$OUT_ROOT"
mkdir -p "$BIN_DIR" "$LIB_DIR" "$SCRAPER_LIB_DIR" "$COMPONENT_DIR" \
    "$PLUMOS_DIR/licenses/plumos-frontend" "$PLUMOS_DIR/fonts"
cp -a "$ROOT_DIR/vendor/plumos-frontend/seed/." "$PLUMOS_DIR/"
rsync -a "$ROOT_DIR/package/app-layer-pixel2/" "$PLUMOS_DIR/"
install -m 0755 /usr/sbin/fsck.fat "$BIN_DIR/fsck.fat"
python3 "$ROOT_DIR/scripts/generate-pixel2-system-logos.py" \
    "$PLUMOS_DIR/themes/default/logos/systems"
install -m 0644 "$ROOT_DIR/package/frontend-pixel2/systems.json" \
    "$PLUMOS_DIR/config/frontend/systems.json"
install -m 0644 "$ROOT_DIR/package/frontend-pixel2/menus.json" \
    "$PLUMOS_DIR/config/frontend/menus.json"
install -m 0644 "$ROOT_DIR/package/frontend-pixel2/apps.json" \
    "$PLUMOS_DIR/config/frontend/apps.json"
install -m 0644 "$ROOT_DIR/package/frontend-pixel2/feature-contract.json" \
    "$PLUMOS_DIR/config/frontend/feature-contract.json"
install -m 0644 "$ROOT_DIR/package/frontend-pixel2/scraper-sources.tsv" \
    "$PLUMOS_DIR/config/frontend/scraper-sources.tsv"
install -m 0644 "$ROOT_DIR/vendor/plumos-frontend/LICENSE" \
    "$PLUMOS_DIR/licenses/plumos-frontend/LICENSE"
install -m 0644 "$ROOT_DIR/vendor/plumos-frontend/SOURCE.manifest" \
    "$PLUMOS_DIR/licenses/plumos-frontend/SOURCE.manifest"
install -m 0644 /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
    "$PLUMOS_DIR/fonts/default.otf"
install -m 0644 /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc \
    "$PLUMOS_DIR/fonts/cjk-fallback.ttc"
install -m 0644 /usr/share/doc/fonts-dejavu-core/copyright \
    "$PLUMOS_DIR/licenses/plumos-frontend/fonts-dejavu-copyright"
install -m 0644 /usr/share/doc/fonts-noto-cjk/copyright \
    "$PLUMOS_DIR/licenses/plumos-frontend/fonts-noto-cjk-copyright"

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
    file "$binary" | grep -q 'ELF ' || continue
    ldd "$binary" 2>/dev/null | awk '/=> \// {print $3} /^\// {print $1}'
done | sort -u | while IFS= read -r library; do
    [ -f "$library" ] || continue
    base=${library##*/}
    case "$base" in libc.so.*|libm.so.*|libdl.so.*|libpthread.so.*|librt.so.*|ld-linux-*.so.*) continue ;; esac
    install -m 0644 "$library" "$LIB_DIR/$base"
done

SCRAPER_RUNTIME_LIBS=""

scraper_runtime_needed() {
    readelf -d "$1" 2>/dev/null |
        awk '/NEEDED/ { gsub(/[][]/, "", $5); print $5 }'
}

find_scraper_runtime_library() {
    local soname="$1" candidate
    for candidate in \
        "/lib/aarch64-linux-gnu/$soname" \
        "/usr/lib/aarch64-linux-gnu/$soname" \
        "/lib/$soname" "/usr/lib/$soname"; do
        if [ -e "$candidate" ]; then
            readlink -f "$candidate"
            return 0
        fi
    done
    return 1
}

copy_scraper_runtime_library() {
    local soname="$1" source child
    case " $SCRAPER_RUNTIME_LIBS " in *" $soname "*) return 0 ;; esac
    source="$(find_scraper_runtime_library "$soname")" || {
        printf 'error: scraper runtime library not found: %s\n' "$soname" >&2
        exit 1
    }
    install -m 0755 "$source" "$SCRAPER_LIB_DIR/$soname"
    SCRAPER_RUNTIME_LIBS="$SCRAPER_RUNTIME_LIBS $soname"
    while IFS= read -r child; do
        [ -n "$child" ] || continue
        copy_scraper_runtime_library "$child"
    done < <(scraper_runtime_needed "$source")
}

install_scraper_runtime() {
    local curl_bin=/usr/bin/curl loader soname
    local doc_dir="$PLUMOS_DIR/share/doc/frontend"
    [ -x "$curl_bin" ] || {
        printf 'error: toolchain curl is unavailable\n' >&2
        exit 1
    }
    install -m 0755 "$curl_bin" "$SCRAPER_LIB_DIR/plumos-curl"
    while IFS= read -r soname; do
        [ -n "$soname" ] || continue
        copy_scraper_runtime_library "$soname"
    done < <(scraper_runtime_needed "$curl_bin")
    loader="$(readlink -f /lib/ld-linux-aarch64.so.1 2>/dev/null || true)"
    [ -n "$loader" ] && [ -f "$loader" ] || \
        loader="$(readlink -f /lib/aarch64-linux-gnu/ld-linux-aarch64.so.1)"
    install -m 0755 "$loader" "$SCRAPER_LIB_DIR/ld-linux-aarch64.so.1"
    install -m 0644 /etc/ssl/certs/ca-certificates.crt \
        "$SCRAPER_LIB_DIR/ca-certificates.crt"
    mkdir -p "$doc_dir"
    install -m 0644 /usr/share/doc/curl/copyright "$doc_dir/curl-copyright"
    [ ! -r /usr/share/doc/libcurl4/copyright ] || \
        install -m 0644 /usr/share/doc/libcurl4/copyright \
            "$doc_dir/libcurl-copyright"
    [ ! -r /usr/share/doc/ca-certificates/copyright ] || \
        install -m 0644 /usr/share/doc/ca-certificates/copyright \
            "$doc_dir/ca-certificates-copyright"
    cat >"$BIN_DIR/curl" <<'EOF'
#!/bin/sh
set -eu
PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/plumos}"
PLUMOS_SCRAPER_LIB_DIR="${PLUMOS_SCRAPER_LIB_DIR:-$PLUMOS_ROOT/scraper/lib}"
export CURL_CA_BUNDLE="${CURL_CA_BUNDLE:-$PLUMOS_SCRAPER_LIB_DIR/ca-certificates.crt}"
export SSL_CERT_FILE="${SSL_CERT_FILE:-$CURL_CA_BUNDLE}"
exec "$PLUMOS_SCRAPER_LIB_DIR/ld-linux-aarch64.so.1" \
    --library-path "$PLUMOS_SCRAPER_LIB_DIR" \
    "$PLUMOS_SCRAPER_LIB_DIR/plumos-curl" "$@"
EOF
    chmod 0755 "$BIN_DIR/curl"
}

install_scraper_runtime

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
    find bin config factory-defaults fonts frontend scraper share themes licenses/plumos-frontend \
        -type f -print | sort | while IFS= read -r file; do sha256sum "$file"; done
    sha256sum components/frontend/manifest.json
) >"$COMPONENT_DIR/checksums.sha256"
printf 'frontend_component=result-ok output=%s\n' "$PLUMOS_DIR"
