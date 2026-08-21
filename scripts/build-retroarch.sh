#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if [ "${1:-}" != --inside ]; then
    exec "$ROOT_DIR/scripts/docker-build.sh" retroarch "$@"
fi
shift

ROOT_DIR=/work
SOURCE_URL=https://github.com/libretro/RetroArch.git
SOURCE_COMMIT=69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576
ASSETS_SOURCE_URL=https://github.com/libretro/retroarch-assets.git
ASSETS_SOURCE_COMMIT=73106363e14e34c08a5854b4cfbc29f184e3b783
WORK="$ROOT_DIR/output/build/retroarch-pixel2"
ASSETS_WORK="$ROOT_DIR/output/build/retroarch-assets-pixel2"
OUT_ROOT="$ROOT_DIR/output/retroarch/pixel2"
PLUMOS_DIR="$OUT_ROOT/plumos"
COMPONENT_DIR="$PLUMOS_DIR/components/retroarch"
VERSION="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}"
SOURCE_REF="$(git -C "$ROOT_DIR" rev-parse --short HEAD)"
SOURCE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[ -n "$SOURCE_EPOCH" ] || SOURCE_EPOCH="$(git -C "$ROOT_DIR" show -s --format=%ct HEAD)"
export SOURCE_DATE_EPOCH="$SOURCE_EPOCH"

if [ ! -d "$WORK/.git" ]; then
    rm -rf "$WORK"
    git clone "$SOURCE_URL" "$WORK"
fi
git -C "$WORK" fetch --quiet origin "$SOURCE_COMMIT"
git -C "$WORK" checkout --quiet --detach "$SOURCE_COMMIT"
git -C "$WORK" reset --hard --quiet "$SOURCE_COMMIT"
git -C "$WORK" clean -fdx --quiet
for patch_file in "$ROOT_DIR"/patches/retroarch/*.patch; do
    git -C "$WORK" apply "$patch_file"
done
if [ ! -d "$ASSETS_WORK/.git" ]; then
    rm -rf "$ASSETS_WORK"
    git clone --filter=blob:none "$ASSETS_SOURCE_URL" "$ASSETS_WORK"
fi
git -C "$ASSETS_WORK" fetch --quiet origin "$ASSETS_SOURCE_COMMIT"
git -C "$ASSETS_WORK" checkout --quiet --detach "$ASSETS_SOURCE_COMMIT"
git -C "$ASSETS_WORK" reset --hard --quiet "$ASSETS_SOURCE_COMMIT"
git -C "$ASSETS_WORK" clean -fdx --quiet
(
    cd "$WORK"
    ./configure --prefix=/usr \
        --disable-x11 --disable-wayland --disable-sdl --disable-sdl2 \
        --disable-opengl --disable-opengl1 --disable-opengl_core \
        --enable-opengles --enable-egl --disable-vulkan \
        --enable-kms --enable-plain_drm --enable-alsa --enable-udev \
        --enable-rgui --enable-materialui --enable-ozone --enable-xmb \
        --enable-freetype \
        --disable-pulse --disable-jack --disable-oss --disable-ffmpeg \
        --disable-networking
    make -j"${JOBS:-$(nproc)}"
    for menu_driver in RGUI MATERIALUI OZONE XMB; do
        grep -Eq "^HAVE_${menu_driver} = 1$" config.mk || {
            printf 'error: RetroArch menu driver was not built: %s\n' \
                "$menu_driver" >&2
            exit 1
        }
    done
    grep -Eq '^HAVE_LANGEXTRA = 1$' config.mk || {
        printf 'error: RetroArch multi-language support was not built\n' >&2
        exit 1
    }
    nm retroarch >menu-symbols.txt
    for menu_symbol in menu_ctx_rgui menu_ctx_mui menu_ctx_ozone menu_ctx_xmb; do
        grep -Eq "[[:space:]]${menu_symbol}$" menu-symbols.txt || {
            printf 'error: RetroArch menu symbol is missing: %s\n' \
                "$menu_symbol" >&2
            exit 1
        }
    done
    rm -f menu-symbols.txt
)

rm -rf "$OUT_ROOT"
mkdir -p "$PLUMOS_DIR/bin" "$PLUMOS_DIR/emulator/lib" \
    "$PLUMOS_DIR/emulator/dri" "$PLUMOS_DIR/emulator/egl_vendor.d" \
    "$PLUMOS_DIR/retroarch/assets" \
    "$PLUMOS_DIR/factory-defaults/retroarch/autoconfig/udev" \
    "$PLUMOS_DIR/factory-defaults/retroarch/remaps/ParaLLEl N64" \
    "$PLUMOS_DIR/factory-defaults/alsa" \
    "$PLUMOS_DIR/licenses" "$COMPONENT_DIR"
install -m 0755 "$WORK/retroarch" "$PLUMOS_DIR/bin/retroarch"
strip "$PLUMOS_DIR/bin/retroarch" 2>/dev/null || true
install -m 0644 "$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg" \
    "$PLUMOS_DIR/factory-defaults/retroarch/retroarch.cfg"
install -m 0644 "$ROOT_DIR/package/retroarch-pixel2/retroarch-core-options.cfg" \
    "$PLUMOS_DIR/factory-defaults/retroarch/retroarch-core-options.cfg"
install -m 0644 \
    "$ROOT_DIR/package/retroarch-pixel2/remaps/ParaLLEl N64/ParaLLEl N64.rmp" \
    "$PLUMOS_DIR/factory-defaults/retroarch/remaps/ParaLLEl N64/ParaLLEl N64.rmp"
install -m 0644 "$ROOT_DIR/package/retroarch-pixel2/pixel2-joypad-udev.cfg" \
    "$PLUMOS_DIR/factory-defaults/retroarch/autoconfig/udev/pixel2_joypad.cfg"
install -m 0644 "$ROOT_DIR/package/retroarch-pixel2/alsa.conf" \
    "$PLUMOS_DIR/factory-defaults/alsa/alsa.conf"
install -m 0644 "$WORK/COPYING" "$PLUMOS_DIR/licenses/RetroArch-COPYING"

# Graphical menu drivers are present in the binary even without their media,
# but selecting one then yields a blank or incomplete menu. Keep these assets
# app-layer managed and immutable; mutable RetroArch configuration merely
# points at this directory. pkg contains the language-specific fallback fonts
# used by Ozone and MaterialUI.
for asset_tree in fonts glui ozone pkg rgui sounds xmb; do
    [ -d "$ASSETS_WORK/$asset_tree" ] || {
        printf 'error: RetroArch assets tree is missing: %s\n' "$asset_tree" >&2
        exit 1
    }
    cp -a "$ASSETS_WORK/$asset_tree" "$PLUMOS_DIR/retroarch/assets/"
done
install -m 0644 "$ASSETS_WORK/COPYING" \
    "$PLUMOS_DIR/licenses/RetroArch-Assets-COPYING"
install -m 0644 "$ASSETS_WORK/README.md" \
    "$PLUMOS_DIR/licenses/RetroArch-Assets-README.md"

copy_runtime_tree() {
    local object="$1" library base
    ldd "$object" 2>/dev/null | awk '/=> \// {print $3} /^\// {print $1}' |
        sort -u | while IFS= read -r library; do
            [ -f "$library" ] || continue
            base=${library##*/}
            # These are provided by the stock Pixel2 System ABI. Everything
            # else belongs to this managed graphics/emulator runtime.
            case "$base" in libc.so.*|libm.so.*|ld-linux-*.so.*) continue ;; esac
            if [ ! -f "$PLUMOS_DIR/emulator/lib/$base" ]; then
                install -m 0644 "$library" "$PLUMOS_DIR/emulator/lib/$base"
                copy_runtime_tree "$library"
            fi
        done
}

