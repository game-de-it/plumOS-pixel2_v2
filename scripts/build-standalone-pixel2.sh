#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if [ "${1:-}" != --inside ]; then
    exec "$ROOT_DIR/scripts/docker-build.sh" standalone "$@"
fi
shift

ROOT_DIR=/work
OUT_ROOT="${PLUMOS_PIXEL2_STANDALONE_OUT:-$ROOT_DIR/output/standalone/pixel2}"
PLUMOS_DIR="$OUT_ROOT/plumos"
COMPONENT_DIR="$PLUMOS_DIR/components/standalone"
PACKAGE_ROOT="$ROOT_DIR/package/standalone-pixel2/plumos"
BUILD_ROOT="${PLUMOS_PIXEL2_STANDALONE_BUILD:-$ROOT_DIR/output/build/standalone-pixel2}"
SRC_ROOT="$BUILD_ROOT/src"
LOG_DIR="$OUT_ROOT/logs"
PATCH_DIR="$ROOT_DIR/patches"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"
STRIP="${STRIP:-strip}"
ARMHF_STRIP="${PLUMOS_PIXEL2_ARMHF_STRIP:-arm-linux-gnueabihf-strip}"
FILTER=all
OPENBOR_REPO="${PLUMOS_PIXEL2_OPENBOR_REPO:-https://github.com/DCurrent/openbor.git}"
OPENBOR_REF="${PLUMOS_PIXEL2_OPENBOR_REF:-494708eb34e71d1afda237873907701c4ec3a569}"
DRASTIC_REPO="${PLUMOS_PIXEL2_DRASTIC_REPO:-https://github.com/steward-fu/nds.git}"
DRASTIC_REF="${PLUMOS_PIXEL2_DRASTIC_REF:-b88e6b75963106c0bd54dfe112f860c6bdbfe593}"
DRASTIC_TAG="${PLUMOS_PIXEL2_DRASTIC_TAG:-final-china-devices}"
DRASTIC_RELEASE_URL="${PLUMOS_PIXEL2_DRASTIC_RELEASE_URL:-https://github.com/steward-fu/nds/releases/download/final-china-devices/drastic_miyoo-flip_20251104.zip}"
DRASTIC_RELEASE_SHA256="${PLUMOS_PIXEL2_DRASTIC_RELEASE_SHA256:-9e4ed98047dea0f014daea7c3530793f92f19d60073fceb9fd2a040696f66491}"
DRASTIC_ARCHIVE="${PLUMOS_PIXEL2_DRASTIC_ARCHIVE:-$ROOT_DIR/output/downloads/drastic_miyoo-flip_20251104.zip}"
DRASTIC_PATCH="$PATCH_DIR/drastic/steward-fu-nds-pixel2-toolchain.patch"
DRASTIC_MMAP_COMPAT="$ROOT_DIR/package/standalone-pixel2/src/drastic-mmap-compat.c"
PCSX_REPO="${PLUMOS_PIXEL2_PCSX_REPO:-https://github.com/notaz/pcsx_rearmed.git}"
PCSX_REF="${PLUMOS_PIXEL2_PCSX_REF:-9f8b6f248e073f03c530efda7c4cc60a7e2ecafc}"
PCSX_PICOFE_REF="${PLUMOS_PIXEL2_PCSX_PICOFE_REF:-dd11f2d723162eb1cf8e6db9f40de7db0d0b6bba}"
PCSX_SDL12_REPO="${PLUMOS_PIXEL2_PCSX_SDL12_REPO:-https://github.com/libsdl-org/sdl12-compat.git}"
PCSX_SDL12_REF="${PLUMOS_PIXEL2_PCSX_SDL12_REF:-fc2ec0c128197f1f5050e48359bc41e618f3abfb}"
PCSX_INPUT_AUDIO_PATCH="$PATCH_DIR/pcsx_rearmed/pcsx-rearmed-r26l-pixel2-input-audio.patch"
PCSX_PLATFORM_PATCH="$PATCH_DIR/pcsx_rearmed/pcsx-rearmed-r26l-pixel2-platform.patch"
PCSX_EVDEV_MENU_PATCH="$PATCH_DIR/pcsx_rearmed/pcsx-rearmed-r26l-pixel2-evdev-menu.patch"
PCSX_PICOFE_PATCH="$PATCH_DIR/pcsx_rearmed/libpicofe-r26l-pixel2-input.patch"
PCSX_FBDEV_HEADER="$ROOT_DIR/package/standalone-pixel2/src/pcsx-pixel2-fbdev.h"
PPSSPP_REPO="${PLUMOS_PIXEL2_PPSSPP_REPO:-https://github.com/hrydgard/ppsspp.git}"
PPSSPP_REF="${PLUMOS_PIXEL2_PPSSPP_REF:-v1.20.4}"
PPSSPP_COMMIT="${PLUMOS_PIXEL2_PPSSPP_COMMIT:-fa50bb1976065c4f8b1b47af227d367fe9771555}"
PPSSPP_PATCH="$PATCH_DIR/ppsspp/ppsspp-1.20.4-pixel2-no-sdl2-ttf.patch"
COMMON_CFLAGS="${PLUMOS_PIXEL2_STANDALONE_CFLAGS:--O2 -pipe -march=armv8-a+crc -mtune=cortex-a35 -fomit-frame-pointer -fcommon}"
VERSION="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}"
PROJECT_SOURCE_REF="$(git -C "$ROOT_DIR" rev-parse --short HEAD)"
SOURCE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[ -n "$SOURCE_EPOCH" ] || SOURCE_EPOCH="$(git -C "$ROOT_DIR" show -s --format=%ct HEAD)"
export SOURCE_DATE_EPOCH="$SOURCE_EPOCH"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --filter) FILTER=${2:-}; shift 2 ;;
    --filter=*) FILTER=${1#--filter=}; shift ;;
    *) printf 'error: unknown standalone option: %s\n' "$1" >&2; exit 2 ;;
  esac
