#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
DEFAULT_ROOT_DIR="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="${ROOT_DIR:-$DEFAULT_ROOT_DIR}"
OUT_DIR_EXPLICIT=0
if [ "${PLUMOS_PIXEL2_CORES_OUT+x}" = x ]; then
  OUT_DIR_EXPLICIT=1
fi
OUT_ROOT="${PLUMOS_PIXEL2_CORES_OUT:-output/libretro-cores/pixel2}"
SRC_ROOT="${PLUMOS_PIXEL2_CORES_SRC:-output/build/libretro-cores-pixel2/src}"
CORE_RECIPES="${CORE_RECIPES:-${ROOT_DIR}/docker/pixel2-tools/libretro-core-recipes.tsv}"
CORE_INFO_REPO="${CORE_INFO_REPO:-https://github.com/libretro/libretro-core-info.git}"
CORE_INFO_REF="${CORE_INFO_REF:-HEAD}"
PLUMOS_CORE_FILTER="${PLUMOS_CORE_FILTER:-all}"
FAIL_ON_CORE_ERROR="${FAIL_ON_CORE_ERROR:-1}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"
BUILD_JOB_FALLBACKS="${BUILD_JOB_FALLBACKS:-1}"
LIBRETRO_SERIAL_CORES="${LIBRETRO_SERIAL_CORES:-nestopia quicknes gambatte gpsp picodrive mednafen_pce_fast mednafen_supergrafx mednafen_ngp mednafen_lynx handy prosystem gw pokemini mednafen_vb dinothawr mrboom tgbdual beetle_saturn flycast flycast_xtreme mupen64plus_next parallel_n64 yabasanshiro}"
COMMON_CFLAGS="${COMMON_CFLAGS:--O3 -pipe -DNDEBUG -fPIC -fomit-frame-pointer}"
COMMON_CXXFLAGS="${COMMON_CXXFLAGS:-$COMMON_CFLAGS}"
COMMON_LDFLAGS="${COMMON_LDFLAGS:-}"
CC="${CC:-gcc}"
CXX="${CXX:-g++}"
AS="${AS:-$CC -c}"
AR="${AR:-ar}"
RANLIB="${RANLIB:-ranlib}"
STRIP="${STRIP:-strip}"
READELF="${READELF:-readelf}"
YABASANSHIRO_CC="${YABASANSHIRO_CC:-clang}"
YABASANSHIRO_CXX="${YABASANSHIRO_CXX:-clang++}"
YABASANSHIRO_AS="${YABASANSHIRO_AS:-$YABASANSHIRO_CC -c}"
STAGE_EXISTING=0
REPLACE_CANONICAL=0

usage() {
  cat <<'EOF'
Usage:
  scripts/build-libretro-cores.sh [options]

Options:
  --out-dir PATH       Output root; default output/libretro-cores/pixel2.
  --src-root PATH      Source/build root; default output/build/libretro-cores-pixel2/src.
  --recipes PATH       Recipe TSV; default docker/pixel2-tools/libretro-core-recipes.tsv.
  --filter FILTER      all, pixel2, plumos, class-a, class-b, class-o,
                       class-ab, or comma-separated core IDs.
                       Default: all. Filtered builds use an isolated output.
  --jobs N             Per-core make jobs; default nproc.
  --fail-on-error 0|1  Fail if any selected core fails; default 1.
  --stage-existing     Restage already-built AArch64 core outputs without rebuilding.
  --replace-canonical  Allow a filtered build to replace the canonical full output.
  --list               Print selected recipes and exit.

Environment:
  PLUMOS_CORE_FILTER   Same as --filter.
  CORE_INFO_REPO/REF   libretro-core-info source used for .info files.
  YABASANSHIRO_CC/CXX  Compiler override for the pinned 2.10.4 AArch64 core.
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --out-dir)
      OUT_ROOT="$2"
      OUT_DIR_EXPLICIT=1
      shift 2
      ;;
    --src-root)
      SRC_ROOT="$2"
      shift 2
      ;;
    --recipes)
      CORE_RECIPES="$2"
      shift 2
      ;;
    --filter)
      PLUMOS_CORE_FILTER="$2"
      shift 2
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    --fail-on-error)
      FAIL_ON_CORE_ERROR="$2"
      shift 2
      ;;
    --stage-existing)
      STAGE_EXISTING=1
      shift
      ;;
    --replace-canonical)
      REPLACE_CANONICAL=1
      shift
      ;;
    --list)
      LIST_ONLY=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      printf 'error: unknown argument: %s\n' "$1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

LIST_ONLY="${LIST_ONLY:-0}"
case "$JOBS" in
  ''|*[!0-9]*) JOBS=2 ;;
esac
[ "$JOBS" -gt 0 ] || JOBS=1
BUILD_JOB_FALLBACKS="${BUILD_JOB_FALLBACKS//,/ }"
for fallback_job in $BUILD_JOB_FALLBACKS; do
  case "$fallback_job" in
    ''|*[!0-9]*) BUILD_JOB_FALLBACKS=1 ;;
  esac
done

case "$ROOT_DIR" in
  /*) ;;
  *) ROOT_DIR="$(pwd)/$ROOT_DIR" ;;
esac
case "$PLUMOS_CORE_FILTER" in
  all|ALL) complete_filter=1 ;;
  *) complete_filter=0 ;;
esac
if [ "$complete_filter" -eq 0 ] && [ "$OUT_DIR_EXPLICIT" -eq 0 ]; then
  safe_filter="$(printf '%s' "$PLUMOS_CORE_FILTER" |
    tr '[:upper:]' '[:lower:]' |
    sed 's/[^a-z0-9._-]/_/g; s/^_*//; s/_*$//')"
  [ -n "$safe_filter" ] || safe_filter=custom
  OUT_ROOT="output/libretro-cores/pixel2-filtered/$safe_filter"
