#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if [ "${1:-}" != --inside ]; then
    exec "$ROOT_DIR/scripts/docker-build.sh" pyxel-runtime "$@"
fi
shift

ROOT_DIR=/work
OUT_ROOT="${PLUMOS_PIXEL2_PYXEL_OUT:-$ROOT_DIR/output/pyxel-runtime/pixel2}"
PLUMOS_DIR="$OUT_ROOT/plumos"
COMPONENT_DIR="$PLUMOS_DIR/components/pyxel"
PACKAGE_DIR="$ROOT_DIR/package/pyxel-pixel2/plumos"
LOCK_FILE="${PLUMOS_PIXEL2_PYXEL_LOCK:-$PACKAGE_DIR/share/pyxel/requirements.lock.txt}"
DEFAULT_REQUIREMENTS="${PLUMOS_PIXEL2_PYXEL_REQUIREMENTS:-$PACKAGE_DIR/share/pyxel/requirements.txt}"
FIT_SOURCE="$ROOT_DIR/package/pyxel-pixel2/src/plumos_pyxel_fit.c"
ROTATE_SOURCE="$ROOT_DIR/package/portmaster-pixel2/src/plumos_portmaster_gl_rotate.c"
SDL_VERSION="${PLUMOS_PIXEL2_PYXEL_SDL_VERSION:-2.28.4}"
SDL_ARCHIVE="$ROOT_DIR/output/downloads/SDL2-${SDL_VERSION}.tar.gz"
SDL_URL="${PLUMOS_PIXEL2_PYXEL_SDL_URL:-https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL2-${SDL_VERSION}.tar.gz}"
SDL_SHA256="${PLUMOS_PIXEL2_PYXEL_SDL_SHA256:-888b8c39f36ae2035d023d1b14ab0191eb1d26403c3cf4d4d5ede30e66a4942c}"
SDL_BUILD_ROOT="$ROOT_DIR/output/build/pyxel-sdl2-pixel2"
PYTHON_VERSION="${PLUMOS_PIXEL2_PYTHON_VERSION:-3.11}"
PYTHON_BIN="${PLUMOS_PIXEL2_PYTHON_BIN:-/usr/bin/python3.11}"
PIP_VERSION="${PLUMOS_PIXEL2_PIP_VERSION:-23.0.1}"
PIP_CACHE_DIR="${PLUMOS_PIXEL2_PIP_CACHE:-$ROOT_DIR/output/build/pip-cache}"
READELF="${READELF:-readelf}"
STRIP="${STRIP:-strip}"
CC="${CC:-gcc}"
SOURCE_REF="$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || printf unknown)"
SOURCE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[ -n "$SOURCE_EPOCH" ] || SOURCE_EPOCH="$(git -C "$ROOT_DIR" show -s --format=%ct HEAD 2>/dev/null || date +%s)"

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

find_target_lib() {
    local name="$1"
    local dir bundled
    if [ -d "$PLUMOS_DIR" ]; then
        bundled="$(find "$PLUMOS_DIR" -type f -name "$name" -print -quit)"
        if [ -n "$bundled" ]; then
            printf '%s\n' "$bundled"
            return 0
        fi
    fi
    for dir in "$ROOT_DIR/output/standalone/pixel2/plumos/lib" \
        "$ROOT_DIR/output/retroarch/pixel2/plumos/emulator/lib" \
        /lib/aarch64-linux-gnu /usr/lib/aarch64-linux-gnu /lib /usr/lib; do
        [ -d "$dir" ] || continue
        if [ -e "$dir/$name" ]; then
            readlink -f "$dir/$name"
            return 0
        fi
        bundled="$(find "$dir" -maxdepth 1 -type f -name "$name.*" -print 2>/dev/null | sort | tail -n 1)"
        if [ -n "$bundled" ]; then
            printf '%s\n' "$bundled"
            return 0
        fi
    done
    return 1
}

copy_dependency_tree() {
    local elf="$1"
    local destination="$2"
    local dependency source soname
    "$READELF" -d "$elf" 2>/dev/null |
        awk -F'[][]' '/NEEDED/ { print $2 }' |
        while IFS= read -r dependency; do
            [ "$dependency" = "ld-linux-aarch64.so.1" ] && continue
            [ -e "$destination/$dependency" ] && continue
            source="$(find_target_lib "$dependency" || true)"
            [ -n "$source" ] || fail "runtime dependency not found: $dependency"
            install -m 0644 "$source" "$destination/$dependency"
            soname="$("$READELF" -d "$source" 2>/dev/null | awk -F'[][]' '/SONAME/ { print $2; exit }')"
            if [ -n "$soname" ] && [ "$soname" != "$dependency" ] && [ ! -e "$destination/$soname" ]; then
                cp -p "$destination/$dependency" "$destination/$soname"
            fi
            copy_dependency_tree "$source" "$destination"
        done
}

