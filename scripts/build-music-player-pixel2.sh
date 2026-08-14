#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${ROOT_DIR:-$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)}"
BUILD_ROOT="${BUILD_ROOT:-$ROOT_DIR/build/music-player-pixel2}"
TARGET_DIR="${TARGET_DIR:-$ROOT_DIR/output/music-player/pixel2}"
SOURCE_REF="${MUSIC_SOURCE_REF:-bc49dafe782173f35ab557035fa96ba81564038d}"
SOURCE_BASE="${MUSIC_SOURCE_BASE:-https://raw.githubusercontent.com/game-de-it/plumOS-V90S_V2/$SOURCE_REF}"
SOURCE_SHA256="930349bf23de57b2f0d04db1fe0b78e58301e551d292edf015fb742298f4fcd7"
RENDERER_SHA256="98d40e0dd437c2e6e7a94d751459af94579cc8ed0ae16065d2d4f89e7323e171"
MINIAUDIO_REF="9634bedb5b5a2ca38c1ee7108a9358a4e233f14d"
MINIAUDIO_SHA256="ac7af4de748b7e26b777f37e01cee313a308a7296a3eb080e2906b320cc55c89"
PATCH_FILE="$ROOT_DIR/docker/pixel2-tools/patches/music-player-pixel2.patch"
CC="${CC:-gcc}"
STRIP="${STRIP:-strip}"
READELF="${READELF:-readelf}"

fetch_verified() {
    local url="$1"
    local output="$2"
    local expected="$3"
    curl -LfsS "$url" -o "$output"
    printf '%s  %s\n' "$expected" "$output" | sha256sum -c -
}

find_target_lib() {
    local name="$1"
    local dir
    for dir in /lib/aarch64-linux-gnu /usr/lib/aarch64-linux-gnu /lib /usr/lib; do
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
                ld-linux-aarch64.so.1|libc.so.6|libm.so.6|libpthread.so.0|libdl.so.2|librt.so.1|libgcc_s.so.1)
                    continue
                    ;;
            esac
            [ -e "$destination/$dependency" ] && continue
            source="$(find_target_lib "$dependency" || true)"
            [ -n "$source" ] || {
                printf 'error: runtime dependency not found: %s\n' "$dependency" >&2
                exit 1
            }
            real_name="$(basename "$source")"
            install -m 0644 "$source" "$destination/$real_name"
            if [ "$real_name" != "$dependency" ]; then
                cp -f "$destination/$real_name" "$destination/$dependency"
            fi
            copy_dependency_tree "$source" "$destination"
        done
}

rm -rf "$BUILD_ROOT" "$TARGET_DIR"
mkdir -p "$BUILD_ROOT/apps" "$BUILD_ROOT/frontend" "$BUILD_ROOT/include" \
    "$TARGET_DIR/plumos/apps/music-player/bin" \
    "$TARGET_DIR/plumos/apps/music-player/lib" \
    "$TARGET_DIR/plumos/bin" \
    "$TARGET_DIR/plumos/components/music-player" \
    "$TARGET_DIR/plumos/share/doc/music-player"

fetch_verified "$SOURCE_BASE/src/apps/plumos_music_player.c" \
    "$BUILD_ROOT/apps/plumos_music_player.c" "$SOURCE_SHA256"
fetch_verified "$SOURCE_BASE/src/apps/plumos_music_v90s_renderer.h" \
    "$BUILD_ROOT/apps/plumos_music_pixel2_renderer.h" "$RENDERER_SHA256"
fetch_verified \
    "https://raw.githubusercontent.com/mackron/miniaudio/$MINIAUDIO_REF/miniaudio.h" \
    "$BUILD_ROOT/include/miniaudio.h" "$MINIAUDIO_SHA256"
cp "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h" \
    "$BUILD_ROOT/frontend/plumos_fbdev_renderer.h"
sed -i \
    -e 's/plumos_music_v90s_renderer/plumos_music_pixel2_renderer/g' \
    -e 's/input_name_matches_v90s_gamepad/input_name_matches_legacy_gamepad/g' \
    "$BUILD_ROOT/apps/plumos_music_player.c"
patch -d "$BUILD_ROOT/apps" -p1 <"$PATCH_FILE"

APP_ROOT="$TARGET_DIR/plumos/apps/music-player"
"$CC" -std=c11 -O2 -pipe \
    -DPLUMOS_FBDEV_ENABLE_FREETYPE=1 \
    -DPLUMOS_FBDEV_ENABLE_PNG=1 \
    -DPLUMOS_FBDEV_ENABLE_DRM=1 \
    -DPLUMOS_MUSIC_ENABLE_ALSA=1 \
    -I"$BUILD_ROOT/include" \
    -I"$BUILD_ROOT/apps" \
    -I"$BUILD_ROOT/frontend" \
    $(pkg-config --cflags freetype2 libpng alsa libdrm) \
    -o "$APP_ROOT/bin/plumos-music-player.bin" \
    "$BUILD_ROOT/apps/plumos_music_player.c" \
    -lasound -ldl -lfreetype -lpng -ljpeg -lz -ldrm -lm -lpthread
"$STRIP" "$APP_ROOT/bin/plumos-music-player.bin" 2>/dev/null || true
copy_dependency_tree "$APP_ROOT/bin/plumos-music-player.bin" "$APP_ROOT/lib"