fi
case "$OUT_ROOT" in
  /*) ;;
  *) OUT_ROOT="$ROOT_DIR/$OUT_ROOT" ;;
esac
CANONICAL_OUT_DIR="$ROOT_DIR/output/libretro-cores/pixel2"
if [ "$complete_filter" -eq 0 ] && [ "$OUT_ROOT" = "$CANONICAL_OUT_DIR" ] &&
   [ "$REPLACE_CANONICAL" -ne 1 ]; then
  printf 'error: filtered core builds cannot replace canonical output: %s\n' "$OUT_ROOT" >&2
  printf 'hint: omit --out-dir, choose a separate path, or pass --replace-canonical explicitly\n' >&2
  exit 2
fi
OUT_DIR="$OUT_ROOT/raw"
PLUMOS_DIR="$OUT_ROOT/plumos"
COMPONENT_DIR="$PLUMOS_DIR/components/libretro-cores"
case "$SRC_ROOT" in
  /*) ;;
  *) SRC_ROOT="$ROOT_DIR/$SRC_ROOT" ;;
esac
case "$CORE_RECIPES" in
  /*) ;;
  *) CORE_RECIPES="$ROOT_DIR/$CORE_RECIPES" ;;
esac
cd "$ROOT_DIR"

msg() {
  printf '[libretro-cores] %s\n' "$*" >&2
}

append_manifest() {
  printf '%s\n' "$*" >> "$MANIFEST"
}

core_aliases() {
  case "$1" in
    beetle_saturn) printf '%s\n' mednafen_saturn ;;
    mednafen_saturn) printf '%s\n' beetle_saturn ;;
    mednafen_lynx) printf '%s\n' beetle_lynx ;;
    mednafen_ngp) printf '%s\n' beetle_ngp ;;
    mednafen_pce_fast) printf '%s\n' beetle_pce_fast ;;
    mednafen_supergrafx) printf '%s\n' beetle_supergrafx ;;
    mednafen_vb) printf '%s\n' beetle_vb ;;
    mednafen_wswan) printf '%s\n' beetle_wswan ;;
  esac
}

core_output_aliases() {
  # Match Pixel2 package filenames when the Pixel2-safe aarch64 build is provided
  # by the same upstream family under a different libretro core name.
  case "$1:$2" in
    beetle_saturn:mednafen_saturn_libretro.so)
      printf '%s\n' beetle_saturn_libretro.so
      ;;
    dosbox_pure:dosbox_pure_libretro.so)
      printf '%s\n' dosbox_pure_0.9.7_libretro.so
      ;;
    puae:puae_libretro.so)
      printf '%s\n' km_puae_xtreme_amped_libretro.so
      ;;
    puae2021:puae2021_libretro.so)
      printf '%s\n' uae4arm_libretro.so
      ;;
  esac
}

core_stage_base() {
  case "$1:$2" in
    flycast_xtreme:flycast_libretro.so)
      printf '%s\n' flycast_xtreme_libretro.so
      ;;
    km_duckswanstation_xtreme_amped:swanstation_libretro.so)
      printf '%s\n' km_duckswanstation_xtreme_amped_libretro.so
      ;;
    km_mame2003_xtreme:km_mame2003_xtreme_amped_libretro.so)
      printf '%s\n' km_mame2003_xtreme_libretro.so
      ;;
    km_superbroswar:superbroswar_libretro.so)
      printf '%s\n' km_superbroswar_libretro.so
      ;;
    puae2021:puae_libretro.so)
      printf '%s\n' puae2021_libretro.so
      ;;
    *)
      printf '%s\n' "$2"
      ;;
  esac
}

core_selected() {
  local id="$1"
  local class="$2"
  local token
  local normalized
  local alias

  IFS=',' read -r -a filters <<< "$PLUMOS_CORE_FILTER"
  for token in "${filters[@]}"; do
    normalized="$(printf '%s' "$token" | tr -d '[:space:]')"
    [ -n "$normalized" ] || continue
    case "$normalized" in
      all|ALL)
        return 0
        ;;
      pixel2|plumos|default|class-plumos|Class-plumOS)
        { [ "$class" = "A" ] || [ "$class" = "B" ]; } && return 0
        ;;
      class-a|Class-A|a|A)
        [ "$class" = "A" ] && return 0
        ;;
      class-b|Class-B|b|B)
        [ "$class" = "B" ] && return 0
        ;;
      class-ab|Class-AB|ab|AB)
        { [ "$class" = "A" ] || [ "$class" = "B" ]; } && return 0
        ;;
      class-o|Class-O|o|O|extended-extra|Extended-extra)
        [ "$class" = "O" ] && return 0
        ;;
      "$id")
        return 0
        ;;
    esac
    for alias in $(core_aliases "$id"); do
      [ "$normalized" = "$alias" ] && return 0
    done
  done
  return 1
}

core_table() {
  awk '
    /^[[:space:]]*($|#)/ { next }
    { print }
  ' "$CORE_RECIPES"
}

clone_or_update_repo() {
  local id="$1"
  local repo="$2"
  local ref="$3"
  local dst="$4"
  local log="$5"

  if [ ! -d "$dst/.git" ]; then
    rm -rf "$dst"
    mkdir -p "$(dirname "$dst")"
    msg "cloning $id"
    git clone --recursive "$repo" "$dst" >> "$log" 2>&1
  fi

  (
    cd "$dst"
    git reset --hard --quiet HEAD
    git clean -fdx --quiet
    git fetch --tags --quiet origin
    if [ "$ref" = "HEAD" ]; then
      branch="$(git remote show origin 2>/dev/null | awk '/HEAD branch/ {print $NF; exit}')"
      if [ -n "$branch" ]; then
        git checkout --quiet "$branch"
        git reset --hard --quiet "origin/$branch"
      else
        git checkout --quiet HEAD
      fi
    else
      git fetch --quiet origin "$ref"
      fetched_commit="$(git rev-parse "FETCH_HEAD^{commit}")"
      git checkout --quiet --detach "$fetched_commit"
      [ "$(git rev-parse HEAD)" = "$fetched_commit" ] || exit 1
    fi
    git submodule update --init --recursive >> "$log" 2>&1 || true
    git clean -fdx --quiet
  ) >> "$log" 2>&1
}

find_makefile() {
  local work="$1"
  local hint="$2"
  local candidate

  if [ -n "$hint" ] && [ -f "$work/$hint" ]; then
    printf '%s\n' "$hint"
    return 0
  fi
  for candidate in \
    Makefile.libretro \
    makefile.libretro \
    makefilelibretro \
    libretro/Makefile \
    src/libretro/Makefile \
    platforms/libretro/Makefile \
    platform/libretro/Makefile \
    libretroBuildSystem/Makefile \
    CMakeLists.txt \
    Makefile \
    makefile; do
    if [ -f "$work/$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done
  return 1
}

cmake_target_from_args() {
  local id="$1"
  local arg

  for arg in "${args[@]}"; do
    case "$arg" in
      target=*)
        printf '%s\n' "${arg#target=}"
        return 0
        ;;
    esac
  done
  printf '%s_libretro\n' "$id"
}

cmake_args_without_internal_keys() {
  local arg

  for arg in "${args[@]}"; do
    case "$arg" in
      target=*)
        ;;
      *)
        printf '%s\n' "$arg"
      ;;
    esac
  done
}

core_initial_jobs() {
  local id="$1"
  local serial_cores=" ${LIBRETRO_SERIAL_CORES//,/ } "

  case "$serial_cores" in
    *" $id "*) printf '%s\n' 1 ;;
    *) printf '%s\n' "$JOBS" ;;
  esac
}

run_with_job_retry() {
  local id="$1"
  local log="$2"
  local job
  local seen=" "
  shift 2

  for job in "$(core_initial_jobs "$id")" $BUILD_JOB_FALLBACKS; do
    [ -n "$job" ] || continue
    case "$seen" in
      *" $job "*) continue ;;
    esac
    seen="${seen}${job} "
    printf '\n[plumOS] jobs=%s command=%s\n' "$job" "$*" >> "$log"
    if JOBS_ACTIVE="$job" "$@"; then
      LAST_SUCCESSFUL_JOBS="$job"
      return 0
    fi
    if [ "$job" != "1" ]; then
      msg "$id failed with jobs=$job; retrying lower parallelism"
    fi
  done
  return 1
}

patch_core_source() {
  local id="$1"
  local src="$2"
  local log="$3"
  local patch_dir="$ROOT_DIR/docker/pixel2-tools/patches"
  local lua_makefile

  if [ "$id" = "yabasanshiro" ]; then
    local arm64_gcc12_patch="$patch_dir/yabasanshiro-2.10.4-arm64-gcc12.patch"
    local vdp1_readback_patch="$patch_dir/yabasanshiro-2.10.4-vdp1-framebuffer-readback.patch"

    if [ ! -f "$arm64_gcc12_patch" ] || [ ! -f "$vdp1_readback_patch" ]; then
      printf '\n[plumOS] missing required YabaSanshiro 2.10.4 patch\n' >> "$log"
      return 1
    fi
    if ! patch --dry-run -d "$src" -p1 < "$arm64_gcc12_patch" >/dev/null 2>> "$log"; then
      printf '\n[plumOS] required YabaSanshiro 2.10.4 ARM64 register-clobber patch does not apply\n' >> "$log"
      return 1
    fi
    patch -d "$src" -p1 < "$arm64_gcc12_patch" >> "$log" 2>&1
    printf '\n[plumOS] patched YabaSanshiro 2.10.4 ARM64 dynarec register clobber (upstream GCC 12+ fix)\n' >> "$log"
    if ! patch --dry-run -d "$src" -p1 < "$vdp1_readback_patch" >/dev/null 2>> "$log"; then
      printf '\n[plumOS] required YabaSanshiro 2.10.4 VDP1 framebuffer-readback patch does not apply\n' >> "$log"
      return 1
    fi
    patch -d "$src" -p1 < "$vdp1_readback_patch" >> "$log" 2>&1
    printf '\n[plumOS] patched YabaSanshiro 2.10.4 complete VDP1 framebuffer readback\n' >> "$log"
  fi

  if [ "$id" = "scummvm" ]; then
    local audio_clock_patch="$patch_dir/scummvm-libretro-audio-clock.patch"

    if [ ! -f "$audio_clock_patch" ]; then
      printf '\n[plumOS] missing required ScummVM audio-clock patch\n' >> "$log"
      return 1
    fi
    if ! patch --dry-run -d "$src" -p1 < "$audio_clock_patch" >/dev/null 2>> "$log"; then
      printf '\n[plumOS] required ScummVM audio-clock patch does not apply\n' >> "$log"
      return 1
    fi
    patch -d "$src" -p1 < "$audio_clock_patch" >> "$log" 2>&1
    printf '\n[plumOS] patched ScummVM libretro continuous audio and fractional refresh clock\n' >> "$log"
  fi

  if [ "$id" = "neocd" ]; then
    local unibios_drive_patch="$patch_dir/neocd-unibios-toploader-drive.patch"

    if [ ! -f "$unibios_drive_patch" ]; then
      printf '\n[plumOS] missing required NeoCD UniBIOS drive patch\n' >> "$log"
      return 1
    fi
    if ! patch --dry-run -d "$src" -p1 < "$unibios_drive_patch" >/dev/null 2>> "$log"; then
      printf '\n[plumOS] required NeoCD UniBIOS drive patch does not apply\n' >> "$log"
      return 1
    fi
    patch -d "$src" -p1 < "$unibios_drive_patch" >> "$log" 2>&1
    printf '\n[plumOS] patched NeoCD UniBIOS 3.3 to use the Pixel2-compatible top-loader drive path\n' >> "$log"
  fi

  if [ "$id" = "mupen64plus_next" ]; then
    local powervr_buffer_patch="$patch_dir/mupen64plus-next-powervr-buffer-storage.patch"

    if [ ! -f "$powervr_buffer_patch" ]; then
      printf '\n[plumOS] missing required Mupen64Plus-Next PowerVR buffer-storage patch\n' >> "$log"
      return 1
    fi
    if ! patch --dry-run -d "$src" -p1 < "$powervr_buffer_patch" >/dev/null 2>> "$log"; then
      printf '\n[plumOS] required Mupen64Plus-Next PowerVR buffer-storage patch does not apply\n' >> "$log"
      return 1
    fi
    patch -d "$src" -p1 < "$powervr_buffer_patch" >> "$log" 2>&1
    printf '\n[plumOS] disabled broken persistent buffer storage on PowerVR for Mupen64Plus-Next\n' >> "$log"
  fi

  case "$id" in
    mgba)
      if [ -f "$src/src/core/CMakeLists.txt" ]; then
        sed -i -E '/^[[:space:]]+scripting\.c$/d' "$src/src/core/CMakeLists.txt"
        printf '\n[plumOS] patched mgba minimal libretro build to omit core scripting.c\n' >> "$log"
      fi
      if [ -f "$src/CMakeLists.txt" ]; then
        sed -i -E \
          -e 's|add_library\(\$\{BINARY_NAME\}_libretro SHARED \$\{CORE_SRC\} \$\{RETRO_SRC\} \$\{CORE_VFS_SRC\}\)|add_library(${BINARY_NAME}_libretro SHARED ${CORE_SRC} ${RETRO_SRC} ${CORE_VFS_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/src/util/vfs/vfs-fd.c)|' \
          "$src/CMakeLists.txt"
        printf '[plumOS] patched mgba libretro CMake source list for duplicate/missing VFS symbols\n' >> "$log"
      fi
      ;;
    quasi88)
      if [ -f "$patch_dir/quasi88-libretro-monitor-options.patch" ]; then
        if patch --dry-run -d "$src" -p1 < "$patch_dir/quasi88-libretro-monitor-options.patch" >/dev/null 2>> "$log"; then
          patch -d "$src" -p1 < "$patch_dir/quasi88-libretro-monitor-options.patch" >> "$log" 2>&1
          printf '\n[plumOS] patched quasi88 libretro monitor options\n' >> "$log"
        else
          printf '\n[plumOS] skipped quasi88 monitor-options patch: source already patched or layout does not match\n' >> "$log"
        fi
      fi
      ;;
    nekop2)
      if [ -f "$patch_dir/nekop2-libretro-joypad-keyboard.patch" ]; then
        if patch --dry-run -d "$src" -p1 < "$patch_dir/nekop2-libretro-joypad-keyboard.patch" >/dev/null 2>> "$log"; then
          patch -d "$src" -p1 < "$patch_dir/nekop2-libretro-joypad-keyboard.patch" >> "$log" 2>&1
          printf '\n[plumOS] patched nekop2 joypad-to-keyboard mapping\n' >> "$log"
        else
          printf '\n[plumOS] skipped nekop2 joypad patch: source already patched or layout does not match\n' >> "$log"
        fi
      fi
      ;;
    px68k)
      if [ -f "$patch_dir/px68k-libretro-uppercase-bios.patch" ]; then
        if patch --dry-run -d "$src" -p1 < "$patch_dir/px68k-libretro-uppercase-bios.patch" >/dev/null 2>> "$log"; then
          patch -d "$src" -p1 < "$patch_dir/px68k-libretro-uppercase-bios.patch" >> "$log" 2>&1
          printf '\n[plumOS] patched px68k uppercase BIOS filename fallbacks\n' >> "$log"
        else
          printf '\n[plumOS] skipped px68k uppercase BIOS patch: source already patched or layout does not match\n' >> "$log"
        fi
      fi
      ;;
    hatari)
      if [ -f "$patch_dir/hatari-libretro-skip-empty-media-options.patch" ]; then
        if patch --dry-run -d "$src" -p1 < "$patch_dir/hatari-libretro-skip-empty-media-options.patch" >/dev/null 2>> "$log"; then
          patch -d "$src" -p1 < "$patch_dir/hatari-libretro-skip-empty-media-options.patch" >> "$log" 2>&1
          printf '\n[plumOS] patched hatari to skip empty media command-line options\n' >> "$log"
        else
          printf '\n[plumOS] skipped hatari empty-media patch: source layout does not match\n' >> "$log"
        fi
      fi
      ;;
    km_duckswanstation_xtreme_amped)
      if [ -f "$src/CMakeLists.txt" ]; then
        sed -i \
          '/^# Enable LTO\/LTCG on Release builds\./,/^endif()/c\
# plumOS: disable forced SwanStation IPO/LTO for reproducible handheld feedback builds.' \
          "$src/CMakeLists.txt"
        printf '\n[plumOS] patched SwanStation CMake to disable forced IPO/LTO\n' >> "$log"
      fi
      ;;
    atari800)
      if [ -f "$patch_dir/atari800-libretro-audio-batch-pacing.patch" ]; then
        perl -0pi -e 's/\r\n/\n/g' "$src/libretro/core-mapper.c" "$src/libretro/libretro-core.c" 2>/dev/null || true
        if patch --dry-run -d "$src" -p1 < "$patch_dir/atari800-libretro-audio-batch-pacing.patch" >/dev/null 2>> "$log"; then
          patch -d "$src" -p1 < "$patch_dir/atari800-libretro-audio-batch-pacing.patch" >> "$log" 2>&1
          printf '\n[plumOS] patched atari800 libretro audio batching for frontend-driven timing\n' >> "$log"
        else
          printf '\n[plumOS] skipped atari800 audio patch: source already patched or layout does not match\n' >> "$log"
        fi
      fi
      ;;
    lutro)
      lua_makefile="$src/deps/lua/src/Makefile"
      if [ -f "$lua_makefile" ]; then
        sed -i -E \
          -e 's/^AR=[[:space:]]*ar rcu/AR= ar/' \
          -e 's/^\t\$\(AR\)[[:space:]]+\$@/\t$(AR) rcu $@/' \
          "$lua_makefile"
        printf '\n[plumOS] patched lutro Lua Makefile for command-line AR override\n' >> "$log"
      fi
      ;;
    fake08)
      if [ -f "$patch_dir/fake08-libretro-persistent-content-buffer.patch" ]; then
        if patch --dry-run -d "$src" -p1 < "$patch_dir/fake08-libretro-persistent-content-buffer.patch" >/dev/null 2>> "$log"; then
          patch -d "$src" -p1 < "$patch_dir/fake08-libretro-persistent-content-buffer.patch" >> "$log" 2>&1
          printf '\n[plumOS] patched fake08 libretro content buffer lifetime\n' >> "$log"
        else
          printf '\n[plumOS] skipped fake08 content buffer patch: source already patched or layout does not match\n' >> "$log"
        fi
      fi
      ;;
    mame2000)
      if [ -f "$src/Makefile" ]; then
        perl -0pi -e 's/ifneq \(\$\(ARM\), 1\)\s*\n\s*IS_X86 = 1\s*\nendif/ifneq ($(ARM), 1)\n   ifneq (,$(filter x86_64 i%86,$(shell uname -m)))\n      IS_X86 = 1\n   endif\nendif/s' "$src/Makefile"
        printf '\n[plumOS] patched mame2000 unix platform detection for non-x86 aarch64 builds\n' >> "$log"
      fi
      if [ -f "$src/src/libretro/osinline.h" ]; then
        sed -i -E 's/#if[[:space:]]*\\(IS_ARM\\)/#if (IS_ARM) \&\& !defined(__aarch64__)/g' "$src/src/libretro/osinline.h"
        printf '\n[plumOS] patched mame2000 ARM inline vector multiply guard for aarch64\n' >> "$log"
      fi
      ;;
    parallel_n64)
      if [ -f "$patch_dir/parallel-n64-knulli-a133.patch" ]; then
        if patch --dry-run -d "$src" -p1 < "$patch_dir/parallel-n64-knulli-a133.patch" >/dev/null 2>> "$log"; then
          patch -d "$src" -p1 < "$patch_dir/parallel-n64-knulli-a133.patch" >> "$log" 2>&1
          printf '\n[plumOS] patched Parallel-N64 with the A133/H5 AArch64 GLES target\n' >> "$log"
        else
          printf '\n[plumOS] required Parallel-N64 A133/H5 patch does not apply\n' >> "$log"
          return 1
        fi
      else
        printf '\n[plumOS] missing required Parallel-N64 A133/H5 patch\n' >> "$log"
        return 1
      fi
      while IFS= read -r lua_makefile; do
        [ -f "$lua_makefile" ] || continue
        sed -i -E \
          -e 's/[[:space:]]-flto(=[^[:space:]]+)?//g' \
          -e 's/[[:space:]]-fwhole-program//g' \
          -e 's/[[:space:]]-fuse-linker-plugin//g' \
          "$lua_makefile"
      done <<EOF_PARALLEL_N64_LTO
$(find "$src" -maxdepth 4 -type f \( \
  -name 'Makefile' -o \
  -name 'Makefile.*' -o \
  -name 'makefile' -o \
  -name 'makefile.*' -o \
  -name '*.mk' -o \
  -name '*.mak' \
\) -print)
EOF_PARALLEL_N64_LTO
      printf '\n[plumOS] patched LTO-sensitive Makefiles for native Pixel2 feedback builds\n' >> "$log"
      ;;
    mednafen_ngp|beetle_saturn|flycast|flycast_xtreme|mupen64plus_next|yabasanshiro)
      while IFS= read -r lua_makefile; do
        [ -f "$lua_makefile" ] || continue
        sed -i -E \
          -e 's/[[:space:]]-flto(=[^[:space:]]+)?//g' \
          -e 's/[[:space:]]-fwhole-program//g' \
          -e 's/[[:space:]]-fuse-linker-plugin//g' \
          "$lua_makefile"
      done <<EOF_HIGH_END_LTO
$(find "$src" -maxdepth 4 -type f \( \
  -name 'Makefile' -o \
  -name 'Makefile.*' -o \
  -name 'makefile' -o \
  -name 'makefile.*' -o \
  -name '*.mk' -o \
  -name '*.mak' \
\) -print)
EOF_HIGH_END_LTO
      printf '\n[plumOS] patched LTO-sensitive Makefiles for native Pixel2 feedback builds\n' >> "$log"
      ;;
    easyrpg)
      if [ -f "$src/builds/libretro/CMakeLists.txt" ]; then
        sed -i -E \
          -e 's/include\(ConfigureWindows\)/include(PlayerConfigureWindows OPTIONAL)/' \
          "$src/builds/libretro/CMakeLists.txt"
        printf '\n[plumOS] patched EasyRPG libretro CMake to avoid a Windows-only include on Linux\n' >> "$log"
      fi
      ;;
    tic80)
      if [ -f "$src/core/vendor/zip/CMakeLists.txt" ]; then
        sed -i -E \
          -e 's/cmake_minimum_required\(VERSION 3\.14\)/cmake_minimum_required(VERSION 3.13)/' \
          "$src/core/vendor/zip/CMakeLists.txt"
        printf '\n[plumOS] patched tic80 vendored zip CMake minimum\n' >> "$log"
      fi
      ;;
  esac
}

cmake_build_command() {
  local source_dir="$1"
  local build_dir="$2"
  local target="$3"
  local build_type="$4"
  local log="$5"
  shift 5

  (
    rm -rf "$build_dir"
    env CC="$CC" CXX="$CXX" AR="$AR" RANLIB="$RANLIB" \
      cmake -S "$source_dir" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DCMAKE_C_FLAGS="$COMMON_CFLAGS" \
        -DCMAKE_CXX_FLAGS="$COMMON_CXXFLAGS" \
        -DCMAKE_SHARED_LINKER_FLAGS="$COMMON_LDFLAGS" \
        "$@"
    cmake --build "$build_dir" --parallel "$JOBS_ACTIVE" --target "$target"
  ) >> "$log" 2>&1
}

stage_core_runtime_deps() {
  local elf="$1"
  local path soname target

  while IFS= read -r path; do
    [ -f "$path" ] || continue
    soname="$(basename "$path")"
    case "$soname" in
      ld-linux-aarch64.so.1|libc.so.6|libm.so.6|libpthread.so.0|libdl.so.2|librt.so.1|\
      libEGL.so.*|libGLESv2.so.*|libGL.so.*|libMali.so.*|libSDL2-2.0.so.*)
        continue
        ;;
    esac

    target="$OUT_DIR/lib/libretro/$soname"
    if [ ! -f "$target" ]; then
      cp -L "$path" "$target"
      stage_core_runtime_deps "$target"
    fi
  done < <(
    ldd "$elf" 2>/dev/null |
      awk '/=> \/[^ ]+/ {print $3} /^[[:space:]]*\// {print $1}' |
      sort -u
  )
}

build_inih_for_easyrpg() {
  local src="$1"
  local log="$2"
  local inih_dir="$src/lib/inih"

  rm -rf "$inih_dir"
  mkdir -p "$(dirname "$inih_dir")"
  git clone --depth 1 https://github.com/benhoyt/inih.git "$inih_dir" >> "$log" 2>&1 || return 1
  (
    cd "$inih_dir"
    env CC="$CC" CFLAGS="$COMMON_CFLAGS -fPIC" "$CC" -c ini.c -o ini.o
    "$AR" rcs libinih.a ini.o
    "$RANLIB" libinih.a
  ) >> "$log" 2>&1
}

prepare_liblcf_for_easyrpg() {
  local src="$1"
  local log="$2"
  local liblcf_dir="$src/lib/liblcf"
  local liblcf_ref="${EASYRPG_LIBLCF_REF:-abc215345ba962a031f2b8c645f4357cf1bece85}"

  rm -rf "$liblcf_dir"
  mkdir -p "$(dirname "$liblcf_dir")"
  git clone --depth 1 https://github.com/EasyRPG/liblcf.git "$liblcf_dir" >> "$log" 2>&1 || return 1
  (
    cd "$liblcf_dir"
    git fetch --depth 1 origin "$liblcf_ref"
    git checkout --detach FETCH_HEAD
  ) >> "$log" 2>&1
}

run_scummvm_build() {
  local src="$1"
  local log="$2"

  (
    cd "$src/backends/platform/libretro"
    env CC="$CC" CXX="$CXX" AR="$AR" RANLIB="$RANLIB" \
      CFLAGS="$COMMON_CFLAGS" CXXFLAGS="$COMMON_CXXFLAGS" LDFLAGS="$COMMON_LDFLAGS" \
      make clean >/dev/null 2>&1 || true
    env CC="$CC" CXX="$CXX" AR="$AR" RANLIB="$RANLIB" \
      CFLAGS="$COMMON_CFLAGS" CXXFLAGS="$COMMON_CXXFLAGS" LDFLAGS="$COMMON_LDFLAGS" \
      make -j"$JOBS_ACTIVE" \
        platform=unix \
        LITE=1 \
        NO_WIP=1 \
        FORCE_OPENGLNONE=1 \
        USE_MT32EMU= \
        USE_VORBIS= \
        USE_THEORADEC= \
        USE_FLUIDSYNTH= \
        USE_FREETYPE2= \
        USE_MPEG2= \
        USE_IMGUI=
  ) >> "$log" 2>&1
}

make_build_command() {
  local work="$1"
  local makefile="$2"
  local log="$3"
  local commit="$4"
  shift 4

  (
    cd "$work"
    make -f "$makefile" clean >/dev/null 2>&1 || true
    env \
      CC="$CC" \
      CXX="$CXX" \
      AS="$AS" \
      AR="$AR" \
      RANLIB="$RANLIB" \
      CFLAGS="$COMMON_CFLAGS" \
      CXXFLAGS="$COMMON_CXXFLAGS" \
      LDFLAGS="$COMMON_LDFLAGS" \
      make -f "$makefile" -j"$JOBS_ACTIVE" "$@" \
        CC="$CC" CXX="$CXX" AS="$AS" AR="$AR" RANLIB="$RANLIB" \
        GIT_VERSION=-"$(printf '%s' "$commit" | cut -c 1-7)"
  ) >> "$log" 2>&1
}

build_special_core() {
  local id="$1"
  local src="$2"
  local log="$3"
  local tic80_src

  case "$id" in
    mgba)
      run_with_job_retry "$id" "$log" cmake_build_command "$src" "$src/build-libretro" mgba_libretro Release "$log" \
        -DBUILD_LIBRETRO=ON \
        -DBUILD_SDL=OFF \
        -DBUILD_QT=OFF \
        -DBUILD_GL=OFF \
        -DBUILD_GLES2=OFF \
        -DBUILD_PGO=OFF \
        -DBUILD_LTO=OFF \
        -DBUILD_PERF=OFF \
        -DBUILD_TEST=OFF \
        -DBUILD_DOCGEN=OFF \
        -DENABLE_SCRIPTING=OFF \
        -DUSE_LUA=OFF \
        -DUSE_JSON_C=OFF \
        -DUSE_FREETYPE=OFF \
        -DUSE_FFMPEG=OFF \
        -DUSE_DISCORD_RPC=OFF \
        -DUSE_EDITLINE=OFF
      ;;
    tic80)
      tic80_src="$src"
      [ ! -f "$src/core/CMakeLists.txt" ] || tic80_src="$src/core"
      run_with_job_retry "$id" "$log" cmake_build_command "$tic80_src" "$src/build-libretro" tic80_libretro MinSizeRel "$log" \
        -DBUILD_STATIC=ON \
        -DBUILD_PLAYER=OFF \
        -DBUILD_SDL=OFF \
        -DBUILD_SDLGPU=OFF \
        -DBUILD_TOOLS=OFF \
        -DBUILD_LIBRETRO=ON \
        -DBUILD_WITH_MRUBY=OFF
      ;;
    easyrpg)
      prepare_liblcf_for_easyrpg "$src" "$log" || return 1
      build_inih_for_easyrpg "$src" "$log" || return 1
      run_with_job_retry "$id" "$log" cmake_build_command "$src" "$src/build-libretro" easyrpg_libretro Release "$log" \
        -DPLAYER_TARGET_PLATFORM=libretro \
        -DPLAYER_BUILD_LIBLCF=ON \
        -DINIH_INCLUDE_DIR="$src/lib/inih" \
        -DINIH_LIBRARY="$src/lib/inih/libinih.a" \
        -DLIBLCF_WITH_ICU=ON \
        -DLIBLCF_WITH_XML=ON \
        -DLIBLCF_ENABLE_TOOLS=OFF \
        -DLIBLCF_ENABLE_TESTS=OFF \
        -DLIBLCF_ENABLE_BENCHMARKS=OFF \
        -DPLAYER_ENABLE_TESTS=OFF \
        -DPLAYER_ENABLE_BENCHMARKS=OFF \
        -DBUILD_SHARED_LIBS=ON \
        -DPLAYER_WITH_FREETYPE=ON \
        -DPLAYER_WITH_HARFBUZZ=ON \
        -DPLAYER_WITH_MPG123=ON \
        -DPLAYER_WITH_LIBSNDFILE=ON \
        -DPLAYER_WITH_OGGVORBIS=ON \
        -DPLAYER_WITH_OPUS=ON \
        -DPLAYER_WITH_WILDMIDI=OFF \
        -DPLAYER_WITH_FLUIDSYNTH=OFF \
        -DPLAYER_WITH_FLUIDLITE=OFF \
        -DPLAYER_WITH_XMP=ON \
        -DPLAYER_WITH_NATIVE_MIDI=OFF \
        -DPLAYER_ENABLE_FMMIDI=ON \
        -DPLAYER_ENABLE_DRWAV=ON \
        -DPLAYER_WITH_SPEEXDSP=ON \
        -DPLAYER_WITH_SAMPLERATE=ON
      ;;
    scummvm)
      run_with_job_retry "$id" "$log" run_scummvm_build "$src" "$log"
      ;;
    squirreljme)
      [ -f "$src/nanocoat/CMakeLists.txt" ] || return 99
      run_with_job_retry "$id" "$log" cmake_build_command "$src/nanocoat" "$src/nanocoat/build-libretro" squirreljme_libretro Release "$log" \
        -DSQUIRRELJME_ENABLE_FRONTEND_LIBRETRO=ON \
        -DSQUIRRELJME_ENABLE_FRONTEND_JRI=OFF \
        -DSQUIRRELJME_ENABLE_DYLIB=ON \
        -DSQUIRRELJME_ENABLE_PACKING=OFF \
        -DLIBRETRO_REALLY_STATIC=OFF
      ;;
    *)
      return 99
      ;;
  esac
}

copy_core_info() {
  local base="$1"
  local src="$2"
  local info=""

  if [ "$base" = "flycast_xtreme_libretro" ] &&
     [ -f "$SRC_ROOT/core-info/flycast_libretro.info" ]; then
    info="$SRC_ROOT/core-info/flycast_libretro.info"
  elif [ -f "$SRC_ROOT/core-info/$base.info" ]; then
    info="$SRC_ROOT/core-info/$base.info"
  else
    info="$(find "$src" -type f -name "$base.info" | sort | head -n 1 || true)"
  fi
  if [ -n "$info" ] && [ -f "$info" ]; then
    cp "$info" "$OUT_DIR/info/$base.info"
    if [ "$base" = "flycast_xtreme_libretro" ]; then
      sed -i \
        -e 's/^display_name = .*/display_name = "Sega - Dreamcast\/NAOMI (Flycast Xtreme)"/' \
        -e 's/^corename = .*/corename = "Flycast Xtreme"/' \
        "$OUT_DIR/info/$base.info"
    fi
  fi
}