copy_runtime_tree "$PLUMOS_DIR/bin/retroarch"

# The stock boot substrate exposes Rockchip DRM but intentionally carries no
# Mesa userspace. Libretro GLES cores, PPSSPP and Pyxel therefore share the
# app-layer-owned Mesa runtime instead of modifying the stock System.
MESA_DRI=/usr/lib/aarch64-linux-gnu/dri/rockchip_dri.so
MESA_EGL=/usr/lib/aarch64-linux-gnu/libEGL_mesa.so.0
MESA_EGL_JSON=/usr/share/glvnd/egl_vendor.d/50_mesa.json
for required in "$MESA_DRI" "$MESA_EGL" "$MESA_EGL_JSON"; do
    [ -r "$required" ] || {
        printf 'error: Mesa Pixel2 runtime input is missing: %s\n' "$required" >&2
        exit 1
    }
done
install -m 0644 "$MESA_DRI" "$PLUMOS_DIR/emulator/dri/rockchip_dri.so"
install -m 0644 "$(readlink -f "$MESA_EGL")" \
    "$PLUMOS_DIR/emulator/lib/libEGL_mesa.so.0"
install -m 0644 "$MESA_EGL_JSON" \
    "$PLUMOS_DIR/emulator/egl_vendor.d/50_mesa.json"
copy_runtime_tree "$MESA_DRI"
copy_runtime_tree "$(readlink -f "$MESA_EGL")"

for required_library in libpthread.so.0; do
    [ -f "$PLUMOS_DIR/emulator/lib/$required_library" ] || {
        printf 'error: RetroArch runtime library was not bundled: %s\n' \
            "$required_library" >&2
        exit 1
    }
done

cat >"$COMPONENT_DIR/manifest.json" <<EOF
{
  "name": "RetroArch for plumOS Pixel2",
  "component": "retroarch",
  "device": "pixel2",
  "architecture": "aarch64",
  "version": "$VERSION",
  "upstream": "$SOURCE_URL",
  "upstream_commit": "$SOURCE_COMMIT",
  "assets_upstream": "$ASSETS_SOURCE_URL",
  "assets_upstream_commit": "$ASSETS_SOURCE_COMMIT",
  "source_ref": "$SOURCE_REF",
  "source_date_epoch": $SOURCE_EPOCH,
  "video_driver": "plain_drm",
  "graphics_runtime": "app-layer-mesa-rockchip-dri",
  "display_rotation": "ccw",
  "audio_driver": "alsa",
  "input_driver": "udev",
  "menu_drivers": ["rgui", "glui", "ozone", "xmb"],
  "multi_language": true
}
EOF
(
    cd "$PLUMOS_DIR"
    find bin emulator factory-defaults licenses retroarch -type f -print | sort |
        while IFS= read -r file; do sha256sum "$file"; done
    sha256sum components/retroarch/manifest.json
) >"$COMPONENT_DIR/checksums.sha256"
file "$PLUMOS_DIR/bin/retroarch"
printf 'retroarch_component=result-ok output=%s\n' "$PLUMOS_DIR"
