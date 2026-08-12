#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if [ "${1:-}" != --inside ]; then
    exec "$ROOT_DIR/scripts/docker-build.sh" picoarch "$@"
fi
shift

ROOT_DIR=/work
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
PICOARCH_REPO="${PICOARCH_REPO:-https://github.com/shauninman/picoarch.git}"
PICOARCH_REF="${PICOARCH_REF:-802047c276a5a931b0bf837c4ea4b8e238bdeabe}"
SDL12_REPO="${SDL12_REPO:-https://github.com/libsdl-org/sdl12-compat.git}"
SDL12_REF="${SDL12_REF:-fc2ec0c128197f1f5050e48359bc41e618f3abfb}"
SRC_ROOT="${PLUMOS_PIXEL2_PICOARCH_SRC:-$ROOT_DIR/output/build/picoarch-pixel2}"
SRC="$SRC_ROOT/picoarch"
SDL12_SRC="$SRC_ROOT/sdl12-compat"
OUT_ROOT="${PLUMOS_PIXEL2_PICOARCH_OUT:-$ROOT_DIR/output/picoarch/pixel2}"
PLUMOS_DIR="$OUT_ROOT/plumos"
COMPONENT_DIR="$PLUMOS_DIR/components/picoarch"
PATCH_DIR="$ROOT_DIR/docker/pixel2-tools/picoarch"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"

checkout_source() {
  local repo="$1" ref="$2" dst="$3"
  if [ ! -d "$dst/.git" ]; then
    rm -rf "$dst"
    git clone "$repo" "$dst"
  fi
  git -C "$dst" fetch --depth 1 origin "$ref"
  git -C "$dst" reset --hard FETCH_HEAD
  git -C "$dst" clean -ffdx
}

apply_once() {
  local patch_file="$1"
  if git -C "$SRC" apply --check "$patch_file" >/dev/null 2>&1; then
    git -C "$SRC" apply "$patch_file"
  elif ! git -C "$SRC" apply --reverse --check "$patch_file" >/dev/null 2>&1; then
    printf 'error: PicoArch patch does not apply: %s\n' "$patch_file" >&2
    exit 1
  fi
}

mkdir -p "$SRC_ROOT"
checkout_source "$PICOARCH_REPO" "$PICOARCH_REF" "$SRC"
git -C "$SRC" submodule update --init --recursive
git -C "$SRC" submodule foreach --recursive 'git reset --hard; git clean -ffdx'
checkout_source "$SDL12_REPO" "$SDL12_REF" "$SDL12_SRC"

perl -0pi -e 's/scaler_neon\.o/scaler_c.o picoarch_pixel2_host.o/ or die "scaler object marker missing\n";
  s/-lpng12/-lpng/ or die "libpng marker missing\n";
  s/else ifeq \(\$\(platform\), unix\)/else ifeq (\$(platform), pixel2)\n\tOBJS += plat_linux.o\n\tCFLAGS += -march=armv8-a+crc -mtune=cortex-a35 -pthread -DCONTENT_DIR='"'"'"\/roms"'"'"'\n\tLDFLAGS += -fPIE -pthread\nelse ifeq (\$(platform), unix)/ or die "platform marker missing\n"' "$SRC/Makefile"

awk 'BEGIN { n=0 } /^ifeq \(\$\(platform\), trimui\)/ { n++; if (n == 2) exit } { print }' \
  "$SRC/Makefile" > "$SRC/Makefile.pixel2"
mv "$SRC/Makefile.pixel2" "$SRC/Makefile"

{
  printf '%s\n' '#include <stdint.h>' '#include <string.h>' '#include "scaler_neon.h"'
  for scale in 1 2 3 4 5 6; do
    for depth in 16 32; do
      printf 'void scale%sx_n%s(void *src, void *dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dp) { scale%sx_c%s(src, dst, sw, sh, sp, dp); }\n' \
        "$scale" "$depth" "$scale" "$depth"
    done
  done
  awk '/^void scale1x_c16/ { copy=1 } copy { print }' "$SRC/scaler_neon.c"
} > "$SRC/scaler_c.c"

cp "$PATCH_DIR/picoarch_pixel2_fbdev.h" "$SRC/picoarch_pixel2_fbdev.h"
cp "$PATCH_DIR/picoarch_pixel2_host.c" "$SRC/picoarch_pixel2_host.c"
cp "$PATCH_DIR/picoarch_pixel2_host.h" "$SRC/picoarch_pixel2_host.h"

apply_once "$PATCH_DIR/picoarch-pixel2-input-aspect.patch"
apply_once "$PATCH_DIR/picoarch-pixel2-content-dir.patch"

perl -0pi -e 's{\tSDL_InitSubSystem\(SDL_INIT_JOYSTICK\);\n\n\tjoycount = SDL_NumJoysticks\(\);}{\t/* Pixel2 controller input is owned by evdev. */\n\tjoycount = 0;} or die "SDL joystick initialization marker missing\n"' \
  "$SRC/libpicofe/in_sdl.c"