stage_outputs() {
  local id="$1"
  local src="$2"
  local count=0
  local so
  local source_base
  local base
  local stem
  local alias

  while IFS= read -r so; do
    [ -n "$so" ] || continue
    if ! "$READELF" -h "$so" >/dev/null 2>&1; then
      continue
    fi
    if ! "$READELF" -h "$so" 2>/dev/null |
         grep -Eq 'Machine:[[:space:]]*(AArch64|ARM aarch64)'; then
      msg "ignoring non-AArch64 output for $id: $so"
      continue
    fi
    source_base="$(basename "$so")"
    base="$source_base"
    case "$base" in
      *_libretro.dll) base="${base%.dll}.so" ;;
    esac
    base="$(core_stage_base "$id" "$base")"
    stem="${base%.so}"
    cp "$so" "$OUT_DIR/cores/$base"
    "$STRIP" "$OUT_DIR/cores/$base" >/dev/null 2>&1 || true
    copy_core_info "$stem" "$src"
    append_manifest "  output=cores/$base"
    if [ "$source_base" != "$base" ]; then
      append_manifest "  source_output=$source_base"
    fi
    for alias in $(core_output_aliases "$id" "$base"); do
      [ ! -f "$OUT_DIR/cores/$alias" ] || continue
      cp "$OUT_DIR/cores/$base" "$OUT_DIR/cores/$alias"
      copy_core_info "${alias%.so}" "$src"
      append_manifest "  alias_output=cores/$alias"
      append_manifest "  alias_of=cores/$base"
    done
    count=$((count + 1))
  done <<EOF_STAGE
