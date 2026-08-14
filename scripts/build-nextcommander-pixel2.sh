#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${ROOT_DIR:-$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)}"
BUILD_ROOT="${BUILD_ROOT:-$ROOT_DIR/build}"
TARGET_DIR="${TARGET_DIR:-$ROOT_DIR/output/nextcommander/pixel2}"
SRC_ROOT="${SRC_ROOT:-$BUILD_ROOT/nextcommander-pixel2}"
PATCH_FILE="$ROOT_DIR/docker/pixel2-tools/patches/nextcommander-pixel2.patch"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"
NEXTCOMMANDER_REPO="${NEXTCOMMANDER_REPO:-https://github.com/LoveRetro/NextCommander.git}"
NEXTCOMMANDER_REF="${NEXTCOMMANDER_REF:-49c24bb67c12aea8078f48c833815f9ef2dcc5e2}"
CC="${CC:-gcc}"
CXX="${CXX:-g++}"
STRIP="${STRIP:-strip}"
READELF="${READELF:-readelf}"

find_target_lib() {
    local name="$1"
    local dir
    for dir in \
        /lib/aarch64-linux-gnu \
        /usr/lib/aarch64-linux-gnu \
        /usr/lib/aarch64-linux-gnu/pulseaudio \
        /lib \
        /usr/lib; do
        if [ -e "$dir/$name" ]; then
            readlink -f "$dir/$name"
            return 0
        fi
    done
    return 1
}

copy_dependency_tree() {
    local elf="$1"
    local destination="$2"
    local dependency source real_name
    "$READELF" -d "$elf" 2>/dev/null |
        awk -F'[][]' '/NEEDED/ { print $2 }' |
        while IFS= read -r dependency; do
            case "$dependency" in
                ld-linux-aarch64.so.1|libc.so.6|libm.so.6|libpthread.so.0|libdl.so.2|librt.so.1|libgcc_s.so.1|libstdc++.so.6)
                    continue
                    ;;
            esac
            [ -e "$destination/$dependency" ] && continue
            source="$(find_target_lib "$dependency" || true)"
            [ -n "$source" ] || {
                printf 'warning: runtime dependency not found: %s\n' "$dependency" >&2
                continue
            }
            real_name="$(basename "$source")"
            install -m 0644 "$source" "$destination/$real_name"
            if [ "$real_name" != "$dependency" ]; then
                cp -f "$destination/$real_name" "$destination/$dependency"
            fi
            copy_dependency_tree "$source" "$destination"
        done
}

rm -rf "$TARGET_DIR" "$SRC_ROOT"
mkdir -p "$TARGET_DIR/plumos/apps/nextcommander/bin" \
    "$TARGET_DIR/plumos/apps/nextcommander/lib" \
    "$TARGET_DIR/plumos/apps/nextcommander/config" \
    "$TARGET_DIR/plumos/bin" \
    "$TARGET_DIR/plumos/components/nextcommander" \
    "$TARGET_DIR/plumos/share/doc/nextcommander" \
    "$SRC_ROOT"

git clone --filter=blob:none --no-checkout "$NEXTCOMMANDER_REPO" "$SRC_ROOT/source"
git -C "$SRC_ROOT/source" fetch --depth 1 origin "$NEXTCOMMANDER_REF"
git -C "$SRC_ROOT/source" checkout --detach FETCH_HEAD
patch -d "$SRC_ROOT/source" -p1 <"$PATCH_FILE"
install -m 0644 "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h" \
    "$SRC_ROOT/source/src/plumos_fbdev_renderer.h"
make -C "$SRC_ROOT/source" clean >/dev/null 2>&1 || true
make -C "$SRC_ROOT/source" -j"$JOBS" PLATFORM=pixel2 PREFIX=/usr CC="$CC" CXX="$CXX"

APP_ROOT="$TARGET_DIR/plumos/apps/nextcommander"
install -m 0755 "$SRC_ROOT/source/output/NextCommander" "$APP_ROOT/bin/NextCommander"
"$STRIP" "$APP_ROOT/bin/NextCommander" 2>/dev/null || true
cp -a "$SRC_ROOT/source/res" "$APP_ROOT/"
install -m 0644 /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc \
    "$APP_ROOT/res/font1.ttf"