perl -0pi -e 's{// begin [[:alpha:]]+ hardware scaling support.*?// end [[:alpha:]]+ hardware scaling support}{static void buffer_init(void) {}\nstatic void buffer_quit(void) {}\nstatic void buffer_scale(unsigned w, unsigned h, size_t pitch, const void *src) {\n\tscale(w, h, pitch, src, screen->pixels);\n}}s or die "hardware scaling block marker missing\n";
  s{static SDL_Surface\* screen;}{static SDL_Surface* screen;\nstatic SDL_Surface* display;\n#include "picoarch_pixel2_fbdev.h"} or die "screen marker missing\n";
  s{static void \*fb_flip\(void\)\n\{\n\tSDL_Flip\(screen\);\n\treturn screen->pixels;\n\}}{static void *fb_flip(void)\n{\n\tpixel2_fb_present(screen);\n\treturn screen->pixels;\n}} or die "flip marker missing\n";
  s{screen = SDL_SetVideoMode\(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP \* 8, SDL_SWSURFACE\);\n\tif \(screen == NULL\) \{\n\t\tPA_ERROR\("%s, failed to set video mode\\n", __func__\);\n\t\treturn -1;\n\t\}}{display = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_SWSURFACE);\n\tif (display == NULL) {\n\t\tPA_ERROR("%s, failed to set video mode\\n", __func__);\n\t\treturn -1;\n\t}\n\tscreen = SDL_CreateRGBSurface(SDL_SWSURFACE, SCREEN_WIDTH, SCREEN_HEIGHT, 16,\n\t                              0xF800, 0x07E0, 0x001F, 0x0000);\n\tif (screen == NULL || pixel2_fb_init() < 0) {\n\t\tPA_ERROR("%s, failed to create Pixel2 RGB565 framebuffer\\n", __func__);\n\t\treturn -1;\n\t}} or die "video init marker missing\n";
  s{\tSDL_FreeSurface\(screen\);\n\tscreen = NULL;}{\tSDL_FreeSurface(screen);\n\tSDL_FreeSurface(display);\n\tpixel2_fb_finish();\n\tscreen = NULL;\n\tdisplay = NULL;} or die "video finish marker missing\n"' "$SRC/plat_sdl.c"

perl -0pi -e 's{static void get_tag_name\(const char\* in_path, char\* out_tag\) \{.*?\n\}\n\nint main}{static void get_tag_name(const char* in_path, char* out_tag) {\n\tconst char *system = getenv("PLUMOS_PICOARCH_SYSTEM");\n\tif (system && system[0]) {\n\t\tsnprintf(out_tag, MAX_PATH, "%s", system);\n\t\treturn;\n\t}\n\tconst char *slash = strrchr(in_path, '\''/'\'');\n\tsize_t len = slash ? (size_t)(slash - in_path) : strlen(in_path);\n\twhile (len > 0 && in_path[len - 1] == '\''/'\'') len--;\n\tconst char *start = in_path;\n\tfor (size_t i = 0; i < len; i++) if (in_path[i] == '\''/'\'') start = in_path + i + 1;\n\tsnprintf(out_tag, MAX_PATH, "%.*s", (int)(in_path + len - start), start);\n}\n\nint main}s or die "tag function marker missing\n"' "$SRC/main.c"

perl -0pi -e 's{static void set_directories\(const char \*core_name, const char \*tag_name\) \{.*?\n\}\n\n// based on eggs}{static void set_directories(const char *core_name, const char *tag_name) {\n\tconst char *home = getenv("HOME");\n\tconst char *save_root = getenv("PLUMOS_PICOARCH_SAVE_ROOT");\n\tconst char *bios_dir = getenv("PLUMOS_PICOARCH_BIOS_DIR");\n\tif (home) {\n\t\tsnprintf(config_dir, MAX_PATH, "%s/.picoarch-%s-%s/", home, core_name, tag_name);\n\t\tmkdir(config_dir, 0755);\n\t}\n\tif (!save_root || !save_root[0]) save_root = "/mnt/plumos/saves";\n\tif (!bios_dir || !bios_dir[0]) bios_dir = "/mnt/plumos-user/bios";\n\tsnprintf(save_dir, MAX_PATH, "%s/%s/", save_root, tag_name);\n\tmkdir(save_root, 0755);\n\tmkdir(save_dir, 0755);\n\tsnprintf(system_dir, MAX_PATH, "%s", bios_dir);\n}\n\n// based on eggs}s or die "directory function marker missing\n"' "$SRC/core.c"

perl -0pi -e 's{options_init\(\*\(const struct retro_core_option_definition \*\*\)data\);}{options_init((const struct retro_core_option_definition *)data);} or die "core options pointer marker missing\n"' \
  "$SRC/core.c"