$(find "$src" -type f \( -name '*_libretro.so' -o -name '*_libretro.dll' \) | sort)
EOF_STAGE

  [ "$count" -gt 0 ]
}

stage_license_file() {
  local id="$1"
  local src="$2"
  local file
  local destination

  mkdir -p "$OUT_DIR/licenses"
  for file in \
      COPYING COPYING.txt COPYING.md Copying \
      LICENSE LICENSE.txt LICENSE.md LICENSES UNLICENSE License.txt license.txt license.mkd \
      copyright; do
    if [ -f "$src/$file" ]; then
      destination="$OUT_DIR/licenses/${id}-${file//\//_}"
      cp "$src/$file" "$destination"
      append_manifest "license=${destination#"$OUT_DIR"/}"
      return 0
    fi
  done

  file="$(find "$src" -mindepth 2 -maxdepth 4 -type f \
      \( -iname 'COPYING*' -o -iname 'LICENSE*' -o -iname 'UNLICENSE' -o -iname 'copyright' \
         -o -iname 'GPL*' -o -iname 'LGPL*' \) \
      | sort | sed -n '1p')"
  if [ -n "$file" ]; then
    destination="$OUT_DIR/licenses/${id}-$(basename "$file")"
    cp "$file" "$destination"
    append_manifest "license=${destination#"$OUT_DIR"/}"
    append_manifest "license_source=${file#"$src"/}"
    return 0
  fi

  for file in README README.txt README.md readme.txt readme.md; do
    if [ -f "$src/$file" ] && grep -Eiq \
        'permission is hereby granted|redistribution and use|gnu (lesser )?general public licen[cs]e|licensed under|source form, for non-commercial|public domain|licen[cs]e agreement' \
        "$src/$file"; then
      destination="$OUT_DIR/licenses/${id}-${file}"
      cp "$src/$file" "$destination"
      append_manifest "license=${destination#"$OUT_DIR"/}"
      return 0
    fi
  done

  case "$id" in
    daphne) file="daphne/daphne-1.0-src/daphne.h" ;;
    snes9x2002) file="src/soundux.c" ;;
    *) file= ;;
  esac
  if [ -n "$file" ] && [ -f "$src/$file" ]; then
    destination="$OUT_DIR/licenses/${id}-license-bearing-$(basename "$file")"
    cp "$src/$file" "$destination"
    append_manifest "license=${destination#"$OUT_DIR"/}"
    append_manifest "license_source=$file"
    return 0
  fi

  # libretro-uzem carries its complete MIT text in the primary source file.
  if [ "$id" = uzem ] && [ -f "$src/uzem_libretro.cpp" ]; then
    destination="$OUT_DIR/licenses/uzem-license-bearing-source.cpp"
    cp "$src/uzem_libretro.cpp" "$destination"
    append_manifest "license=${destination#"$OUT_DIR"/}"
    return 0
  fi

  append_manifest "license=not-found-in-source-root"
  return 1
}