done

selected() {
  id=$1
  case "$FILTER" in
    all|ALL) return 0 ;;
  esac
  case ",$FILTER," in
    *,"$id",*) return 0 ;;
    *) return 1 ;;
  esac
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || {
    printf 'error: required command is missing: %s\n' "$1" >&2
    exit 1
  }
}

sha256_file() {
  sha256sum "$1" | awk '{ print $1 }'
}

verify_sha256() {
  expected=$1
  path=$2
  actual=$(sha256_file "$path")
  [ "$actual" = "$expected" ] || {
    printf 'error: unexpected SHA-256 for %s\nexpected: %s\nactual:   %s\n' \
      "$path" "$expected" "$actual" >&2
    exit 1
  }
}

clone_checkout() {
  id=$1
  repo=$2
  ref=$3
  dst="$SRC_ROOT/$id"
  log="$LOG_DIR/$id.log"
  mkdir -p "$SRC_ROOT" "$LOG_DIR"
  if [ ! -d "$dst/.git" ]; then
    [ ! -e "$dst" ] || {
      printf 'error: source path is not a Git clone: %s\n' "$dst" >&2
      return 1
    }
    git clone --filter=blob:none "$repo" "$dst" >>"$log" 2>&1 || return 1
  fi
  git -C "$dst" fetch --depth 1 origin "$ref" >>"$log" 2>&1 || return 1
  git -C "$dst" checkout --detach FETCH_HEAD >>"$log" 2>&1 || return 1
  git -C "$dst" reset --hard FETCH_HEAD >>"$log" 2>&1 || return 1
  git -C "$dst" clean -fdx >>"$log" 2>&1 || return 1
  printf '%s\n' "$dst"
}

copy_overlay_file() {
  name=$1
  destination=$2
  source=$(find "$DRASTIC_OVERLAY_DIR" -type f -name "$name" -print -quit 2>/dev/null)
  [ -n "$source" ] || {
    printf 'error: private DraStic runtime file is missing: %s\n' "$name" >&2
    exit 1
  }
  install -m 0755 "$source" "$destination"
}

extract_zip() {
  archive=$1
  destination=$2
  python3 - "$archive" "$destination" <<'PY'
from pathlib import Path
import sys
import zipfile

archive = Path(sys.argv[1])
destination = Path(sys.argv[2])
with zipfile.ZipFile(archive) as handle:
    handle.extractall(destination)
PY
}

copy_runtime_deps() {
  elf=$1
  ldd "$elf" 2>/dev/null |
    awk '/=> \/[^ ]+/ {print $3} /^[[:space:]]*\// {print $1}' |
    sort -u |
    while IFS= read -r dep; do
      [ -f "$dep" ] || continue
      soname=$(basename "$dep")
      case "$soname" in
        libSDL-1.2.so.*)
          # PCSX-ReARMed uses its pinned package-local sdl12-compat build.
          continue
          ;;
        ld-linux-aarch64.so.1|libc.so.6|libm.so.6|libpthread.so.0|libdl.so.2|librt.so.1)
          continue
          ;;
      esac
      real=$(readlink -f "$dep")
      real_name=$(basename "$real")
      mkdir -p "$PLUMOS_DIR/lib"
      if [ ! -f "$PLUMOS_DIR/lib/$real_name" ]; then
        install -m 0644 "$real" "$PLUMOS_DIR/lib/$real_name"
        copy_runtime_deps "$real"
      fi
      if [ "$soname" != "$real_name" ]; then
        map_line=$(printf '%s\t%s' "$soname" "$real_name")
        grep -Fqx "$map_line" "$PLUMOS_DIR/config/standalone/soname-links.tsv" ||
          printf '%s\n' "$map_line" >>"$PLUMOS_DIR/config/standalone/soname-links.tsv"
      fi
    done
}

build_openbor() {
  selected openbor || return 0
  mkdir -p "$LOG_DIR" "$PLUMOS_DIR/standalone/openbor/bin" \
    "$PLUMOS_DIR/config/standalone" "$PLUMOS_DIR/licenses"
  : >"$LOG_DIR/openbor.log"
  src=$(clone_checkout openbor "$OPENBOR_REPO" "$OPENBOR_REF") || return 1
  patch_file="$PATCH_DIR/openbor/openbor-v6391-pixel2-sdl.patch"
  git -C "$src" apply --check "$patch_file" >>"$LOG_DIR/openbor.log" 2>&1
  git -C "$src" apply "$patch_file" >>"$LOG_DIR/openbor.log" 2>&1
  (
    cd "$src/engine" || exit 1
    make clean BUILD_LINUX=1 >/dev/null 2>&1 || true
    make -j"$JOBS" BUILD_LINUX=1 BUILD_MMX= BUILD_OPENGL= BUILD_LOADGL= \
      BUILD_WEBM= NO_STRIP=1 VERSION_NAME=OpenBOR LNXDEV=/usr/bin PREFIX= \
      GCC_TARGET=aarch64-linux-gnu TARGET_ARCH=aarch64 \
      ARCHFLAGS="$COMMON_CFLAGS -DPLUMOS_PIXEL2=1 -Isource/webmlib" \
      LIBRARIES=/usr/lib/aarch64-linux-gnu CC=gcc
  ) >>"$LOG_DIR/openbor.log" 2>&1 || return 1
  binary="$src/engine/OpenBOR"
  [ -x "$binary" ] || return 1
  file "$binary" | grep -q 'ELF 64-bit.*ARM aarch64' || return 1
  install -m 0755 "$binary" "$PLUMOS_DIR/standalone/openbor/bin/OpenBOR"
  "$STRIP" "$PLUMOS_DIR/standalone/openbor/bin/OpenBOR" >/dev/null 2>&1 || true
  install -m 0644 "$src/LICENSE" "$PLUMOS_DIR/licenses/openbor-LICENSE.txt"
  copy_runtime_deps "$PLUMOS_DIR/standalone/openbor/bin/OpenBOR"
}