apply_once "$PATCH_DIR/picoarch-pixel2-pixel-format.patch"
apply_once "$PATCH_DIR/picoarch-pixel2-libretro-env.patch"
apply_once "$PATCH_DIR/picoarch-pixel2-controller-init.patch"
apply_once "$PATCH_DIR/picoarch-pixel2-frame-audio-callback.patch"
git -C "$SRC" apply --recount "$PATCH_DIR/picoarch-pixel2-async-audio-callback.patch"
apply_once "$PATCH_DIR/picoarch-pixel2-frame-pacing.patch"
git -C "$SRC" apply --recount --unidiff-zero "$PATCH_DIR/picoarch-pixel2-display-audio-rate.patch"

perl -0pi -e 's{#include "core\.h"}{#include "core.h"\n#include "picoarch_pixel2_host.h"} or die "host interface include marker missing\n";
  s~\tcase RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY: \{ /\* 30 \*/~\tcase RETRO_ENVIRONMENT_GET_PERF_INTERFACE: { /* 28 */\n\t\treturn pixel2_get_perf_interface(data);\n\t}\n\tcase RETRO_ENVIRONMENT_GET_VFS_INTERFACE: { /* 45 | experimental */\n\t\treturn pixel2_get_vfs_interface(data, core_path);\n\t}\n\tcase RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY: { /* 30 */~ or die "host interface environment marker missing\n"' \
  "$SRC/core.c"

make -C "$SRC" platform=pixel2 MMENU=0 libpicofe/.patched
make -C "$SRC" platform=pixel2 MMENU=0 -j"$JOBS" picoarch

cmake -S "$SDL12_SRC" -B "$SDL12_SRC/build-pixel2" \
  -DCMAKE_BUILD_TYPE=Release \
  -DSDL2_INCLUDE_DIRS=/usr/include/SDL2 \
  -DSDL12TESTS=OFF
cmake --build "$SDL12_SRC/build-pixel2" -j"$JOBS"

rm -rf "$OUT_ROOT"
mkdir -p "$PLUMOS_DIR/picoarch/bin" "$PLUMOS_DIR/picoarch/lib" \
  "$PLUMOS_DIR/bin" "$PLUMOS_DIR/config/standalone" "$PLUMOS_DIR/licenses" \
  "$COMPONENT_DIR"
install -m 0755 "$SRC/picoarch" "$PLUMOS_DIR/picoarch/bin/picoarch"
install -m 0644 "$SDL12_SRC/build-pixel2/libSDL-1.2.so.1.2.72" \
  "$PLUMOS_DIR/picoarch/lib/libSDL-1.2.so.0"
install -m 0644 "$(ldconfig -p | awk '/libpng16\.so\.16 \(/{print $NF; exit}')" \
  "$PLUMOS_DIR/picoarch/lib/libpng16.so.16"
install -m 0644 "$(ldconfig -p | awk '/libz\.so\.1 \(/{print $NF; exit}')" \
  "$PLUMOS_DIR/picoarch/lib/libz.so.1"
install -m 0755 "$ROOT_DIR/package/picoarch-pixel2/plumos/bin/plumos-picoarch-launch" "$PLUMOS_DIR/bin/"
install -m 0755 "$ROOT_DIR/package/picoarch-pixel2/plumos/bin/plumos-picoarch-stop" "$PLUMOS_DIR/bin/"
install -m 0644 "$ROOT_DIR/package/picoarch-pixel2/plumos/config/standalone/picoarch.env" \
  "$PLUMOS_DIR/config/standalone/"
install -m 0644 "$SRC/LICENSE" "$PLUMOS_DIR/licenses/picoarch-LICENSE"
install -m 0644 "$SDL12_SRC/LICENSE.txt" "$PLUMOS_DIR/licenses/picoarch-sdl12-compat-LICENSE.txt"

cat > "$COMPONENT_DIR/manifest.json" <<EOF
{
  "name": "plumOS Pixel2 PicoArch",
  "component": "picoarch",
  "device": "pixel2",
  "architecture": "aarch64",
  "picoarch_repo": "$PICOARCH_REPO",
  "picoarch_ref": "$PICOARCH_REF",
  "sdl12_compat_repo": "$SDL12_REPO",
  "sdl12_compat_ref": "$SDL12_REF",
  "render_contract": "Pixel2 fbdev RGB565-to-BGRA8888 presenter with SDL12 dummy surface",
  "input_contract": "Pixel2 evdev dpad, ABXY east-confirm, shoulders, start/select, menu",
  "audio_contract": "plumOS ALSA plumos_output",
  "core_route": "cores/*_libretro.so"
}
EOF

(
  cd "$PLUMOS_DIR"
  find bin picoarch config/standalone licenses components/picoarch -type f \
    ! -path 'components/picoarch/checksums.sha256' \
    -print |
    sort |
    while IFS= read -r path; do sha256sum "$path"; done
) > "$COMPONENT_DIR/checksums.sha256"
(
  cd "$PLUMOS_DIR"
  sha256sum -c components/picoarch/checksums.sha256 >/dev/null
)
printf 'created: %s\n' "$PLUMOS_DIR"