build_one_core() {
  local id="$1"
  local class="$2"
  local repo="$3"
  local ref="$4"
  local subdir="$5"
  local makefile_hint="$6"
  local make_args="$7"
  local src="$SRC_ROOT/$id"
  local work
  local log="$LOG_DIR_ABS/$id.log"
  local makefile
  local commit
  local special_status
  local LAST_SUCCESSFUL_JOBS=""
  local CC="$CC"
  local CXX="$CXX"
  local AS="$AS"

  if ! core_selected "$id" "$class"; then
    SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
    return 0
  fi

  : > "$log"
  append_manifest ""
  append_manifest "[$id]"
  append_manifest "class=$class"
  append_manifest "repo=$repo"
  append_manifest "ref=$ref"
  append_manifest "log=logs/$id.log"

  if [ "$id" = "yabasanshiro" ]; then
    CC="$YABASANSHIRO_CC"
    CXX="$YABASANSHIRO_CXX"
    AS="$YABASANSHIRO_AS"
    append_manifest "compiler=$CC"
    append_manifest "cxx_compiler=$CXX"
    if ! command -v "$CC" >/dev/null 2>&1 || ! command -v "$CXX" >/dev/null 2>&1; then
      msg "FAILED $id: required compiler missing ($CC/$CXX)"
      append_manifest "status=failed"
      append_manifest "reason=compiler_missing"
      FAILED_COUNT=$((FAILED_COUNT + 1))
      return 0
    fi
  fi

  if ! clone_or_update_repo "$id" "$repo" "$ref" "$src" "$log"; then
    msg "FAILED clone $id"
    append_manifest "status=failed"
    append_manifest "reason=clone_failed"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi

  commit="$(git -C "$src" rev-parse HEAD 2>/dev/null || printf unknown)"
  append_manifest "commit=$commit"
  patch_core_source "$id" "$src" "$log"
  stage_license_file "$id" "$src" || true

  if build_special_core "$id" "$src" "$log"; then
    special_status=0
  else
    special_status=$?
  fi
  if [ "$special_status" -eq 0 ]; then
    append_manifest "builder=special"
    append_manifest "jobs=${LAST_SUCCESSFUL_JOBS:-$JOBS}"
    if stage_outputs "$id" "$src"; then
      append_manifest "status=built"
      BUILT_COUNT=$((BUILT_COUNT + 1))
      msg "built $id"
    else
      msg "FAILED $id: no *_libretro.so output"
      append_manifest "status=failed"
      append_manifest "reason=no_output"
      FAILED_COUNT=$((FAILED_COUNT + 1))
    fi
    return 0
  elif [ "$special_status" -ne 99 ]; then
    msg "FAILED special build $id"
    append_manifest "builder=special"
    append_manifest "status=failed"
    append_manifest "reason=special_build_failed"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi

  work="$src"
  if [ -n "$subdir" ]; then
    work="$src/$subdir"
  fi
  if [ ! -d "$work" ]; then
    msg "FAILED $id: missing subdir $subdir"
    append_manifest "status=failed"
    append_manifest "reason=missing_subdir:$subdir"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi
  if ! makefile="$(find_makefile "$work" "$makefile_hint")"; then
    msg "FAILED $id: no libretro makefile"
    append_manifest "status=failed"
    append_manifest "reason=no_makefile"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi

  append_manifest "makefile=${subdir:+$subdir/}$makefile"
  append_manifest "make_args=$make_args"
  append_manifest "jobs=$JOBS"

  read -r -a args <<< "$make_args"
  if [ "$makefile" = "CMakeLists.txt" ]; then
    local cmake_build_dir="$src/.plumos-cmake-build"
    local cmake_target
    local cmake_args=()

    cmake_target="$(cmake_target_from_args "$id")"
    while IFS= read -r arg; do
      [ -n "$arg" ] || continue
      cmake_args+=("$arg")
    done <<EOF_CMAKE_ARGS
$(cmake_args_without_internal_keys)
EOF_CMAKE_ARGS

    append_manifest "build_system=cmake"
    append_manifest "cmake_target=$cmake_target"
    if ! run_with_job_retry "$id" "$log" cmake_build_command "$work" "$cmake_build_dir" "$cmake_target" Release "$log" "${cmake_args[@]}"; then
      msg "FAILED build $id"
      append_manifest "status=failed"
      append_manifest "reason=cmake_build_failed"
      FAILED_COUNT=$((FAILED_COUNT + 1))
      return 0
    fi
  else
    append_manifest "build_system=make"
    if ! run_with_job_retry "$id" "$log" make_build_command "$work" "$makefile" "$log" "$commit" "${args[@]}"; then
      msg "FAILED build $id"
      append_manifest "status=failed"
      append_manifest "reason=build_failed"
      FAILED_COUNT=$((FAILED_COUNT + 1))
      return 0
    fi
  fi

  if stage_outputs "$id" "$src"; then
    append_manifest "status=built"
    BUILT_COUNT=$((BUILT_COUNT + 1))
    msg "built $id"
  else
    msg "FAILED $id: no *_libretro.so output"
    append_manifest "status=failed"
    append_manifest "reason=no_output"
    FAILED_COUNT=$((FAILED_COUNT + 1))
  fi
}