copy_named_library() {
    local name="$1"
    local destination="$2"
    local source
    source="$(find_target_lib "$name" || true)"
    [ -n "$source" ] || fail "required library not found: $name"
    if [ -e "$destination/$name" ] && [ "$(readlink -f "$source")" = "$(readlink -f "$destination/$name")" ]; then
        return 0
    fi
    install -m 0644 "$source" "$destination/$name"
    copy_dependency_tree "$source" "$destination"
}

materialize_links() {
    local root="$1"
    local link target
    while IFS= read -r link; do
        target="$(readlink -f "$link")"
        [ -f "$target" ] || fail "unsupported Python symlink: $link"
        rm -f "$link"
        cp -p "$target" "$link"
    done < <(find "$root" -type l -print)
}

[ -x "$PYTHON_BIN" ] || fail "Python runtime is missing: $PYTHON_BIN"
"$PYTHON_BIN" -m pip --version >/dev/null 2>&1 ||
    fail "python3-pip is required in the Pixel2 toolchain image"
[ -r "$LOCK_FILE" ] || fail "Pyxel lock file is missing: $LOCK_FILE"
[ -r "$DEFAULT_REQUIREMENTS" ] ||
    fail "Pyxel default requirements are missing: $DEFAULT_REQUIREMENTS"
[ -r "$FIT_SOURCE" ] || fail "Pixel2 Pyxel display fit source is missing: $FIT_SOURCE"
[ -r "$ROTATE_SOURCE" ] || fail "Pixel2 Pyxel GL rotation source is missing: $ROTATE_SOURCE"

mkdir -p "$(dirname "$SDL_ARCHIVE")"
if [ ! -f "$SDL_ARCHIVE" ]; then
    curl -L --fail --retry 3 -o "$SDL_ARCHIVE" "$SDL_URL"
fi
[ "$(sha256sum "$SDL_ARCHIVE" | awk '{ print $1 }')" = "$SDL_SHA256" ] ||
    fail "unexpected SDL2 source SHA-256"

rm -rf "$OUT_ROOT"
PYTHON_ROOT="$PLUMOS_DIR/apps/python"
PYXEL_ROOT="$PLUMOS_DIR/apps/pyxel"
PYTHON_SITE="$PYTHON_ROOT/site-packages"
PYXEL_SITE="$PYXEL_ROOT/site"
PYXEL_LIB="$PYXEL_ROOT/lib"
mkdir -p \
    "$PYTHON_ROOT/bin" "$PYTHON_ROOT/lib" "$PYTHON_SITE" \
    "$PYXEL_SITE" "$PYXEL_LIB" \
    "$PLUMOS_DIR/bin" "$COMPONENT_DIR" \
    "$PLUMOS_DIR/share/pyxel" "$PLUMOS_DIR/share/doc/pyxel" \
    "$PLUMOS_DIR/licenses"

install -m 0755 "$PYTHON_BIN" "$PYTHON_ROOT/bin/python3.11"
cp -a "/usr/lib/python${PYTHON_VERSION}" "$PYTHON_ROOT/lib/"
rm -rf \
    "$PYTHON_ROOT/lib/python${PYTHON_VERSION}/config-"* \
    "$PYTHON_ROOT/lib/python${PYTHON_VERSION}/idlelib" \
    "$PYTHON_ROOT/lib/python${PYTHON_VERSION}/test" \
    "$PYTHON_ROOT/lib/python${PYTHON_VERSION}/tkinter" \
    "$PYTHON_ROOT/lib/python${PYTHON_VERSION}/turtledemo"
find "$PYTHON_ROOT" -type d -name __pycache__ -prune -exec rm -rf {} +
materialize_links "$PYTHON_ROOT"

mkdir -p "$PIP_CACHE_DIR"
PIP_CACHE_DIR="$PIP_CACHE_DIR" PIP_DISABLE_PIP_VERSION_CHECK=1 "$PYTHON_BIN" -m pip install \
    --break-system-packages \
    --no-compile \
    --only-binary=:all: \
    --target "$PYTHON_SITE" \
    "pip==$PIP_VERSION"
PIP_CACHE_DIR="$PIP_CACHE_DIR" PIP_DISABLE_PIP_VERSION_CHECK=1 "$PYTHON_BIN" -m pip install \
    --break-system-packages \
    --no-compile \
    --only-binary=:all: \
    --requirement "$LOCK_FILE" \
    --target "$PYXEL_SITE"