cat >"$TARGET_DIR/plumos/bin/plumos-music-player-launch" <<'EOF'
#!/bin/sh
set -u

PLUMOS_ROOT="${PLUMOS_ROOT:-/mnt/plumos}"
BB="${PLUMOS_BUSYBOX:-/mnt/plumos-user/.tmp_update/busybox}"
APP_ROOT="${PLUMOS_MUSIC_PLAYER_ROOT:-$PLUMOS_ROOT/apps/music-player}"
STATE_DIR="${PLUMOS_MUSIC_PLAYER_STATE:-$PLUMOS_ROOT/state/apps/music-player}"
LOG_DIR="${PLUMOS_MUSIC_PLAYER_LOG_DIR:-$PLUMOS_ROOT/logs/apps}"
mkdir -p "$STATE_DIR" "$LOG_DIR" 2>/dev/null || true
[ -x "$BB" ] || BB=/bin/busybox

[ -x "$APP_ROOT/bin/plumos-music-player.bin" ] || {
    printf 'error: missing Music Player binary: %s\n' \
        "$APP_ROOT/bin/plumos-music-player.bin" >&2
    exit 127
}

AUDIO_OUTPUT="$PLUMOS_ROOT/bin/plumos-audio-output"
[ -x "$AUDIO_OUTPUT" ] || {
    printf 'plumos-music-player-launch: audio router is missing\n' >&2
    exit 1
}
"$BB" sh "$AUDIO_OUTPUT" prepare \
    >>"$LOG_DIR/music-player.log" 2>&1 || {
    printf 'plumos-music-player-launch: audio route preparation failed\n' >&2
    exit 1
}
if [ -x "$PLUMOS_ROOT/bin/plumos-volume-control" ]; then
    "$BB" sh "$PLUMOS_ROOT/bin/plumos-volume-control" apply \
        >>"$LOG_DIR/music-player.log" 2>&1 || true
fi
export HOME="$STATE_DIR"
export XDG_CONFIG_HOME="$STATE_DIR/.config"
export PLUMOS_ROOT
export PLUMOS_MUSIC_FONT="${PLUMOS_MUSIC_FONT:-$PLUMOS_ROOT/fonts/default.otf}"
export PLUMOS_MUSIC_FALLBACK_FONT="${PLUMOS_MUSIC_FALLBACK_FONT:-$PLUMOS_ROOT/fonts/cjk-fallback.ttc}"
export ALSA_CONFIG_PATH="${ALSA_CONFIG_PATH:-/run/plumos/audio/asound.conf}"
export ALSA_PLUGIN_DIR="${ALSA_PLUGIN_DIR:-$PLUMOS_ROOT/lib/alsa-lib}"
export PLUMOS_MUSIC_ALSA_DEVICE="${PLUMOS_MUSIC_ALSA_DEVICE:-plumos_output}"
export PLUMOS_MUSIC_IGNORE_ANALOG="${PLUMOS_MUSIC_IGNORE_ANALOG:-1}"
export PLUMOS_DRM_DEVICE="${PLUMOS_DRM_DEVICE:-/dev/dri/card0}"
export PLUMOS_MUSIC_ROTATION="${PLUMOS_MUSIC_ROTATION:-ccw}"
# Do not inherit service-specific glibc paths (notably from adbd).  This app
# owns its non-system dependencies and must otherwise use the Pixel2 system libc.
export LD_LIBRARY_PATH="$APP_ROOT/lib"
cd "$APP_ROOT" || exit 1
exec "$APP_ROOT/bin/plumos-music-player.bin" >>"$LOG_DIR/music-player.log" 2>&1
EOF
chmod 0755 "$TARGET_DIR/plumos/bin/plumos-music-player-launch"

generated_at="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
cat >"$TARGET_DIR/plumos/components/music-player/manifest.json" <<EOF
{
  "name": "plumOS Music Player for Pixel2",
  "component": "music-player",
  "device": "pixel2",
  "architecture": "aarch64",
  "source_ref": "$SOURCE_REF",
  "source_sha256": "$SOURCE_SHA256",
  "miniaudio_ref": "$MINIAUDIO_REF",
  "generated_at": "$generated_at",
  "display": "DRM page flip 640x480",
  "audio": "ALSA plumos_output",
  "input": "plumOS Pixel2 Controller"
}
EOF
cat >"$TARGET_DIR/plumos/share/doc/music-player/README.txt" <<EOF
plumOS Music Player for Pixel2
source=$SOURCE_BASE/src/apps/plumos_music_player.c
source_ref=$SOURCE_REF
source_sha256=$SOURCE_SHA256
decoder=miniaudio mp3/flac/wav
display=Pixel2 DRM page flip
audio=ALSA plumos_output
music_roots=/mnt/plumos-user/music,/mnt/plumos-user/roms/music
controls=D-pad select/seek, A play, B/Function exit, X/Y track, Select EQ, L/R volume
EOF
(
    cd "$TARGET_DIR/plumos"
    find apps/music-player bin/plumos-music-player-launch \
        share/doc/music-player components/music-player/manifest.json \
        -type f -print | sort | while IFS= read -r path; do sha256sum "$path"; done
) >"$TARGET_DIR/plumos/components/music-player/checksums.sha256"
(
    cd "$TARGET_DIR/plumos"
    sha256sum -c components/music-player/checksums.sha256
)
printf 'created: %s\n' "$TARGET_DIR"