stage_existing_core() {
  local id="$1"
  local class="$2"
  local repo="$3"
  local ref="$4"
  local src="$SRC_ROOT/$id"
  local commit=unknown

  if ! core_selected "$id" "$class"; then
    SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
    return 0
  fi

  append_manifest ""
  append_manifest "[$id]"
  append_manifest "class=$class"
  append_manifest "repo=$repo"
  append_manifest "ref=$ref"
  append_manifest "mode=stage-existing"
  if [ -d "$src/.git" ]; then
    commit="$(git -C "$src" rev-parse HEAD 2>/dev/null || printf unknown)"
  fi
  append_manifest "commit=$commit"

  if [ ! -d "$src" ]; then
    msg "FAILED $id: existing source tree is missing"
    append_manifest "status=failed"
    append_manifest "reason=source_missing"
    FAILED_COUNT=$((FAILED_COUNT + 1))
    return 0
  fi
  stage_license_file "$id" "$src" || true
  if stage_outputs "$id" "$src"; then
    append_manifest "status=staged-existing"
    BUILT_COUNT=$((BUILT_COUNT + 1))
    msg "staged existing $id"
  else
    msg "FAILED $id: no existing AArch64 libretro output"
    append_manifest "status=failed"
    append_manifest "reason=existing_output_missing"
    FAILED_COUNT=$((FAILED_COUNT + 1))
  fi
}