find "$PYXEL_SITE" -type d \( -name test -o -name tests \) -prune \
    -exec rm -rf {} +
rm -rf \
    "$PYXEL_SITE/pygame/docs" \
    "$PYXEL_SITE/pygame/examples" \
    "$PYXEL_SITE/pyxel/examples"
find "$PYXEL_SITE" -type d -name __pycache__ -prune -exec rm -rf {} +

install -m 0644 "$DEFAULT_REQUIREMENTS" "$PLUMOS_DIR/share/pyxel/requirements.txt"
install -m 0644 "$LOCK_FILE" "$PLUMOS_DIR/share/pyxel/requirements.lock.txt"
install -m 0644 /etc/ssl/certs/ca-certificates.crt \
    "$PYTHON_ROOT/ca-certificates.crt"
install -m 0755 "$PACKAGE_DIR/bin/plumos-python-pixel2" "$PLUMOS_DIR/bin/"
install -m 0755 "$PACKAGE_DIR/bin/plumos-pyxel-pixel2-launch" "$PLUMOS_DIR/bin/"
install -m 0755 "$PACKAGE_DIR/bin/plumos-pyxel-setup" "$PLUMOS_DIR/bin/"

loader="$(find_target_lib ld-linux-aarch64.so.1)" ||
    fail "AArch64 loader not found"
install -m 0755 "$loader" "$PYTHON_ROOT/lib/ld-linux-aarch64.so.1"
copy_dependency_tree "$PYTHON_ROOT/bin/python3.11" "$PYTHON_ROOT/lib"

while IFS= read -r elf; do
    copy_dependency_tree "$elf" "$PYTHON_ROOT/lib"
done < <(
    find "$PYTHON_ROOT/lib/python${PYTHON_VERSION}" "$PYXEL_SITE" -type f \
        -exec sh -c '
            for path do
                case "$(file -b "$path" 2>/dev/null)" in
                    ELF*) printf "%s\n" "$path" ;;
                esac
            done
        ' sh {} +
)

for library in \
    libEGL.so.1 \
    libGLESv2.so.2 \
    libGLdispatch.so.0 \
    libgbm.so.1 \
    libdrm.so.2 \
    libudev.so.1 \
    libglib-2.0.so.0 \
    libgthread-2.0.so.0 \
    libpcre2-8.so.0; do
    copy_named_library "$library" "$PYXEL_LIB"
done

# pygame 2.6.1 was compiled against SDL 2.28.4, while Pyxel needs KMSDRM on
# this console.  The wheel's bundled SDL has no KMSDRM and Debian's 2.26.5 is
# rejected by pygame as a downgrade.  Build one shared SDL 2.28.4 satisfying
# both consumers, then place it ahead of both wheel-private copies.
rm -rf "$SDL_BUILD_ROOT"
mkdir -p "$SDL_BUILD_ROOT/source" "$SDL_BUILD_ROOT/build" "$SDL_BUILD_ROOT/prefix"
tar -xzf "$SDL_ARCHIVE" -C "$SDL_BUILD_ROOT/source" --strip-components=1
cmake -S "$SDL_BUILD_ROOT/source" -B "$SDL_BUILD_ROOT/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$SDL_BUILD_ROOT/prefix" \
    -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF \
    -DSDL_KMSDRM=ON -DSDL_WAYLAND=OFF -DSDL_X11=OFF \
    -DSDL_ALSA=ON -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF
cmake --build "$SDL_BUILD_ROOT/build" -j"$(nproc 2>/dev/null || echo 4)"
cmake --install "$SDL_BUILD_ROOT/build"
sdl_library="$(find "$SDL_BUILD_ROOT/prefix/lib" -type f -name 'libSDL2-2.0.so.*' -print | sort | tail -n 1)"
[ -n "$sdl_library" ] || fail "Pixel2 SDL2 KMSDRM library was not installed"
install -m 0644 "$sdl_library" "$PYXEL_LIB/libSDL2-2.0.so.0"
copy_dependency_tree "$PYXEL_LIB/libSDL2-2.0.so.0" "$PYXEL_LIB"
strings "$PYXEL_LIB/libSDL2-2.0.so.0" | grep KMSDRM >/dev/null ||
    fail "Pixel2 SDL2 build has no KMSDRM driver"
install -m 0644 "$SDL_BUILD_ROOT/source/LICENSE.txt" \
    "$PLUMOS_DIR/licenses/SDL2-${SDL_VERSION}-LICENSE.txt"