copy_dependency_tree "$APP_ROOT/bin/NextCommander" "$APP_ROOT/lib"
for library in libstdc++.so.6 libgcc_s.so.1; do
    source="$(find_target_lib "$library" || true)"
    [ -n "$source" ] || {
        printf 'error: NextCommander C++ runtime is missing: %s\n' \
            "$library" >&2
        exit 1
    }
    install -m 0644 "$source" "$APP_ROOT/lib/$library"
    copy_dependency_tree "$source" "$APP_ROOT/lib"
done

cat >"$APP_ROOT/config/pixel2.cfg" <<'EOF'
disp_width=640
disp_height=480
disp_bpp=32
disp_ppu_x=2
disp_ppu_y=2
disp_autoscale=false
disp_autoscale_dpi=false
path_default=/mnt/plumos-user
path_default_right=/mnt/plumos-user/roms
path_default_right_fallback=/mnt/plumos-user/roms
res_dir=/mnt/plumos/apps/nextcommander/res
EOF

cat >"$TARGET_DIR/plumos/bin/plumos-nextcommander-launch" <<'EOF'
#!/bin/sh
set -u

PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/plumos}"
APP_ROOT="${PLUMOS_NEXTCOMMANDER_ROOT:-$PLUMOS_ROOT/apps/nextcommander}"
STATE_DIR="${PLUMOS_NEXTCOMMANDER_STATE:-$PLUMOS_ROOT/state/apps/nextcommander}"
LOG_DIR="${PLUMOS_NEXTCOMMANDER_LOG_DIR:-$PLUMOS_ROOT/logs/apps}"
mkdir -p "$STATE_DIR/.cache" "$STATE_DIR/.config" "$LOG_DIR" 2>/dev/null || true

[ -x "$APP_ROOT/bin/NextCommander" ] || {
    printf 'error: missing NextCommander binary: %s\n' "$APP_ROOT/bin/NextCommander" >&2
    exit 127
}

export HOME="$STATE_DIR"
export XDG_CACHE_HOME="$STATE_DIR/.cache"
export XDG_CONFIG_HOME="$STATE_DIR/.config"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"
export SDL_GAMECONTROLLERCONFIG="${SDL_GAMECONTROLLERCONFIG:-}"
export SDL_NOMOUSE=1
export PLUMOS_DRM_DEVICE="${PLUMOS_DRM_DEVICE:-/dev/dri/card0}"
export LD_LIBRARY_PATH="$APP_ROOT/lib"
cd "$APP_ROOT" || exit 1
exec "$APP_ROOT/bin/NextCommander" \
    --config "$APP_ROOT/config/pixel2.cfg" \
    --res-dir "$APP_ROOT/res" >>"$LOG_DIR/nextcommander.log" 2>&1
EOF
chmod 0755 "$TARGET_DIR/plumos/bin/plumos-nextcommander-launch"

generated_at="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
cat >"$TARGET_DIR/plumos/components/nextcommander/manifest.json" <<EOF
{
  "name": "NextCommander for plumOS Pixel2",
  "component": "nextcommander",
  "device": "pixel2",
  "architecture": "aarch64",
  "upstream": "$NEXTCOMMANDER_REPO",
  "upstream_ref": "$NEXTCOMMANDER_REF",
  "generated_at": "$generated_at",
  "display": "native Pixel2 DRM page flip 640x480",
  "input": "SDL2 joystick via plumOS Pixel2 Controller"
}
EOF
cat >"$TARGET_DIR/plumos/share/doc/nextcommander/README.txt" <<EOF
NextCommander for plumOS Pixel2
upstream=$NEXTCOMMANDER_REPO
upstream_ref=$NEXTCOMMANDER_REF
patch=nextcommander-pixel2.patch
EOF
(
    cd "$TARGET_DIR/plumos"
    find apps/nextcommander bin/plumos-nextcommander-launch \
        share/doc/nextcommander components/nextcommander/manifest.json \
        -type f -print | sort | while IFS= read -r path; do sha256sum "$path"; done
) >"$TARGET_DIR/plumos/components/nextcommander/checksums.sha256"
(
    cd "$TARGET_DIR/plumos"
    sha256sum -c components/nextcommander/checksums.sha256
)
printf 'created: %s\n' "$TARGET_DIR"
