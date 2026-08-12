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
COMMON_CFLAGS="${PLUMOS_PIXEL2_STANDALONE_CFLAGS:--O2 -pipe -march=armv8-a+crc -mtune=cortex-a35 -fomit-frame-pointer -fcommon}"

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
  install -m 0755 "$DRASTIC_RELEASE_DIR/drastic/drastic" \
    "$DRASTIC_DST/bin/drastic"
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

rm -rf "$OUT_ROOT"
mkdir -p "$PLUMOS_DIR" "$COMPONENT_DIR" "$PLUMOS_DIR/licenses" \
  "$PLUMOS_DIR/config/standalone" "$PLUMOS_DIR/standalone" \
  "$PLUMOS_DIR/lib" "$LOG_DIR"
rsync -a "$PACKAGE_ROOT/" "$PLUMOS_DIR/"
chmod 0755 "$PLUMOS_DIR/bin/plumos-standalone-launch" \
  "$PLUMOS_DIR/bin/plumos-standalone-stop"
: >"$PLUMOS_DIR/config/standalone/soname-links.tsv"

OPENBOR_STATUS=pending-binary
DRASTIC_STATUS=pending-binary
if selected openbor; then
  build_openbor
  OPENBOR_STATUS=built
fi
if selected drastic; then
  build_drastic
  DRASTIC_STATUS=built
fi

cat > "$COMPONENT_DIR/manifest.json" <<'EOF'
{
  "name": "plumOS Pixel2 standalone launcher",
  "component": "standalone",
  "device": "pixel2",
  "architecture": "aarch64",
  "status": "partial-binaries",
  "runtime_contract": "Pixel2 app-layer launcher, ALSA plumos_output, SDL/KMSDRM defaults",
  "emulators": [
    {"id": "pcsx_rearmed", "status": "pending-binary"},
    {"id": "ppsspp", "status": "pending-binary"},
EOF
cat >> "$COMPONENT_DIR/manifest.json" <<EOF
    {"id": "drastic", "status": "$DRASTIC_STATUS"},
EOF
cat >> "$COMPONENT_DIR/manifest.json" <<'EOF'
    {"id": "yabasanshiro", "status": "pending-binary"},
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
  find bin config/standalone components/standalone standalone lib licenses -type f \
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