"$CC" -O2 -fPIC -Wall -Wextra -Werror -shared \
    -Wl,-soname,plumos-pyxel-fit.so \
    -o "$PYXEL_LIB/plumos-pyxel-fit.so" "$FIT_SOURCE" -ldl
"$STRIP" --strip-unneeded "$PYXEL_LIB/plumos-pyxel-fit.so" >/dev/null 2>&1 || true
"$CC" -O2 -fPIC -Wall -Wextra -Werror -shared \
    -I/usr/include/SDL2 -I/usr/include/GLES2 \
    '-DPLUMOS_GL_ROTATION_ENV="PLUMOS_PYXEL_GL_ROTATION"' \
    '-DPLUMOS_GL_ROTATION_LABEL="Pyxel"' \
    -Wl,-z,defs -Wl,-soname,plumos-pyxel-gl-rotate.so \
    -o "$PYXEL_LIB/plumos-pyxel-gl-rotate.so" "$ROTATE_SOURCE" -ldl
"$STRIP" --strip-unneeded "$PYXEL_LIB/plumos-pyxel-gl-rotate.so" >/dev/null 2>&1 || true

PYTHONHOME="$PYTHON_ROOT" \
PYTHONPATH="$PYTHON_ROOT/site-packages:$PYXEL_SITE" \
LD_LIBRARY_PATH="$PYXEL_LIB:$PYTHON_ROOT/lib" \
PYTHONDONTWRITEBYTECODE=1 \
"$PYTHON_ROOT/bin/python3.11" - <<'PY'
from importlib import import_module, metadata
for distribution, module in (
    ("pyxel", "pyxel"),
    ("pygame", "pygame"),
    ("numpy", "numpy"),
    ("Pillow", "PIL"),
):
    import_module(module)
    print(f"{distribution}={metadata.version(distribution)}")
PY

find "$PYTHON_ROOT" "$PYXEL_ROOT" -type d -name __pycache__ -prune \
    -exec rm -rf {} +

lock_sha256="$(sha256sum "$LOCK_FILE" | awk '{print $1}')"
fit_sha256="$(sha256sum "$FIT_SOURCE" | awk '{print $1}')"
rotate_sha256="$(sha256sum "$ROTATE_SOURCE" | awk '{print $1}')"
file_count="$(find "$PLUMOS_DIR/apps" -type f | wc -l | tr -d ' ')"
cat >"$COMPONENT_DIR/manifest.json" <<EOF
{
  "name": "plumOS Pixel2 Pyxel runtime",
  "component": "pyxel",
  "device": "pixel2",
  "architecture": "aarch64",
  "python": "$PYTHON_VERSION",
  "pip": "$PIP_VERSION",
  "requirements_sha256": "$lock_sha256",
  "display_fit_sha256": "$fit_sha256",
  "display_rotation_sha256": "$rotate_sha256",
  "sdl2": {"version": "$SDL_VERSION", "source_sha256": "$SDL_SHA256", "kmsdrm": true},
  "source_ref": "$SOURCE_REF",
  "source_date_epoch": $SOURCE_EPOCH,
  "runtime_contract": "Pixel2 bundled Python, Pyxel, pygame, SDL2 KMSDRM/GLES, 640x480 fit plus 270-degree scanout, ALSA plumos_pyxel",
  "launcher": "bin/plumos-pyxel-pixel2-launch",
  "setup": "bin/plumos-pyxel-setup",
  "file_count": $file_count
}
EOF
cat >"$PLUMOS_DIR/share/doc/pyxel/README.txt" <<EOF
plumOS Pixel2 Pyxel runtime
python=$PYTHON_VERSION
pip=$PIP_VERSION
requirements_sha256=$lock_sha256
baseline=$PLUMOS_DIR/apps/pyxel/site
user_site=/mnt/plumos/state/pyxel-site
display=SDL2 KMSDRM GLES with Pixel2 640x480 fit and 270-degree scanout
audio=ALSA plumos_pyxel
EOF
(
    cd "$PLUMOS_DIR"
    find apps/python apps/pyxel \
        bin/plumos-python-pixel2 \
        bin/plumos-pyxel-pixel2-launch \
        bin/plumos-pyxel-setup \
        share/pyxel \
        share/doc/pyxel \
        components/pyxel/manifest.json \
        -type f -print | sort |
        while IFS= read -r path; do sha256sum "$path"; done
) >"$COMPONENT_DIR/checksums.sha256"
(
    cd "$PLUMOS_DIR"
    sha256sum -c components/pyxel/checksums.sha256 >/dev/null
)
printf 'pyxel_runtime=result-ok output=%s\n' "$PLUMOS_DIR"