build_pcsx_rearmed() {
  selected pcsx_rearmed || return 0
  for command in cmake file git make ninja readelf sha256sum; do
    require_command "$command"
  done
  for input in \
    "$PCSX_INPUT_AUDIO_PATCH" \
    "$PCSX_PLATFORM_PATCH" \
    "$PCSX_EVDEV_MENU_PATCH" \
    "$PCSX_PICOFE_PATCH" \
    "$PCSX_FBDEV_HEADER"; do
    [ -s "$input" ] || {
      printf 'error: PCSX-ReARMed Pixel2 input is missing: %s\n' "$input" >&2
      return 1
    }
  done

  PCSX_LOG="$LOG_DIR/pcsx_rearmed.log"
  PCSX_DST="$PLUMOS_DIR/standalone/pcsx_rearmed"
  PCSX_SDL_BUILD="$BUILD_ROOT/pcsx-sdl12-build"
  PCSX_SDL_PREFIX="$BUILD_ROOT/pcsx-sdl12-prefix"
  mkdir -p "$LOG_DIR" "$PLUMOS_DIR/licenses"
  : >"$PCSX_LOG"

  src=$(clone_checkout pcsx_rearmed "$PCSX_REPO" "$PCSX_REF") || return 1
  git -C "$src" submodule sync frontend/libpicofe >>"$PCSX_LOG" 2>&1
  git -C "$src" submodule update --init --force frontend/libpicofe \
    >>"$PCSX_LOG" 2>&1 || return 1
  [ "$(git -C "$src" rev-parse HEAD)" = "$PCSX_REF" ] || {
    printf 'error: unexpected PCSX-ReARMed source ref\n' >&2
    return 1
  }
  [ "$(git -C "$src/frontend/libpicofe" rev-parse HEAD)" = \
      "$PCSX_PICOFE_REF" ] || {
    printf 'error: unexpected PCSX-ReARMed libpicofe source ref\n' >&2
    return 1
  }
  git -C "$src" apply --check "$PCSX_INPUT_AUDIO_PATCH" \
    >>"$PCSX_LOG" 2>&1
  git -C "$src" apply "$PCSX_INPUT_AUDIO_PATCH" >>"$PCSX_LOG" 2>&1
  git -C "$src" apply --check "$PCSX_PLATFORM_PATCH" \
    >>"$PCSX_LOG" 2>&1
  git -C "$src" apply "$PCSX_PLATFORM_PATCH" >>"$PCSX_LOG" 2>&1
  git -C "$src" apply --check "$PCSX_EVDEV_MENU_PATCH" \
    >>"$PCSX_LOG" 2>&1
  git -C "$src" apply "$PCSX_EVDEV_MENU_PATCH" >>"$PCSX_LOG" 2>&1
  git -C "$src/frontend/libpicofe" apply --check --unidiff-zero \
    "$PCSX_PICOFE_PATCH" \
    >>"$PCSX_LOG" 2>&1
  git -C "$src/frontend/libpicofe" apply --unidiff-zero \
    "$PCSX_PICOFE_PATCH" \
    >>"$PCSX_LOG" 2>&1
  install -m 0644 "$PCSX_FBDEV_HEADER" "$src/frontend/pcsx_pixel2_fbdev.h"

  sdl_src=$(clone_checkout pcsx_sdl12_compat "$PCSX_SDL12_REPO" \
    "$PCSX_SDL12_REF") || return 1
  [ "$(git -C "$sdl_src" rev-parse HEAD)" = "$PCSX_SDL12_REF" ] || {
    printf 'error: unexpected sdl12-compat source ref\n' >&2
    return 1
  }
  rm -rf "$PCSX_SDL_BUILD" "$PCSX_SDL_PREFIX"
  cmake -S "$sdl_src" -B "$PCSX_SDL_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PCSX_SDL_PREFIX" \
    -DSDL12TESTS=OFF >>"$PCSX_LOG" 2>&1
  cmake --build "$PCSX_SDL_BUILD" -j"$JOBS" >>"$PCSX_LOG" 2>&1
  cmake --install "$PCSX_SDL_BUILD" >>"$PCSX_LOG" 2>&1
  PCSX_SDL_LIBRARY=$(find "$PCSX_SDL_PREFIX" -type f \
    -name 'libSDL-1.2.so.*' -print -quit)
  [ -n "$PCSX_SDL_LIBRARY" ] && [ -f "$PCSX_SDL_LIBRARY" ] || {
    printf 'error: sdl12-compat library was not installed\n' >&2
    return 1
  }

  (
    cd "$src" || exit 1
    make clean >/dev/null 2>&1 || true
    env \
      CC=gcc CXX=g++ AR=ar RANLIB=ranlib STRIP="$STRIP" \
      CFLAGS="$COMMON_CFLAGS -DPLUMOS_PIXEL2=1" \
      CXXFLAGS="$COMMON_CFLAGS -DPLUMOS_PIXEL2=1" \
      SDL_CONFIG="$PCSX_SDL_PREFIX/bin/sdl-config" \
      PATH="$PCSX_SDL_PREFIX/bin:$PATH" \
      ./configure \
        --platform=generic \
        --gpu=neon \
        --sound-drivers=alsa \
        --enable-neon \
        --enable-threads \
        --disable-dynamic \
        --dynarec=ari64
    make -j"$JOBS"
  ) >>"$PCSX_LOG" 2>&1 || return 1

  PCSX_BINARY="$src/pcsx"
  [ -x "$PCSX_BINARY" ] || {
    printf 'error: PCSX-ReARMed binary was not produced\n' >&2
    return 1
  }
  file "$PCSX_BINARY" | grep -q 'ELF 64-bit.*ARM aarch64' || return 1
  needed="$(readelf -d "$PCSX_BINARY" | awk -F'[][]' '/NEEDED/ { print $2 }')"
  for library in libSDL-1.2.so.0 libasound.so.2; do
    grep -qx "$library" <<<"$needed" || {
      printf 'error: PCSX-ReARMed dependency missing: %s\n' "$library" >&2
      return 1
    }
  done

  rm -rf "$PCSX_DST"
  mkdir -p "$PCSX_DST/bin" "$PCSX_DST/lib" "$PCSX_DST/skin"
  install -m 0755 "$PCSX_BINARY" "$PCSX_DST/bin/pcsx"
  "$STRIP" "$PCSX_DST/bin/pcsx" >/dev/null 2>&1 || true
  install -m 0644 "$PCSX_SDL_LIBRARY" "$PCSX_DST/lib/libSDL-1.2.so.0"
  install -m 0644 "$src/frontend/pandora/skin/font.png" \
    "$PCSX_DST/skin/font.png"
  install -m 0644 "$src/frontend/pandora/skin/font.png" \
    "$PCSX_DST/skin/fontx2.png"
  install -m 0644 "$src/frontend/pandora/skin/selector.png" \
    "$PCSX_DST/skin/selector.png"
  install -m 0644 "$src/frontend/pandora/skin/selector.png" \
    "$PCSX_DST/skin/selectorx2.png"
  install -m 0644 "$src/frontend/pandora/skin/background.png" \
    "$PCSX_DST/skin/background.png"
  install -m 0644 "$src/frontend/pandora/skin/skin.txt" \
    "$PCSX_DST/skin/skin.txt"
  install -m 0644 "$src/COPYING" \
    "$PLUMOS_DIR/licenses/pcsx-rearmed-COPYING.txt"
  install -m 0644 "$sdl_src/LICENSE.txt" \
    "$PLUMOS_DIR/licenses/sdl12-compat-LICENSE.txt"
  copy_runtime_deps "$PCSX_DST/bin/pcsx"
  copy_runtime_deps "$PCSX_DST/lib/libSDL-1.2.so.0"
  PCSX_SDL2_LIBRARY=$(readlink -f /usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0)
  [ -f "$PCSX_SDL2_LIBRARY" ] || {
    printf 'error: PCSX-ReARMed SDL2 runtime is unavailable\n' >&2
    return 1
  }
  pcsx_sdl2_real_name=$(basename "$PCSX_SDL2_LIBRARY")
  install -m 0644 "$PCSX_SDL2_LIBRARY" "$PLUMOS_DIR/lib/$pcsx_sdl2_real_name"
  pcsx_sdl2_map=$(printf '%s\t%s' libSDL2-2.0.so.0 "$pcsx_sdl2_real_name")
  grep -Fqx "$pcsx_sdl2_map" \
    "$PLUMOS_DIR/config/standalone/soname-links.tsv" ||
    printf '%s\n' "$pcsx_sdl2_map" >> \
      "$PLUMOS_DIR/config/standalone/soname-links.tsv"
  copy_runtime_deps "$PCSX_SDL2_LIBRARY"

  binary_sha256=$(sha256_file "$PCSX_DST/bin/pcsx")
  sdl_sha256=$(sha256_file "$PCSX_DST/lib/libSDL-1.2.so.0")
  input_audio_sha256=$(sha256_file "$PCSX_INPUT_AUDIO_PATCH")
  platform_sha256=$(sha256_file "$PCSX_PLATFORM_PATCH")
  evdev_menu_sha256=$(sha256_file "$PCSX_EVDEV_MENU_PATCH")
  picofe_sha256=$(sha256_file "$PCSX_PICOFE_PATCH")
  fbdev_sha256=$(sha256_file "$PCSX_FBDEV_HEADER")
  cat >"$PCSX_DST/build-manifest.json" <<EOF
{
  "device": "pixel2",
  "version": "$VERSION",
  "project_source_ref": "$PROJECT_SOURCE_REF",
  "source_date_epoch": $SOURCE_EPOCH,
  "upstream": "$PCSX_REPO",
  "commit": "$PCSX_REF",
  "libpicofe_commit": "$PCSX_PICOFE_REF",
  "sdl12_compat": {
    "upstream": "$PCSX_SDL12_REPO",
    "commit": "$PCSX_SDL12_REF",
    "sha256": "$sdl_sha256"
  },
  "binary": {
    "path": "bin/pcsx",
    "sha256": "$binary_sha256"
  },
  "patches": {
    "input_audio": "$input_audio_sha256",
    "platform": "$platform_sha256",
    "evdev_menu": "$evdev_menu_sha256",
    "libpicofe_input": "$picofe_sha256",
    "fbdev_presenter": "$fbdev_sha256"
  },
  "renderer": "builtin-neon-threaded-pixel2-fbdev-ccw",
  "display": "640x480-logical-on-480x640-physical",
  "audio": "alsa-plumos-output-44100-to-48000",
  "input": "pixel2-evdev-function-menu-and-sdl-fallback",
  "factory_config": "factory-defaults/standalone/pcsx_rearmed/pcsx.cfg"
}
EOF
}

