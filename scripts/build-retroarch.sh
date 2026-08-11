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
WORK="$ROOT_DIR/output/build/retroarch-pixel2"
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
(
    cd "$WORK"
    ./configure --prefix=/usr \
        --disable-x11 --disable-wayland --disable-sdl --disable-sdl2 \
        --disable-opengl --disable-opengl1 --disable-opengl_core \
        --enable-opengles --enable-egl --disable-vulkan \
        --enable-kms --enable-plain_drm --enable-alsa --enable-udev \
        --disable-pulse --disable-jack --disable-oss --disable-ffmpeg \
        --disable-networking
    make -j"${JOBS:-$(nproc)}"
)

rm -rf "$OUT_ROOT"
mkdir -p "$PLUMOS_DIR/bin" "$PLUMOS_DIR/emulator/lib" \
    "$PLUMOS_DIR/factory-defaults/retroarch/autoconfig/udev" \
    "$PLUMOS_DIR/licenses" "$COMPONENT_DIR"
install -m 0755 "$WORK/retroarch" "$PLUMOS_DIR/bin/retroarch"
strip "$PLUMOS_DIR/bin/retroarch" 2>/dev/null || true
install -m 0644 "$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg" \
    "$PLUMOS_DIR/factory-defaults/retroarch/retroarch.cfg"
install -m 0644 "$ROOT_DIR/package/retroarch-pixel2/gkd-pixel2-joypad.cfg" \
    "$PLUMOS_DIR/factory-defaults/retroarch/autoconfig/udev/gkd-pixel2-joypad.cfg"
install -m 0644 "$WORK/COPYING" "$PLUMOS_DIR/licenses/RetroArch-COPYING"

ldd "$PLUMOS_DIR/bin/retroarch" 2>/dev/null | awk '/=> \// {print $3} /^\// {print $1}' |
    sort -u | while IFS= read -r library; do
        [ -f "$library" ] || continue
        base=${library##*/}
        case "$base" in libc.so.*|libm.so.*|libdl.so.*|libpthread.so.*|librt.so.*|ld-linux-*.so.*) continue ;; esac
        install -m 0644 "$library" "$PLUMOS_DIR/emulator/lib/$base"
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
  "source_ref": "$SOURCE_REF",
  "source_date_epoch": $SOURCE_EPOCH,
  "video_driver": "plain_drm",
  "display_rotation": "ccw",
  "audio_driver": "alsa",
  "input_driver": "udev"
}
EOF
(
    cd "$PLUMOS_DIR"
    find bin emulator factory-defaults/retroarch licenses -type f -print | sort |
        while IFS= read -r file; do sha256sum "$file"; done
    sha256sum components/retroarch/manifest.json
) >"$COMPONENT_DIR/checksums.sha256"
file "$PLUMOS_DIR/bin/retroarch"
printf 'retroarch_component=result-ok output=%s\n' "$PLUMOS_DIR"
