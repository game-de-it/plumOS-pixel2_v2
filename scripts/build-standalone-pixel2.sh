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
FILTER=all
OPENBOR_REPO="${PLUMOS_PIXEL2_OPENBOR_REPO:-https://github.com/DCurrent/openbor.git}"
OPENBOR_REF="${PLUMOS_PIXEL2_OPENBOR_REF:-494708eb34e71d1afda237873907701c4ec3a569}"
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

rm -rf "$OUT_ROOT"
mkdir -p "$PLUMOS_DIR" "$COMPONENT_DIR" "$PLUMOS_DIR/licenses" \
  "$PLUMOS_DIR/config/standalone" "$PLUMOS_DIR/standalone" \
  "$PLUMOS_DIR/lib" "$LOG_DIR"
rsync -a "$PACKAGE_ROOT/" "$PLUMOS_DIR/"
chmod 0755 "$PLUMOS_DIR/bin/plumos-standalone-launch" \
  "$PLUMOS_DIR/bin/plumos-standalone-stop"
: >"$PLUMOS_DIR/config/standalone/soname-links.tsv"

OPENBOR_STATUS=pending-binary
if selected openbor; then
  if build_openbor; then
    OPENBOR_STATUS=built
  else
    printf 'error: OpenBOR standalone build failed; see %s\n' \
      "$LOG_DIR/openbor.log" >&2
    exit 1
  fi
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
    {"id": "drastic", "status": "pending-binary"},
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