if [ ! -f "$CORE_RECIPES" ]; then
  printf 'error: recipe file not found: %s\n' "$CORE_RECIPES" >&2
  exit 1
fi

if [ "$LIST_ONLY" = "1" ]; then
  while IFS='|' read -r id class repo ref subdir makefile make_args; do
    if core_selected "$id" "$class"; then
      printf '%s\t%s\t%s\t%s\n' "$id" "$class" "$ref" "$make_args"
    fi
  done <<EOF_LIST
$(core_table)
EOF_LIST
  exit 0
fi

rm -rf "$OUT_ROOT"
mkdir -p "$OUT_DIR/cores" "$OUT_DIR/info" "$OUT_DIR/lib/libretro" "$OUT_DIR/logs" "$OUT_DIR/licenses" "$(dirname "$SRC_ROOT")"
LOG_DIR="$OUT_DIR/logs"
LOG_DIR_ABS="$(CDPATH= cd -- "$LOG_DIR" && pwd)"
MANIFEST="$OUT_DIR/libretro-cores.manifest"
BUILT_COUNT=0
FAILED_COUNT=0
SKIPPED_COUNT=0

{
  printf 'name=plumOS Pixel2 libretro cores\n'
  printf 'generated_at=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
  printf 'target=pixel2-aarch64-linux-gnu\n'
  printf 'recipes=%s\n' "$CORE_RECIPES"
  printf 'filter=%s\n' "$PLUMOS_CORE_FILTER"
  printf 'cc=%s\n' "$CC"
  printf 'cxx=%s\n' "$CXX"
  printf 'common_cflags=%s\n' "$COMMON_CFLAGS"
  printf 'common_cxxflags=%s\n' "$COMMON_CXXFLAGS"
  printf 'common_ldflags=%s\n' "$COMMON_LDFLAGS"
  printf 'jobs=%s\n' "$JOBS"
  printf 'job_fallbacks=%s\n' "$BUILD_JOB_FALLBACKS"
  printf 'serial_cores=%s\n' "$LIBRETRO_SERIAL_CORES"
  if [ "$STAGE_EXISTING" -eq 1 ]; then
    printf 'mode=stage-existing\n'
  else
    printf 'mode=build\n'
  fi
} > "$MANIFEST"