build_drastic() {
  selected drastic || return 0
  for command in curl debugfs git make python3 sha256sum; do
    require_command "$command"
  done
  require_command arm-linux-gnueabihf-gcc

  DRASTIC_BUILD_DIR="$BUILD_ROOT/drastic"
  DRASTIC_SOURCE_DIR="$DRASTIC_BUILD_DIR/source"
  DRASTIC_RELEASE_DIR="$DRASTIC_BUILD_DIR/release"
  DRASTIC_OVERLAY_DIR="$DRASTIC_BUILD_DIR/overlay"
  DRASTIC_LOG="$LOG_DIR/drastic.log"
  DRASTIC_DST="$PLUMOS_DIR/standalone/drastic"
  mkdir -p "$LOG_DIR" "$ROOT_DIR/output/downloads" "$PLUMOS_DIR/licenses"
  : >"$DRASTIC_LOG"

  if [ ! -f "$DRASTIC_ARCHIVE" ]; then
    curl -L --fail --retry 3 --output "$DRASTIC_ARCHIVE" \
      "$DRASTIC_RELEASE_URL" >>"$DRASTIC_LOG" 2>&1
  fi
  verify_sha256 "$DRASTIC_RELEASE_SHA256" "$DRASTIC_ARCHIVE"

  rm -rf "$DRASTIC_RELEASE_DIR"
  mkdir -p "$DRASTIC_RELEASE_DIR"
  extract_zip "$DRASTIC_ARCHIVE" "$DRASTIC_RELEASE_DIR"
  [ -x "$DRASTIC_RELEASE_DIR/drastic/drastic" ] || {
    printf 'error: release archive does not contain the ARM32 DraStic core\n' >&2
    return 1
  }
  ln -sf libSDL2_image-2.0.so.0 \
    "$DRASTIC_RELEASE_DIR/drastic/lib/libSDL2_image.so"
  ln -sf libSDL2_ttf-2.0.so.0 \
    "$DRASTIC_RELEASE_DIR/drastic/lib/libSDL2_ttf.so"

  rm -rf "$DRASTIC_SOURCE_DIR"
  git clone --filter=blob:none --no-checkout --depth 1 \
    --branch "$DRASTIC_TAG" "$DRASTIC_REPO" "$DRASTIC_SOURCE_DIR" \
    >>"$DRASTIC_LOG" 2>&1
  git -C "$DRASTIC_SOURCE_DIR" sparse-checkout init --no-cone \
    >>"$DRASTIC_LOG" 2>&1
  git -C "$DRASTIC_SOURCE_DIR" sparse-checkout set \
    Makefile.base \
    Makefile.gkd_pixel2 \
    LICENSE \
    alsa \
    assets/gkd_pixel2 \
    common \
    detour \
    drastic \
    inc \
    runner \
    sdl2 >>"$DRASTIC_LOG" 2>&1
  git -C "$DRASTIC_SOURCE_DIR" checkout "$DRASTIC_REF" \
    >>"$DRASTIC_LOG" 2>&1
  git -C "$DRASTIC_SOURCE_DIR" apply "$DRASTIC_PATCH" \
    >>"$DRASTIC_LOG" 2>&1
  make -C "$DRASTIC_SOURCE_DIR" -f Makefile.gkd_pixel2 -j"$JOBS" \
    TOOLCHAIN_BIN="$(dirname "$(command -v arm-linux-gnueabihf-gcc)")" \
    NDS_INCLUDE_ROOT=/usr/include \
    NDS_LIBRARY_ROOT="$DRASTIC_RELEASE_DIR/drastic/lib" >>"$DRASTIC_LOG" 2>&1
  runner_rel="$(git -C "$DRASTIC_SOURCE_DIR" rev-parse --short=8 HEAD)"
  gcc -O3 -DGKD_PIXEL2 -DRUNNER -DREL_VER="0x${runner_rel}" \
    -I/usr/include/SDL2 \
    -I"$DRASTIC_SOURCE_DIR/runner" -I"$DRASTIC_SOURCE_DIR/common" \
    "$DRASTIC_SOURCE_DIR/runner/runner.c" \
    "$DRASTIC_SOURCE_DIR/common/common.c" \
    -o "$DRASTIC_SOURCE_DIR/runner/runner" \
    -L"$DRASTIC_SOURCE_DIR/assets/gkd_pixel2/lib" \
    -lSDL2 -lSDL2_image -lEGL -lGLESv2 -l:libjson-c.so.5 \
    >>"$DRASTIC_LOG" 2>&1

  rm -rf "$DRASTIC_DST"
  mkdir -p "$DRASTIC_DST/bin" "$DRASTIC_DST/lib" "$DRASTIC_DST/runtime/lib32"
  rsync -a \
    --exclude='._*' \
    --exclude='drastic' \
    --exclude='drastic64' \
    --exclude='launch.sh' \
    --exclude='overlayfs.img' \
    --exclude='system/drastic_bios_*.bin' \
    --exclude='lib/libcommon.so' \
    --exclude='lib/libdtr.so' \
    --exclude='lib/libSDL2-2.0.so.0' \
    "$DRASTIC_RELEASE_DIR/drastic/" "$DRASTIC_DST/"
  rsync -a --exclude='launch.sh' \
    "$DRASTIC_SOURCE_DIR/assets/gkd_pixel2/" "$DRASTIC_DST/"
  if grep -Fqx 'controls_b[CONTROL_INDEX_MENU] = 1154' \
      "$DRASTIC_DST/config/drastic.cfg"; then
    sed -i \
      's/^controls_b\[CONTROL_INDEX_MENU\] = 1154$/controls_b[CONTROL_INDEX_MENU] = 1032/' \
      "$DRASTIC_DST/config/drastic.cfg"
  fi
  grep -Fqx 'controls_b[CONTROL_INDEX_MENU] = 1032' \
    "$DRASTIC_DST/config/drastic.cfg" || {
    printf 'error: DraStic Pixel2 FUNCTION menu binding is missing\n' >&2
    return 1
  }
  install -m 0755 "$DRASTIC_RELEASE_DIR/drastic/drastic" \
    "$DRASTIC_DST/bin/drastic"
  install -m 0755 "$DRASTIC_SOURCE_DIR/runner/runner" \
    "$DRASTIC_DST/bin/runner"
  install -m 0755 /usr/bin/setarch "$DRASTIC_DST/bin/setarch"
  install -m 0644 "$DRASTIC_SOURCE_DIR/drastic/lib/libcommon.so" \
    "$DRASTIC_DST/lib/libcommon.so"
  install -m 0644 "$DRASTIC_SOURCE_DIR/drastic/lib/libdtr.so" \
    "$DRASTIC_DST/lib/libdtr.so"
  install -m 0644 "$DRASTIC_SOURCE_DIR/drastic/lib/libSDL2-2.0.so.0" \
    "$DRASTIC_DST/lib/libSDL2-2.0.so.0"
  "$ARMHF_STRIP" --strip-unneeded \
    "$DRASTIC_DST/lib/libcommon.so" \
    "$DRASTIC_DST/lib/libdtr.so" \
    "$DRASTIC_DST/lib/libSDL2-2.0.so.0" >/dev/null 2>&1 || true

  rm -rf "$DRASTIC_OVERLAY_DIR"
  mkdir -p "$DRASTIC_OVERLAY_DIR"
  if ! debugfs -R "rdump / $DRASTIC_OVERLAY_DIR" \
      "$DRASTIC_RELEASE_DIR/drastic/overlayfs.img" >>"$DRASTIC_LOG" 2>&1; then
    printf 'warning: debugfs returned non-zero after DraStic overlay extraction; validating extracted files\n' \
      >>"$DRASTIC_LOG"
  fi
  copy_overlay_file ld-linux-armhf.so.3 \
    "$DRASTIC_DST/runtime/ld-linux-armhf.so.3"
  for library in \
    libEGL.so.1 \
    libGLESv2.so.2 \
    libasound.so.2 \
    libc.so.6 \
    libdl.so.2 \
    libdrm.so.2 \
    libfreetype.so.6 \
    libgbm.so.1 \
    libgcc_s.so.1 \
    libm.so.6 \
    libmali.so.1 \
    libmali_hook.so.1 \
    libpthread.so.0 \
    librt.so.1 \
    libstdc++.so.6; do
    copy_overlay_file "$library" "$DRASTIC_DST/runtime/lib32/$library"
  done

  arm-linux-gnueabihf-gcc -Os -fPIC -shared \
    -Wl,-soname,libdrastic_mmap_compat.so \
    -o "$DRASTIC_DST/lib/libdrastic_mmap_compat.so" "$DRASTIC_MMAP_COMPAT" \
    >>"$DRASTIC_LOG" 2>&1
  "$ARMHF_STRIP" --strip-unneeded \
    "$DRASTIC_DST/lib/libdrastic_mmap_compat.so" >/dev/null 2>&1 || true

  install -m 0644 "$DRASTIC_SOURCE_DIR/LICENSE" \
    "$PLUMOS_DIR/licenses/steward-fu-nds-LGPL-2.1"
  install -m 0644 "$DRASTIC_RELEASE_DIR/drastic/readme.txt" \
    "$PLUMOS_DIR/licenses/drastic-upstream-release-readme.txt"

  core_sha256=$(sha256_file "$DRASTIC_DST/bin/drastic")
  common_sha256=$(sha256_file "$DRASTIC_DST/lib/libcommon.so")
  detour_sha256=$(sha256_file "$DRASTIC_DST/lib/libdtr.so")
  sdl2_sha256=$(sha256_file "$DRASTIC_DST/lib/libSDL2-2.0.so.0")
  compat_sha256=$(sha256_file "$DRASTIC_DST/lib/libdrastic_mmap_compat.so")
  cat >"$DRASTIC_DST/build-manifest.json" <<EOF
{
  "device": "pixel2",
  "upstream": "steward-fu/nds",
  "source_ref": "$DRASTIC_REF",
  "release_asset": "drastic_miyoo-flip_20251104.zip",
  "release_sha256": "$DRASTIC_RELEASE_SHA256",
  "closed_core": {
    "path": "bin/drastic",
    "sha256": "$core_sha256",
    "built_from_source": false
  },
  "source_built_integration": {
    "target": "gkd_pixel2",
    "libcommon.so": "$common_sha256",
    "libdtr.so": "$detour_sha256",
    "libSDL2-2.0.so.0": "$sdl2_sha256",
    "libdrastic_mmap_compat.so": "$compat_sha256"
  },
  "runtime_contract": "package-local-armhf-gkd-pixel2",
  "global_usr_overlay": false,
  "process_aslr": "disabled-only-for-drastic"
}
EOF
}

build_ppsspp() {
  selected ppsspp || return 0
  for command in cmake file git ninja readelf rsync sha256sum; do
    require_command "$command"
  done
  [ -s "$PPSSPP_PATCH" ] || {
    printf 'error: PPSSPP patch is missing: %s\n' "$PPSSPP_PATCH" >&2
    return 1
  }

  PPSSPP_BUILD_DIR="$BUILD_ROOT/ppsspp"
  PPSSPP_SOURCE_DIR="$PPSSPP_BUILD_DIR/source"
  PPSSPP_CMAKE_DIR="$PPSSPP_BUILD_DIR/build"
  PPSSPP_LOG="$LOG_DIR/ppsspp.log"
  PPSSPP_DST="$PLUMOS_DIR/standalone/ppsspp"
  PPSSPP_FACTORY="$PLUMOS_DIR/factory-defaults/standalone/ppsspp/PSP/SYSTEM"
  mkdir -p "$LOG_DIR" "$PLUMOS_DIR/licenses" "$PPSSPP_DST/bin" \
    "$PPSSPP_DST/assets" "$PPSSPP_FACTORY"
  : >"$PPSSPP_LOG"

  if [ ! -d "$PPSSPP_SOURCE_DIR/.git" ]; then
    [ ! -e "$PPSSPP_SOURCE_DIR" ] || {
      printf 'error: PPSSPP source path is not a Git clone: %s\n' \
        "$PPSSPP_SOURCE_DIR" >&2
      return 1
    }
    mkdir -p "$(dirname "$PPSSPP_SOURCE_DIR")"
    git clone --filter=blob:none "$PPSSPP_REPO" "$PPSSPP_SOURCE_DIR" \
      >>"$PPSSPP_LOG" 2>&1 || return 1
  fi
  git -C "$PPSSPP_SOURCE_DIR" fetch --tags --force origin "$PPSSPP_REF" \
    >>"$PPSSPP_LOG" 2>&1 || return 1
  git -C "$PPSSPP_SOURCE_DIR" checkout --detach "$PPSSPP_COMMIT" \
    >>"$PPSSPP_LOG" 2>&1 || return 1
  git -C "$PPSSPP_SOURCE_DIR" reset --hard "$PPSSPP_COMMIT" \
    >>"$PPSSPP_LOG" 2>&1 || return 1
  git -C "$PPSSPP_SOURCE_DIR" clean -fdx >>"$PPSSPP_LOG" 2>&1 || return 1
  git -C "$PPSSPP_SOURCE_DIR" submodule sync --recursive \
    >>"$PPSSPP_LOG" 2>&1 || return 1
  git -C "$PPSSPP_SOURCE_DIR" submodule update --init --recursive \
    >>"$PPSSPP_LOG" 2>&1 || return 1
  git -C "$PPSSPP_SOURCE_DIR" apply --check "$PPSSPP_PATCH" \
    >>"$PPSSPP_LOG" 2>&1
  git -C "$PPSSPP_SOURCE_DIR" apply "$PPSSPP_PATCH" >>"$PPSSPP_LOG" 2>&1

  rm -rf "$PPSSPP_CMAKE_DIR"
  cmake -S "$PPSSPP_SOURCE_DIR" -B "$PPSSPP_CMAKE_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="$COMMON_CFLAGS" \
    -DCMAKE_CXX_FLAGS="$COMMON_CFLAGS -DPLUMOS_PIXEL2=1" \
    -DARM64=ON \
    -DARMV7=OFF \
    -DUSING_EGL=OFF \
    -DUSING_FBDEV=ON \
    -DUSING_GLES2=ON \
    -DPLUMOS_PIXEL2=ON \
    -DVULKAN=OFF \
    -DUSING_X11_VULKAN=OFF \
    -DUSE_WAYLAND_WSI=OFF \
    -DUSE_VULKAN_DISPLAY_KHR=OFF \
    -DCMAKE_DISABLE_FIND_PACKAGE_X11=TRUE \
    -DUSE_FFMPEG=ON \
    -DUSE_SYSTEM_FFMPEG=OFF \
    -DUSE_DISCORD=OFF \
    -DUSE_MINIUPNPC=OFF \
    -DUSE_SYSTEM_LIBSDL2=ON \
    -DSDL2_INCLUDE_DIR=/usr/include/SDL2 \
    -DSDL2_LIBRARY=/usr/lib/aarch64-linux-gnu/libSDL2.so \
    -DUSE_SYSTEM_FREETYPE=OFF \
    -DUSE_SYSTEM_LIBCHDR=OFF \
    -DUSE_SYSTEM_LIBZIP=OFF \
    -DUSE_SYSTEM_SNAPPY=OFF \
    -DUSE_SYSTEM_ZSTD=OFF \
    -DHEADLESS=OFF \
    -DUNITTEST=OFF \
    -DATLAS_TOOL=OFF \
    -DUSING_QT_UI=OFF \
    -DMOBILE_DEVICE=OFF \
    -DGOLD=OFF >>"$PPSSPP_LOG" 2>&1
  cmake --build "$PPSSPP_CMAKE_DIR" --target PPSSPPSDL -j"$JOBS" \
    >>"$PPSSPP_LOG" 2>&1 || return 1

  PPSSPP_BINARY="$PPSSPP_CMAKE_DIR/PPSSPPSDL"
  [ -x "$PPSSPP_BINARY" ] || {
    printf 'error: PPSSPP binary was not produced: %s\n' "$PPSSPP_BINARY" >&2
    return 1
  }
  file "$PPSSPP_BINARY" | grep -q 'ELF 64-bit.*ARM aarch64' || return 1
  needed="$(readelf -d "$PPSSPP_BINARY" | awk -F'[][]' '/NEEDED/ { print $2 }')"
  for library in libSDL2-2.0.so.0 libGLESv2.so.2 libEGL.so.1; do
    grep -qx "$library" <<<"$needed" || {
      printf 'error: PPSSPP dependency missing: %s\n' "$library" >&2
      return 1
    }
  done
  if grep -Eq '^lib(X11|Xext|vulkan)' <<<"$needed"; then
    printf 'error: PPSSPP contains an unintended desktop dependency\n' >&2
    return 1
  fi

  rm -rf "$PPSSPP_DST"
  mkdir -p "$PPSSPP_DST/bin" "$PPSSPP_DST/assets"
  install -m 0755 "$PPSSPP_BINARY" "$PPSSPP_DST/bin/PPSSPPSDL"
  "$STRIP" "$PPSSPP_DST/bin/PPSSPPSDL" >/dev/null 2>&1 || true
  rsync -a --delete "$PPSSPP_SOURCE_DIR/assets/" "$PPSSPP_DST/assets/"
  install -m 0644 "$PPSSPP_SOURCE_DIR/LICENSE.TXT" \
    "$PLUMOS_DIR/licenses/ppsspp-LICENSE.txt"
  copy_runtime_deps "$PPSSPP_DST/bin/PPSSPPSDL"

  binary_sha256=$(sha256_file "$PPSSPP_DST/bin/PPSSPPSDL")
  asset_tree_sha256="$(
    cd "$PPSSPP_DST"
    find assets -type f -print0 |
      sort -z |
      xargs -0 sha256sum |
      sha256sum |
      awk '{ print $1 }'
  )"
  patch_sha256=$(sha256_file "$PPSSPP_PATCH")
  cat >"$PPSSPP_DST/build-manifest.json" <<EOF
{
  "device": "pixel2",
  "upstream": "$PPSSPP_REPO",
  "ref": "$PPSSPP_REF",
  "commit": "$PPSSPP_COMMIT",
  "binary": {
    "path": "bin/PPSSPPSDL",
    "sha256": "$binary_sha256"
  },
  "asset_tree_sha256": "$asset_tree_sha256",
  "patches": {
    "ppsspp-1.20.4-pixel2-no-sdl2-ttf": "$patch_sha256"
  },
  "target_cpu": "cortex-a35",
  "compiler_flags": "$COMMON_CFLAGS",
  "renderer": "pixel2-kmsdrm-gles2",
  "audio": "alsa-plumos-output",
  "factory_config": "factory-defaults/standalone/ppsspp/PSP/SYSTEM"
}
EOF
}