core_info_log="$LOG_DIR_ABS/core-info.log"
: > "$core_info_log"
if [ "$STAGE_EXISTING" -eq 1 ] && [ -d "$SRC_ROOT/core-info" ]; then
  append_manifest ""
  append_manifest "[core-info]"
  append_manifest "repo=$CORE_INFO_REPO"
  append_manifest "ref=$CORE_INFO_REF"
  append_manifest "commit=$(git -C "$SRC_ROOT/core-info" rev-parse HEAD 2>/dev/null || printf unknown)"
  append_manifest "status=existing"
elif clone_or_update_repo core-info "$CORE_INFO_REPO" "$CORE_INFO_REF" "$SRC_ROOT/core-info" "$core_info_log"; then
  append_manifest ""
  append_manifest "[core-info]"
  append_manifest "repo=$CORE_INFO_REPO"
  append_manifest "ref=$CORE_INFO_REF"
  append_manifest "commit=$(git -C "$SRC_ROOT/core-info" rev-parse HEAD 2>/dev/null || printf unknown)"
  append_manifest "log=logs/core-info.log"
  append_manifest "status=available"
else
  append_manifest ""
  append_manifest "[core-info]"
  append_manifest "status=failed"
  append_manifest "reason=clone_failed"
  append_manifest "log=logs/core-info.log"
fi

while IFS='|' read -r id class repo ref subdir makefile make_args; do
  if [ "$STAGE_EXISTING" -eq 1 ]; then
    stage_existing_core "$id" "$class" "$repo" "$ref"
  else
    build_one_core "$id" "$class" "$repo" "$ref" "$subdir" "$makefile" "$make_args"
  fi
done <<EOF_CORES
$(core_table)
EOF_CORES

if [ -f "$OUT_DIR/cores/easyrpg_libretro.so" ]; then
  easyrpg_core="$OUT_DIR/cores/easyrpg_libretro.so"
  if unresolved_deps="$(ldd "$easyrpg_core" 2>/dev/null | awk '/not found/ {print $1}')" &&
     [ -n "$unresolved_deps" ]; then
    printf 'error: EasyRPG runtime dependencies are unresolved: %s\n' "$unresolved_deps" >&2
    if [ "$FAIL_ON_CORE_ERROR" = "1" ]; then
      exit 1
    fi
  else
    stage_core_runtime_deps "$easyrpg_core"
    append_manifest ""
    append_manifest "[easyrpg-runtime]"
    append_manifest "audio=mpg123,sndfile,vorbis,opus,xmp,drwav,fmmidi"
    append_manifest "text=freetype,harfbuzz,icu"
    append_manifest "data=xml"
    append_manifest "resampler=speexdsp,samplerate"
    append_manifest "native_midi=disabled-libretro-audio-ownership"
    append_manifest "runtime_dir=lib/libretro"
    append_manifest "status=staged"
    while IFS= read -r runtime_lib; do
      append_manifest "runtime_lib=${runtime_lib#"$OUT_DIR"/}"
    done < <(find "$OUT_DIR/lib/libretro" -maxdepth 1 -type f | sort)
  fi
  if [ ! -f "$OUT_DIR/lib/libretro/libfmt.so.9" ] && [ "$FAIL_ON_CORE_ERROR" = "1" ]; then
    printf 'error: EasyRPG runtime dependency was not staged: libfmt.so.9\n' >&2
    exit 1
  fi
fi

append_manifest ""
append_manifest "[summary]"
append_manifest "built=$BUILT_COUNT"
append_manifest "failed=$FAILED_COUNT"
append_manifest "skipped=$SKIPPED_COUNT"

find "$OUT_DIR" -type f ! -name checksums.sha256 | sort | while IFS= read -r file; do
  rel="${file#"$OUT_DIR"/}"
  sha256sum "$file" | awk -v rel="$rel" '{print $1 "  " rel}'
done > "$OUT_DIR/checksums.sha256"

mkdir -p "$PLUMOS_DIR" "$COMPONENT_DIR"
rsync -a \
  --exclude='checksums.sha256' \
  "$OUT_DIR/" "$PLUMOS_DIR/"

core_json="$OUT_ROOT/core-manifest.jsonl"
: > "$core_json"
while IFS= read -r core_file; do
  base="$(basename "$core_file")"
  id="${base%_libretro.so}"
  row="$(awk -F'|' -v id="$id" '$1 == id { print; exit }' "$CORE_RECIPES")"
  class="$(printf '%s' "$row" | awk -F'|' '{ print $2 }')"
  repo="$(printf '%s' "$row" | awk -F'|' '{ print $3 }')"
  ref="$(printf '%s' "$row" | awk -F'|' '{ print $4 }')"
  rendering="software"
  case "$id" in
    flycast|flycast_xtreme|km_duckswanstation_xtreme_amped|mupen64plus_next|parallel_n64|yabasanshiro)
      rendering="hardware-gles"
      ;;
  esac
  jq -n -c \
    --arg id "$id" \
    --arg class "$class" \
    --arg rendering "$rendering" \
    --arg upstream "$repo" \
    --arg upstream_commit "$ref" \
    --arg binary "cores/$base" \
    '{id:$id,class:$class,rendering:$rendering,upstream:$upstream,upstream_commit:$upstream_commit,binary:$binary}' \
    >> "$core_json"
done < <(find "$PLUMOS_DIR/cores" -maxdepth 1 -type f -name '*_libretro.so' | sort)

jq -s \
  --arg source_ref "$(git -C "$ROOT_DIR" rev-parse --short HEAD)" \
  --arg filter "$PLUMOS_CORE_FILTER" \
  --argjson built "$BUILT_COUNT" \
  --argjson failed "$FAILED_COUNT" \
  --argjson skipped "$SKIPPED_COUNT" \
  '{
    name: "plumOS Pixel2 libretro cores",
    component: "libretro-cores",
    device: "pixel2",
    architecture: "aarch64",
    source_ref: $source_ref,
    filter: $filter,
    built: $built,
    failed: $failed,
    skipped: $skipped,
    cores: .
  }' "$core_json" > "$COMPONENT_DIR/manifest.json"
rm -f "$core_json"

(
  cd "$PLUMOS_DIR"
  find cores info lib licenses components/libretro-cores -type f \
    ! -path 'components/libretro-cores/checksums.sha256' \
    -print |
    sort |
    while IFS= read -r file; do sha256sum "$file"; done
) > "$COMPONENT_DIR/checksums.sha256"
(
  cd "$PLUMOS_DIR"
  sha256sum -c components/libretro-cores/checksums.sha256 >/dev/null
)

printf 'created: %s\n' "$PLUMOS_DIR"
printf 'built: %s\n' "$BUILT_COUNT"
printf 'failed: %s\n' "$FAILED_COUNT"
printf 'skipped: %s\n' "$SKIPPED_COUNT"

if [ "$FAILED_COUNT" -gt 0 ] && [ "$FAIL_ON_CORE_ERROR" = "1" ]; then
  exit 1
fi