rm -rf "$OUT_ROOT"
mkdir -p "$PLUMOS_DIR" "$COMPONENT_DIR" "$PLUMOS_DIR/licenses" \
  "$PLUMOS_DIR/config/standalone" "$PLUMOS_DIR/standalone" \
  "$PLUMOS_DIR/lib" "$LOG_DIR"
rsync -a "$PACKAGE_ROOT/" "$PLUMOS_DIR/"
chmod 0755 "$PLUMOS_DIR/bin/plumos-standalone-launch" \
  "$PLUMOS_DIR/bin/plumos-standalone-stop"
: >"$PLUMOS_DIR/config/standalone/soname-links.tsv"

OPENBOR_STATUS=pending-binary
PCSX_STATUS=pending-binary
DRASTIC_STATUS=pending-binary
PPSSPP_STATUS=pending-binary
if selected openbor; then
  build_openbor
  OPENBOR_STATUS=built
fi
if selected pcsx_rearmed; then
  build_pcsx_rearmed
  PCSX_STATUS=built
fi
if selected drastic; then
  build_drastic
  DRASTIC_STATUS=built
fi
if selected ppsspp; then
  build_ppsspp
  PPSSPP_STATUS=built
fi

cat > "$COMPONENT_DIR/manifest.json" <<EOF
{
  "name": "plumOS Pixel2 standalone launcher",
  "component": "standalone",
  "device": "pixel2",
  "architecture": "aarch64",
  "version": "$VERSION",
  "source_ref": "$PROJECT_SOURCE_REF",
  "source_date_epoch": $SOURCE_EPOCH,
  "status": "partial-binaries",
  "runtime_contract": "Pixel2 app-layer launcher, ALSA plumos_output, SDL/KMSDRM defaults",
  "emulators": [
EOF
cat >> "$COMPONENT_DIR/manifest.json" <<EOF
    {"id": "pcsx_rearmed", "status": "$PCSX_STATUS"},
EOF
cat >> "$COMPONENT_DIR/manifest.json" <<EOF
    {"id": "ppsspp", "status": "$PPSSPP_STATUS"},
EOF
cat >> "$COMPONENT_DIR/manifest.json" <<EOF
    {"id": "drastic", "status": "$DRASTIC_STATUS"},
EOF
cat >> "$COMPONENT_DIR/manifest.json" <<EOF
    {"id": "openbor", "status": "$OPENBOR_STATUS"},
EOF
cat >> "$COMPONENT_DIR/manifest.json" <<'EOF'
    {"id": "scummvm", "status": "pending-binary"},
    {"id": "easyrpg", "status": "pending-binary"},
    {"id": "flycast", "status": "pending-binary"},
    {"id": "mupen64plus", "status": "pending-binary"},
    {"id": "nxengine-evo", "status": "pending-binary"}
  ]
}
EOF

(
  cd "$PLUMOS_DIR"
  find bin config/standalone components/standalone standalone \
    factory-defaults/standalone lib licenses -type f \
    ! -path 'components/standalone/checksums.sha256' \
    -print |
    sort |
    while IFS= read -r path; do sha256sum "$path"; done
) > "$COMPONENT_DIR/checksums.sha256"
(
  cd "$PLUMOS_DIR"
  sha256sum -c components/standalone/checksums.sha256 >/dev/null
)
printf 'created: %s\n' "$PLUMOS_DIR"
