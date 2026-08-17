#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${ROOT_DIR:-$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)}"
OUT_DIR="${PLUMOS_PIXEL2_PORTMASTER_OUT:-${ROOT_DIR}/output/portmaster/pixel2}"
BUILD_DIR="${PLUMOS_PIXEL2_PORTMASTER_BUILD_DIR:-${ROOT_DIR}/build/portmaster-pixel2}"
CACHE_DIR="${PLUMOS_PIXEL2_PORTMASTER_CACHE_DIR:-${ROOT_DIR}/.cache/portmaster}"
PACKAGE_DIR="${ROOT_DIR}/package/portmaster-pixel2"
VERSION="${PORTMASTER_VERSION:-2026.06.23-0015}"
ARCHIVE="PortMaster-${VERSION}.zip"
URL="${PORTMASTER_URL:-https://github.com/PortsMaster/PortMaster-GUI/releases/download/${VERSION}/PortMaster.zip}"
EXPECTED_MD5="${PORTMASTER_MD5:-41d137e6bb123c755806939831bcce2f}"
EXPECTED_SHA256="${PORTMASTER_SHA256:-772f2d56fc1abfbf79a3404ca78f240776c81c5a5b92786a0a748ae554339b7b}"
OPENAL_VERSION="1.23.1"
OPENAL_COMMIT="d3875f333fb6abe2f39d82caca329414871ae53b"
OPENAL_ARCHIVE="openal-soft-${OPENAL_VERSION}.tar.gz"
OPENAL_URL="https://github.com/kcat/openal-soft/archive/refs/tags/${OPENAL_VERSION}.tar.gz"
OPENAL_SHA256="dfddf3a1f61059853c625b7bb03de8433b455f2f79f89548cbcbd5edca3d4a4a"
FFMPEG_COMPAT_VERSION="4.4.6"
FFMPEG_COMPAT_ARCHIVE="ffmpeg-${FFMPEG_COMPAT_VERSION}.tar.xz"
FFMPEG_COMPAT_URL="https://ffmpeg.org/releases/${FFMPEG_COMPAT_ARCHIVE}"
FFMPEG_COMPAT_SHA256="2290461f467c08ab801731ed412d8e724a5511d6c33173654bd9c1d2e25d0617"
LIBEVDEV_VERSION="1.13.1"
LIBEVDEV_ARCHIVE="libevdev-${LIBEVDEV_VERSION}.tar.xz"
LIBEVDEV_URL="https://www.freedesktop.org/software/libevdev/${LIBEVDEV_ARCHIVE}"
LIBEVDEV_SHA256="06a77bf2ac5c993305882bc1641017f5bec1592d6d1b64787bad492ab34f2f36"
FLAC_COMPAT_VERSION="1.3.3"
FLAC_COMPAT_ARCHIVE="flac-${FLAC_COMPAT_VERSION}.tar.xz"
FLAC_COMPAT_URL="https://downloads.xiph.org/releases/flac/${FLAC_COMPAT_ARCHIVE}"
FLAC_COMPAT_SHA256="213e82bd716c9de6db2f98bcadbc4c24c7e2efe8c75939a1a84e28539c4e1748"
JPEG_COMPAT_VERSION="8d"
JPEG_COMPAT_ARCHIVE="jpegsrc.v${JPEG_COMPAT_VERSION}.tar.gz"
JPEG_COMPAT_URL="https://www.ijg.org/files/${JPEG_COMPAT_ARCHIVE}"
JPEG_COMPAT_SHA256="fdc4d4c11338ad028a7d23fb53f5bb9354671392a67fb1b52e0c32a7121891f8"
READLINE_COMPAT_VERSION="7.0"
READLINE_COMPAT_ARCHIVE="readline-${READLINE_COMPAT_VERSION}.tar.gz"
READLINE_COMPAT_URL="https://ftp.gnu.org/gnu/readline/${READLINE_COMPAT_ARCHIVE}"
READLINE_COMPAT_SHA256="750d437185286f40a369e1e4f4764eda932b9459b5ec9a731628393dd3d32334"
THEORA_COMPAT_VERSION="1.1.1"
THEORA_COMPAT_ARCHIVE="libtheora-${THEORA_COMPAT_VERSION}.tar.xz"
THEORA_COMPAT_URL="https://downloads.xiph.org/releases/theora/${THEORA_COMPAT_ARCHIVE}"
THEORA_COMPAT_SHA256="f36da409947aa2b3dcc6af0a8c2e3144bc19db2ed547d64e9171c59c66561c61"
SDL2_MIXER_VERSION="2.6.2"
SDL2_MIXER_ARCHIVE="SDL2_mixer-${SDL2_MIXER_VERSION}.tar.gz"
SDL2_MIXER_URL="https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL2_MIXER_VERSION}/${SDL2_MIXER_ARCHIVE}"
SDL2_MIXER_SHA256="8cdea810366decba3c33d32b8071bccd1c309b2499a54946d92b48e6922aa371"
MESON_BOOTSTRAP_VERSION="1.4.2"
MESON_BOOTSTRAP_ARCHIVE="meson-${MESON_BOOTSTRAP_VERSION}.tar.gz"
MESON_BOOTSTRAP_URL="https://github.com/mesonbuild/meson/archive/refs/tags/${MESON_BOOTSTRAP_VERSION}.tar.gz"
MESON_BOOTSTRAP_SHA256="11d1336fe35e1ade57510a846a31d7dc2e3b6ac1e2491c2831bce5a2a192ba0d"
CAIRO_COMPAT_VERSION="1.18.4"
CAIRO_COMPAT_ARCHIVE="cairo-${CAIRO_COMPAT_VERSION}.tar.xz"
CAIRO_COMPAT_URL="https://cairographics.org/releases/${CAIRO_COMPAT_ARCHIVE}"
CAIRO_COMPAT_SHA256="445ed8208a6e4823de1226a74ca319d3600e83f6369f99b14265006599c32ccb"
PIXMAN_RUNTIME_VERSION="0.42.2-1"
SQUASHFS_TOOLS_VERSION="1:4.5.1-1"
ZIP_VERSION="3.0-13"
BASH_RUNTIME_VERSION="5.2.15-2+b13"
ADAPTER_VERSION="25"

usage() {
    cat <<EOF
Usage: build-portmaster.sh

Builds the plumOS Pixel2 PortMaster package from a pinned official release.

Environment:
  PLUMOS_PIXEL2_PORTMASTER_OUT         Output directory; default output/portmaster/pixel2.
  PORTMASTER_VERSION               Official release version; default ${VERSION}.
  PORTMASTER_URL                   Official archive URL.
  PORTMASTER_MD5                   Expected official MD5.
  PORTMASTER_SHA256                Independently pinned SHA-256.
EOF
}

case "${1:-}" in
    -h|--help|help)
        usage
        exit 0
        ;;
    "") ;;
    *)
        printf 'error: unknown argument: %s\n' "$1" >&2
        usage >&2
        exit 2
        ;;
esac

for tool in cc cmake curl dpkg-query make md5sum meson ninja sha256sum tar unzip python3 rsync zip; do
    command -v "$tool" >/dev/null 2>&1 || {
        printf 'error: required tool is unavailable: %s\n' "$tool" >&2
        exit 1
    }
done

actual_squashfs_tools_version="$(dpkg-query -W -f='${Version}' squashfs-tools)"
[ "$actual_squashfs_tools_version" = "$SQUASHFS_TOOLS_VERSION" ] || {
    printf 'error: squashfs-tools version mismatch: expected %s, got %s\n' \
        "$SQUASHFS_TOOLS_VERSION" "$actual_squashfs_tools_version" >&2
    exit 1
}
[ -x /usr/bin/unsquashfs ] || {
    printf 'error: /usr/bin/unsquashfs is unavailable\n' >&2
    exit 1
}
actual_zip_version="$(dpkg-query -W -f='${Version}' zip)"
[ "$actual_zip_version" = "$ZIP_VERSION" ] || {
    printf 'error: zip version mismatch: expected %s, got %s\n' \
        "$ZIP_VERSION" "$actual_zip_version" >&2
    exit 1
}
[ -x /usr/bin/zip ] || {
    printf 'error: /usr/bin/zip is unavailable\n' >&2
    exit 1
}
actual_bash_runtime_version="$(dpkg-query -W -f='${Version}' bash)"
[ "$actual_bash_runtime_version" = "$BASH_RUNTIME_VERSION" ] || {
    printf 'error: bash runtime version mismatch: expected %s, got %s\n' \
        "$BASH_RUNTIME_VERSION" "$actual_bash_runtime_version" >&2
    exit 1
}
[ -x /bin/bash ] || {
    printf 'error: /bin/bash is unavailable\n' >&2
    exit 1
}
lzo_library="$(find /usr/lib/aarch64-linux-gnu -type f -name 'liblzo2.so.*' | sort | tail -n 1)"
[ -n "$lzo_library" ] && [ -f "$lzo_library" ] || {
    printf 'error: liblzo2 runtime dependency is unavailable\n' >&2
    exit 1
}
actual_pixman_runtime_version="$(dpkg-query -W -f='${Version}' libpixman-1-0)"
[ "$actual_pixman_runtime_version" = "$PIXMAN_RUNTIME_VERSION" ] || {
    printf 'error: pixman runtime version mismatch: expected %s, got %s\n' \
        "$PIXMAN_RUNTIME_VERSION" "$actual_pixman_runtime_version" >&2
    exit 1
}
pixman_library="$(find /usr/lib/aarch64-linux-gnu -type f \
    -name 'libpixman-1.so.0.*' | sort | tail -n 1)"
[ -n "$pixman_library" ] && [ -f "$pixman_library" ] || {
    printf 'error: pixman runtime dependency is unavailable\n' >&2
    exit 1
}
mkdir -p "$CACHE_DIR" "$BUILD_DIR"
archive_path="$CACHE_DIR/$ARCHIVE"

if [ ! -f "$archive_path" ] ||
   [ "$(sha256sum "$archive_path" | awk '{print $1}')" != "$EXPECTED_SHA256" ]; then
    archive_tmp="$archive_path.tmp.$$"
    rm -f "$archive_tmp"
    curl --fail --location --retry 3 --output "$archive_tmp" "$URL"
    mv -f "$archive_tmp" "$archive_path"
fi

actual_md5="$(md5sum "$archive_path" | awk '{print $1}')"
actual_sha256="$(sha256sum "$archive_path" | awk '{print $1}')"
[ "$actual_md5" = "$EXPECTED_MD5" ] || {
    printf 'error: PortMaster MD5 mismatch: expected %s, got %s\n' \
        "$EXPECTED_MD5" "$actual_md5" >&2
    exit 1
}
[ "$actual_sha256" = "$EXPECTED_SHA256" ] || {
    printf 'error: PortMaster SHA-256 mismatch: expected %s, got %s\n' \
        "$EXPECTED_SHA256" "$actual_sha256" >&2
    exit 1
}

openal_archive_path="$CACHE_DIR/$OPENAL_ARCHIVE"
if [ ! -f "$openal_archive_path" ] ||
   [ "$(sha256sum "$openal_archive_path" | awk '{print $1}')" != "$OPENAL_SHA256" ]; then
    openal_archive_tmp="$openal_archive_path.tmp.$$"
    rm -f "$openal_archive_tmp"
    curl --fail --location --retry 3 --output "$openal_archive_tmp" "$OPENAL_URL"
    mv -f "$openal_archive_tmp" "$openal_archive_path"
fi

actual_openal_sha256="$(sha256sum "$openal_archive_path" | awk '{print $1}')"
[ "$actual_openal_sha256" = "$OPENAL_SHA256" ] || {
    printf 'error: OpenAL Soft SHA-256 mismatch: expected %s, got %s\n' \
        "$OPENAL_SHA256" "$actual_openal_sha256" >&2
    exit 1
}

ffmpeg_compat_archive_path="$CACHE_DIR/$FFMPEG_COMPAT_ARCHIVE"
if [ ! -f "$ffmpeg_compat_archive_path" ] ||
   [ "$(sha256sum "$ffmpeg_compat_archive_path" | awk '{print $1}')" != "$FFMPEG_COMPAT_SHA256" ]; then
    ffmpeg_compat_archive_tmp="$ffmpeg_compat_archive_path.tmp.$$"
    rm -f "$ffmpeg_compat_archive_tmp"
    curl --fail --location --retry 3 --output "$ffmpeg_compat_archive_tmp" "$FFMPEG_COMPAT_URL"
    mv -f "$ffmpeg_compat_archive_tmp" "$ffmpeg_compat_archive_path"
fi

actual_ffmpeg_compat_sha256="$(sha256sum "$ffmpeg_compat_archive_path" | awk '{print $1}')"
[ "$actual_ffmpeg_compat_sha256" = "$FFMPEG_COMPAT_SHA256" ] || {
    printf 'error: FFmpeg compatibility source SHA-256 mismatch: expected %s, got %s\n' \
        "$FFMPEG_COMPAT_SHA256" "$actual_ffmpeg_compat_sha256" >&2
    exit 1
}

libevdev_archive_path="$CACHE_DIR/$LIBEVDEV_ARCHIVE"
if [ ! -f "$libevdev_archive_path" ] ||
   [ "$(sha256sum "$libevdev_archive_path" | awk '{print $1}')" != "$LIBEVDEV_SHA256" ]; then
    libevdev_archive_tmp="$libevdev_archive_path.tmp.$$"
    rm -f "$libevdev_archive_tmp"
    curl --fail --location --retry 3 --output "$libevdev_archive_tmp" "$LIBEVDEV_URL"
    mv -f "$libevdev_archive_tmp" "$libevdev_archive_path"
fi

actual_libevdev_sha256="$(sha256sum "$libevdev_archive_path" | awk '{print $1}')"
[ "$actual_libevdev_sha256" = "$LIBEVDEV_SHA256" ] || {
    printf 'error: libevdev source SHA-256 mismatch: expected %s, got %s\n' \
        "$LIBEVDEV_SHA256" "$actual_libevdev_sha256" >&2
    exit 1
}

flac_compat_archive_path="$CACHE_DIR/$FLAC_COMPAT_ARCHIVE"
if [ ! -f "$flac_compat_archive_path" ] ||
   [ "$(sha256sum "$flac_compat_archive_path" | awk '{print $1}')" != "$FLAC_COMPAT_SHA256" ]; then
    flac_compat_archive_tmp="$flac_compat_archive_path.tmp.$$"
    rm -f "$flac_compat_archive_tmp"
    curl --fail --location --retry 3 --output "$flac_compat_archive_tmp" "$FLAC_COMPAT_URL"
    mv -f "$flac_compat_archive_tmp" "$flac_compat_archive_path"
fi

actual_flac_compat_sha256="$(sha256sum "$flac_compat_archive_path" | awk '{print $1}')"
[ "$actual_flac_compat_sha256" = "$FLAC_COMPAT_SHA256" ] || {
    printf 'error: FLAC compatibility source SHA-256 mismatch: expected %s, got %s\n' \
        "$FLAC_COMPAT_SHA256" "$actual_flac_compat_sha256" >&2
    exit 1
}

jpeg_compat_archive_path="$CACHE_DIR/$JPEG_COMPAT_ARCHIVE"
if [ ! -f "$jpeg_compat_archive_path" ] ||
   [ "$(sha256sum "$jpeg_compat_archive_path" | awk '{print $1}')" != "$JPEG_COMPAT_SHA256" ]; then
    jpeg_compat_archive_tmp="$jpeg_compat_archive_path.tmp.$$"
    rm -f "$jpeg_compat_archive_tmp"
    curl --fail --location --retry 3 --output "$jpeg_compat_archive_tmp" "$JPEG_COMPAT_URL"
    mv -f "$jpeg_compat_archive_tmp" "$jpeg_compat_archive_path"
fi

actual_jpeg_compat_sha256="$(sha256sum "$jpeg_compat_archive_path" | awk '{print $1}')"
[ "$actual_jpeg_compat_sha256" = "$JPEG_COMPAT_SHA256" ] || {
    printf 'error: JPEG compatibility source SHA-256 mismatch: expected %s, got %s\n' \
        "$JPEG_COMPAT_SHA256" "$actual_jpeg_compat_sha256" >&2
    exit 1
}

readline_compat_archive_path="$CACHE_DIR/$READLINE_COMPAT_ARCHIVE"
if [ ! -f "$readline_compat_archive_path" ] ||
   [ "$(sha256sum "$readline_compat_archive_path" | awk '{print $1}')" != "$READLINE_COMPAT_SHA256" ]; then
    readline_compat_archive_tmp="$readline_compat_archive_path.tmp.$$"
    rm -f "$readline_compat_archive_tmp"
    curl --fail --location --retry 3 --output "$readline_compat_archive_tmp" "$READLINE_COMPAT_URL"
    mv -f "$readline_compat_archive_tmp" "$readline_compat_archive_path"
fi

actual_readline_compat_sha256="$(sha256sum "$readline_compat_archive_path" | awk '{print $1}')"
[ "$actual_readline_compat_sha256" = "$READLINE_COMPAT_SHA256" ] || {
    printf 'error: Readline compatibility source SHA-256 mismatch: expected %s, got %s\n' \
        "$READLINE_COMPAT_SHA256" "$actual_readline_compat_sha256" >&2
    exit 1
}

theora_compat_archive_path="$CACHE_DIR/$THEORA_COMPAT_ARCHIVE"
if [ ! -f "$theora_compat_archive_path" ] ||
   [ "$(sha256sum "$theora_compat_archive_path" | awk '{print $1}')" != "$THEORA_COMPAT_SHA256" ]; then
    theora_compat_archive_tmp="$theora_compat_archive_path.tmp.$$"
    rm -f "$theora_compat_archive_tmp"
    curl --fail --location --retry 3 --output "$theora_compat_archive_tmp" "$THEORA_COMPAT_URL"
    mv -f "$theora_compat_archive_tmp" "$theora_compat_archive_path"
fi

actual_theora_compat_sha256="$(sha256sum "$theora_compat_archive_path" | awk '{print $1}')"
[ "$actual_theora_compat_sha256" = "$THEORA_COMPAT_SHA256" ] || {
    printf 'error: Theora compatibility source SHA-256 mismatch: expected %s, got %s\n' \
        "$THEORA_COMPAT_SHA256" "$actual_theora_compat_sha256" >&2
    exit 1
}

sdl2_mixer_archive_path="$CACHE_DIR/$SDL2_MIXER_ARCHIVE"
if [ ! -f "$sdl2_mixer_archive_path" ] ||
   [ "$(sha256sum "$sdl2_mixer_archive_path" | awk '{print $1}')" != "$SDL2_MIXER_SHA256" ]; then
    sdl2_mixer_archive_tmp="$sdl2_mixer_archive_path.tmp.$$"
    rm -f "$sdl2_mixer_archive_tmp"
    curl --fail --location --retry 3 \
        --output "$sdl2_mixer_archive_tmp" "$SDL2_MIXER_URL"
    mv -f "$sdl2_mixer_archive_tmp" "$sdl2_mixer_archive_path"
fi

actual_sdl2_mixer_sha256="$(sha256sum "$sdl2_mixer_archive_path" | awk '{print $1}')"
[ "$actual_sdl2_mixer_sha256" = "$SDL2_MIXER_SHA256" ] || {
    printf 'error: SDL2_mixer source SHA-256 mismatch: expected %s, got %s\n' \
        "$SDL2_MIXER_SHA256" "$actual_sdl2_mixer_sha256" >&2
    exit 1
}

meson_bootstrap_archive_path="$CACHE_DIR/$MESON_BOOTSTRAP_ARCHIVE"
if [ ! -f "$meson_bootstrap_archive_path" ] ||
   [ "$(sha256sum "$meson_bootstrap_archive_path" | awk '{print $1}')" != "$MESON_BOOTSTRAP_SHA256" ]; then
    meson_bootstrap_archive_tmp="$meson_bootstrap_archive_path.tmp.$$"
    rm -f "$meson_bootstrap_archive_tmp"
    curl --fail --location --retry 3 \
        --output "$meson_bootstrap_archive_tmp" "$MESON_BOOTSTRAP_URL"
    mv -f "$meson_bootstrap_archive_tmp" "$meson_bootstrap_archive_path"
fi

actual_meson_bootstrap_sha256="$(sha256sum "$meson_bootstrap_archive_path" | awk '{print $1}')"
[ "$actual_meson_bootstrap_sha256" = "$MESON_BOOTSTRAP_SHA256" ] || {
    printf 'error: Meson bootstrap source SHA-256 mismatch: expected %s, got %s\n' \
        "$MESON_BOOTSTRAP_SHA256" "$actual_meson_bootstrap_sha256" >&2
    exit 1
}

cairo_compat_archive_path="$CACHE_DIR/$CAIRO_COMPAT_ARCHIVE"
if [ ! -f "$cairo_compat_archive_path" ] ||
   [ "$(sha256sum "$cairo_compat_archive_path" | awk '{print $1}')" != "$CAIRO_COMPAT_SHA256" ]; then
    cairo_compat_archive_tmp="$cairo_compat_archive_path.tmp.$$"
    rm -f "$cairo_compat_archive_tmp"
    curl --fail --location --retry 3 \
        --output "$cairo_compat_archive_tmp" "$CAIRO_COMPAT_URL"
    mv -f "$cairo_compat_archive_tmp" "$cairo_compat_archive_path"
fi

actual_cairo_compat_sha256="$(sha256sum "$cairo_compat_archive_path" | awk '{print $1}')"
[ "$actual_cairo_compat_sha256" = "$CAIRO_COMPAT_SHA256" ] || {
    printf 'error: Cairo compatibility source SHA-256 mismatch: expected %s, got %s\n' \
        "$CAIRO_COMPAT_SHA256" "$actual_cairo_compat_sha256" >&2
    exit 1
}

openal_src="$BUILD_DIR/openal-soft-src"
openal_build="$BUILD_DIR/openal-soft-build"
rm -rf "$openal_src" "$openal_build"
mkdir -p "$openal_src" "$openal_build"
tar -xzf "$openal_archive_path" --strip-components=1 -C "$openal_src"
cmake -S "$openal_src" -B "$openal_build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DALSOFT_BACKEND_ALSA=ON \
    -DALSOFT_REQUIRE_ALSA=ON \
    -DALSOFT_BACKEND_PIPEWIRE=OFF \
    -DALSOFT_BACKEND_PULSEAUDIO=OFF \
    -DALSOFT_BACKEND_JACK=OFF \
    -DALSOFT_BACKEND_OSS=OFF \
    -DALSOFT_BACKEND_SNDIO=OFF \
    -DALSOFT_BACKEND_PORTAUDIO=OFF \
    -DALSOFT_BACKEND_SDL2=OFF \
    -DALSOFT_BACKEND_SDL3=OFF \
    -DALSOFT_BACKEND_WAVE=OFF \
    -DALSOFT_DLOPEN=OFF \
    -DALSOFT_EMBED_HRTF_DATA=OFF \
    -DALSOFT_UTILS=OFF \
    -DALSOFT_NO_CONFIG_UTIL=ON \
    -DALSOFT_EXAMPLES=OFF \
    -DALSOFT_TESTS=OFF \
    -DALSOFT_INSTALL=OFF
cmake --build "$openal_build" --parallel "${JOBS:-2}" --target OpenAL
openal_library="$(find "$openal_build" -type f -name 'libopenal.so.*' | sort | tail -n 1)"
[ -n "$openal_library" ] && [ -f "$openal_library" ] || {
    printf 'error: OpenAL Soft shared library was not produced\n' >&2
    exit 1
}

ffmpeg_compat_src="$BUILD_DIR/ffmpeg-compat-src"
ffmpeg_compat_build="$BUILD_DIR/ffmpeg-compat-build"
ffmpeg_compat_install="$BUILD_DIR/ffmpeg-compat-install"
rm -rf "$ffmpeg_compat_src" "$ffmpeg_compat_build" "$ffmpeg_compat_install"
mkdir -p "$ffmpeg_compat_src" "$ffmpeg_compat_build" "$ffmpeg_compat_install"
tar -xJf "$ffmpeg_compat_archive_path" --strip-components=1 -C "$ffmpeg_compat_src"
(
    cd "$ffmpeg_compat_build"
    "$ffmpeg_compat_src/configure" \
        --prefix=/usr \
        --arch=aarch64 \
        --target-os=linux \
        --disable-programs \
        --disable-doc \
        --disable-debug \
        --disable-static \
        --enable-shared \
        --enable-pic \
        --disable-autodetect \
        --disable-avdevice \
        --disable-avfilter \
        --disable-postproc \
        --disable-network \
        --disable-encoders \
        --disable-muxers \
        --disable-filters \
        --disable-devices \
        --disable-hwaccels \
        --disable-protocols \
        --enable-protocol=file
    make -j"${JOBS:-2}"
    make DESTDIR="$ffmpeg_compat_install" install
)

libevdev_src="$BUILD_DIR/libevdev-src"
libevdev_build="$BUILD_DIR/libevdev-build"
libevdev_install="$BUILD_DIR/libevdev-install"
rm -rf "$libevdev_src" "$libevdev_build" "$libevdev_install"
mkdir -p "$libevdev_src" "$libevdev_install"
tar -xJf "$libevdev_archive_path" --strip-components=1 -C "$libevdev_src"
meson setup "$libevdev_build" "$libevdev_src" \
    --buildtype=release \
    --default-library=shared \
    --prefix=/usr \
    -Dtests=disabled \
    -Ddocumentation=disabled
meson compile -C "$libevdev_build"
DESTDIR="$libevdev_install" meson install -C "$libevdev_build"

flac_compat_src="$BUILD_DIR/flac-compat-src"
flac_compat_build="$BUILD_DIR/flac-compat-build"
flac_compat_install="$BUILD_DIR/flac-compat-install"
rm -rf "$flac_compat_src" "$flac_compat_build" "$flac_compat_install"
mkdir -p "$flac_compat_src" "$flac_compat_build" "$flac_compat_install"
tar -xJf "$flac_compat_archive_path" --strip-components=1 -C "$flac_compat_src"
(
    cd "$flac_compat_build"
    "$flac_compat_src/configure" \
        --prefix=/usr \
        --disable-static \
        --enable-shared \
        --disable-cpplibs \
        --disable-examples \
        --disable-doxygen-docs
    make -C src/libFLAC -j"${JOBS:-2}"
    make -C src/libFLAC DESTDIR="$flac_compat_install" install
)

jpeg_compat_src="$BUILD_DIR/jpeg-compat-src"
jpeg_compat_build="$BUILD_DIR/jpeg-compat-build"
jpeg_compat_install="$BUILD_DIR/jpeg-compat-install"
rm -rf "$jpeg_compat_src" "$jpeg_compat_build" "$jpeg_compat_install"
mkdir -p "$jpeg_compat_src" "$jpeg_compat_build" "$jpeg_compat_install"
tar -xzf "$jpeg_compat_archive_path" --strip-components=1 -C "$jpeg_compat_src"
config_guess_path=""
config_sub_path=""
for candidate in \
    "$flac_compat_src/config.guess" \
    /usr/share/misc/config.guess \
    /usr/share/automake-*/config.guess; do
    if [ -f "$candidate" ]; then
        config_guess_path="$candidate"
        break
    fi
done
for candidate in \
    "$flac_compat_src/config.sub" \
    /usr/share/misc/config.sub \
    /usr/share/automake-*/config.sub; do
    if [ -f "$candidate" ]; then
        config_sub_path="$candidate"
        break
    fi
done
[ -n "$config_guess_path" ] && [ -n "$config_sub_path" ] || {
    printf 'error: current GNU config.guess/config.sub were not found in the toolchain\n' >&2
    exit 1
}
install -m 0755 "$config_guess_path" "$jpeg_compat_src/config.guess"
install -m 0755 "$config_sub_path" "$jpeg_compat_src/config.sub"
(
    cd "$jpeg_compat_build"
    "$jpeg_compat_src/configure" \
        --prefix=/usr \
        --disable-static \
        --enable-shared
    make -j"${JOBS:-2}"
    make DESTDIR="$jpeg_compat_install" install
)

readline_compat_src="$BUILD_DIR/readline-compat-src"
readline_compat_build="$BUILD_DIR/readline-compat-build"
readline_compat_install="$BUILD_DIR/readline-compat-install"
rm -rf "$readline_compat_src" "$readline_compat_build" "$readline_compat_install"
mkdir -p "$readline_compat_src" "$readline_compat_build" "$readline_compat_install"
tar -xzf "$readline_compat_archive_path" --strip-components=1 -C "$readline_compat_src"
(
    cd "$readline_compat_build"
    "$readline_compat_src/configure" \
        --prefix=/usr \
        --disable-static \
        --enable-shared \
        --with-curses
    make -j"${JOBS:-2}" SHLIB_LIBS="-ltinfo"
    make DESTDIR="$readline_compat_install" SHLIB_LIBS="-ltinfo" install-shared
)

theora_compat_src="$BUILD_DIR/theora-compat-src"
theora_compat_build="$BUILD_DIR/theora-compat-build"
theora_compat_install="$BUILD_DIR/theora-compat-install"
rm -rf "$theora_compat_src" "$theora_compat_build" "$theora_compat_install"
mkdir -p "$theora_compat_src" "$theora_compat_build" "$theora_compat_install"
tar -xJf "$theora_compat_archive_path" --strip-components=1 -C "$theora_compat_src"
install -m 0755 /usr/share/misc/config.guess "$theora_compat_src/config.guess"
install -m 0755 /usr/share/misc/config.sub "$theora_compat_src/config.sub"
(
    cd "$theora_compat_build"
    "$theora_compat_src/configure" \
        --prefix=/usr \
        --disable-static \
        --enable-shared \
        --disable-encode \
        --disable-examples \
        --disable-spec \
        --disable-oggtest \
        --disable-vorbistest
    make -j"${JOBS:-2}"
    make DESTDIR="$theora_compat_install" install
)

sdl2_mixer_src="$BUILD_DIR/sdl2-mixer-src"
sdl2_mixer_build="$BUILD_DIR/sdl2-mixer-build"
rm -rf "$sdl2_mixer_src" "$sdl2_mixer_build"
mkdir -p "$sdl2_mixer_src" "$sdl2_mixer_build"
tar -xzf "$sdl2_mixer_archive_path" --strip-components=1 \
    -C "$sdl2_mixer_src"
cmake -S "$sdl2_mixer_src" -B "$sdl2_mixer_build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DSDL2MIXER_INSTALL=OFF \
    -DSDL2MIXER_SAMPLES=OFF \
    -DSDL2MIXER_CMD=OFF \
    -DSDL2MIXER_FLAC=ON \
    -DSDL2MIXER_FLAC_DRFLAC=ON \
    -DSDL2MIXER_FLAC_LIBFLAC=OFF \
    -DSDL2MIXER_MOD=ON \
    -DSDL2MIXER_MOD_MODPLUG=ON \
    -DSDL2MIXER_MOD_MODPLUG_SHARED=ON \
    -DSDL2MIXER_MOD_XMP=OFF \
    -DSDL2MIXER_MP3=ON \
    -DSDL2MIXER_MP3_DRMP3=ON \
    -DSDL2MIXER_MP3_MPG123=OFF \
    -DSDL2MIXER_MIDI=ON \
    -DSDL2MIXER_MIDI_FLUIDSYNTH=OFF \
    -DSDL2MIXER_MIDI_NATIVE=OFF \
    -DSDL2MIXER_MIDI_TIMIDITY=ON \
    -DSDL2MIXER_OPUS=OFF \
    -DSDL2MIXER_VORBIS=STB \
    -DSDL2MIXER_WAVE=ON
cmake --build "$sdl2_mixer_build" --parallel "${JOBS:-2}" \
    --target SDL2_mixer
sdl2_mixer_library="$(find "$sdl2_mixer_build" -type f \
    -name 'libSDL2_mixer-2.0.so.*' | sort | tail -n 1)"
[ -n "$sdl2_mixer_library" ] && [ -f "$sdl2_mixer_library" ] || {
    printf 'error: constrained SDL2_mixer shared library was not produced\n' >&2
    exit 1
}
if readelf -d "$sdl2_mixer_library" |
   grep -Eq 'Shared library: \[(libfluidsynth|libpulse|libdbus)'; then
    printf 'error: constrained SDL2_mixer unexpectedly requires FluidSynth/PulseAudio/DBus\n' >&2
    exit 1
fi

meson_bootstrap_src="$BUILD_DIR/meson-bootstrap-src"
cairo_compat_src="$BUILD_DIR/cairo-compat-src"
cairo_compat_build="$BUILD_DIR/cairo-compat-build"
rm -rf "$meson_bootstrap_src" "$cairo_compat_src" "$cairo_compat_build"
mkdir -p "$meson_bootstrap_src" "$cairo_compat_src"
tar -xzf "$meson_bootstrap_archive_path" --strip-components=1 \
    -C "$meson_bootstrap_src"
tar -xJf "$cairo_compat_archive_path" --strip-components=1 \
    -C "$cairo_compat_src"
SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-0}" \
python3 "$meson_bootstrap_src/meson.py" setup \
    "$cairo_compat_build" "$cairo_compat_src" \
    --prefix=/usr \
    --libdir=lib \
    --buildtype=release \
    --default-library=shared \
    -Dfontconfig=disabled \
    -Dfreetype=enabled \
    -Dpng=enabled \
    -Dtee=disabled \
    -Dxcb=disabled \
    -Dxlib=disabled \
    -Dxlib-xcb=disabled \
    -Dzlib=enabled \
    -Dtests=disabled \
    -Dlzo=disabled \
    -Dgtk2-utils=disabled \
    -Dglib=disabled \
    -Dspectre=disabled \
    -Dsymbol-lookup=disabled \
    -Dgtk_doc=false
SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-0}" \
python3 "$meson_bootstrap_src/meson.py" compile \
    -C "$cairo_compat_build" cairo
cairo_compat_library="$(find "$cairo_compat_build/src" -type f \
    -name 'libcairo.so.2.*' | sort | tail -n 1)"
[ -n "$cairo_compat_library" ] && [ -f "$cairo_compat_library" ] || {
    printf 'error: Cairo compatibility shared library was not produced\n' >&2
    exit 1
}
readelf -d "$cairo_compat_library" |
    grep -q 'Shared library: \[libpixman-1.so.0\]' || {
        printf 'error: Cairo compatibility library does not use pixman\n' >&2
        exit 1
    }
if readelf -d "$cairo_compat_library" |
   grep -Eq 'Shared library: \[(libX11|libxcb)'; then
    printf 'error: Cairo compatibility library unexpectedly requires X11/XCB\n' >&2
    exit 1
fi

python3 - "$archive_path" <<'PY'
import stat
import sys
import zipfile
from pathlib import PurePosixPath

archive = sys.argv[1]
required = {
    "PortMaster/pugwash",
    "PortMaster/pylibs.zip",
    "PortMaster/control.txt",
    "PortMaster/device_info.txt",
    "PortMaster/funcs.txt",
    "PortMaster/gptokeyb",
    "PortMaster/gptokeyb2",
    "PortMaster/version",
}

with zipfile.ZipFile(archive) as zf:
    names = set()
    for entry in zf.infolist():
        path = PurePosixPath(entry.filename)
        if path.is_absolute() or ".." in path.parts:
            raise SystemExit(f"unsafe archive path: {entry.filename}")
        mode = entry.external_attr >> 16
        if stat.S_ISLNK(mode):
            raise SystemExit(f"symlink is not allowed in release archive: {entry.filename}")
        names.add(entry.filename.rstrip("/"))

    missing = sorted(required - names)
    if missing:
        raise SystemExit("missing required release files: " + ", ".join(missing))
    self_update_call = b"            if portmaster_check_update(pm, config, temp_dir):"
    count = zf.read("PortMaster/pugwash").count(self_update_call)
    if count != 1:
        raise SystemExit(
            "unsupported PortMaster self-update call layout: "
            f"expected one call, found {count}"
        )
PY

stage_dir="$BUILD_DIR/stage.$$"
rm -rf "$stage_dir"
mkdir -p "$stage_dir/plumos/apps/portmaster/upstream"
unzip -q "$archive_path" -d "$stage_dir/plumos/apps/portmaster/upstream"
upstream_portmaster="$stage_dir/plumos/apps/portmaster/upstream/PortMaster"

# The official archive carries launch adapters for many unrelated firmware
# distributions.  Pixel2 never selects those adapters because plumOS injects
# its own contract, and exposing their names inside the image is misleading.
# Keep the common PortMaster runtime, but remove the unused adapter surface.
for foreign_dir in .Backup batocera knulli miyoo muos retrodeck trimui; do
    rm -rf "$upstream_portmaster/$foreign_dir"
done
for foreign_file in \
    libgl_Batocera.txt libgl_EmuELEC.txt libgl_JELOS.txt libgl_Miyoo.txt \
    libgl_REGLinux.txt libgl_ROCKNIX.txt libgl_UnofficialOS.txt \
    libgl_knulli.txt libgl_muOS.txt libgl_uConsole.txt \
    mod_ArkOS.txt 'mod_ArkOS wuMMLe.txt' mod_Batocera.txt mod_EmuELEC.txt \
    mod_JELOS.txt mod_Miyoo.txt mod_REGLinux.txt mod_ROCKNIX.txt \
    mod_TrimUI.txt mod_UnofficialOS.txt mod_dArkOS.txt mod_dArkOSRE.txt \
    mod_knulli.txt mod_muOS.txt; do
    rm -f "$upstream_portmaster/$foreign_file"
done

release_version="$(tr -d '\r\n' < "$upstream_portmaster/version")"
[ "$release_version" = "$VERSION" ] || {
    printf 'error: archive version mismatch: expected %s, got %s\n' \
        "$VERSION" "$release_version" >&2
    rm -rf "$stage_dir"
    exit 1
}

chmod 0755 \
    "$stage_dir/plumos/apps/portmaster/upstream/PortMaster/gptokeyb" \
    "$stage_dir/plumos/apps/portmaster/upstream/PortMaster/gptokeyb2"

rsync -a --copy-links --exclude='__pycache__/' --exclude='*.pyc' \
    "$PACKAGE_DIR/plumos/" "$stage_dir/plumos/"
find "$stage_dir/plumos/bin" \
    "$stage_dir/plumos/apps/portmaster/adapter/shims" \
    -type f -exec chmod 0755 {} +
chmod 0755 \
    "$stage_dir/plumos/apps/portmaster/adapter/control.txt" \
    "$stage_dir/plumos/apps/portmaster/adapter/mod_plumOS.txt" \
    "$stage_dir/plumos/apps/portmaster/adapter/plumos_portmaster_bootstrap.py" \
    "$stage_dir/plumos/apps/portmaster/adapter/plumos_portmaster_update.py"
mkdir -p \
    "$stage_dir/plumos/apps/portmaster/adapter/bin/aarch64" \
    "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64" \
    "$stage_dir/plumos/licenses"
cc -O2 -fPIC -Wall -Wextra -Werror -shared -I/usr/include/SDL2 \
    -Wl,-z,defs -Wl,-soname,libplumos-portmaster-sdl-rotate.so \
    -o "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/libplumos-portmaster-sdl-rotate.so" \
    "$PACKAGE_DIR/src/plumos_portmaster_sdl_rotate.c" -ldl
install -m 0755 /usr/bin/unsquashfs \
    "$stage_dir/plumos/apps/portmaster/adapter/bin/aarch64/unsquashfs"
install -m 0755 /usr/bin/zip \
    "$stage_dir/plumos/apps/portmaster/adapter/bin/aarch64/zip"
install -m 0755 /bin/bash \
    "$stage_dir/plumos/apps/portmaster/adapter/bin/aarch64/bash"
install -m 0644 /usr/share/doc/bash/copyright \
    "$stage_dir/plumos/licenses/bash-copyright.txt"
install -m 0644 "$openal_library" \
    "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/libopenal.so.1"
install -m 0644 "$lzo_library" \
    "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/liblzo2.so.2"
theora_compat_library="$(find "$theora_compat_install/usr/lib" -type f -name 'libtheoradec.so.1.*' | sort | tail -n 1)"
[ -n "$theora_compat_library" ] && [ -f "$theora_compat_library" ] || {
    printf 'error: Theora compatibility library was not produced\n' >&2
    exit 1
}
install -m 0644 "$theora_compat_library" \
    "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/libtheoradec.so.1"
for library in \
    libavcodec.so.58 \
    libavformat.so.58 \
    libavutil.so.56 \
    libswresample.so.3 \
    libswscale.so.5; do
    source_library="$(find "$ffmpeg_compat_install/usr/lib" -type f -name "${library}.*" | sort | tail -n 1)"
    [ -n "$source_library" ] && [ -f "$source_library" ] || {
        printf 'error: FFmpeg compatibility library was not produced: %s\n' "$library" >&2
        exit 1
    }
    install -m 0644 "$source_library" \
        "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/$library"
done
libevdev_library="$(find "$libevdev_install/usr/lib" -type f -name 'libevdev.so.2.*' | sort | tail -n 1)"
[ -n "$libevdev_library" ] && [ -f "$libevdev_library" ] || {
    printf 'error: libevdev shared library was not produced\n' >&2
    exit 1
}
install -m 0644 "$libevdev_library" \
    "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/libevdev.so.2"
flac_compat_library="$(find "$flac_compat_install/usr/lib" -type f -name 'libFLAC.so.8.*' | sort | tail -n 1)"
[ -n "$flac_compat_library" ] && [ -f "$flac_compat_library" ] || {
    printf 'error: FLAC compatibility library was not produced\n' >&2
    exit 1
}
install -m 0644 "$flac_compat_library" \
    "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/libFLAC.so.8"
jpeg_compat_library="$(find "$jpeg_compat_install/usr/lib" -type f -name 'libjpeg.so.8.*' | sort | tail -n 1)"
[ -n "$jpeg_compat_library" ] && [ -f "$jpeg_compat_library" ] || {
    printf 'error: JPEG compatibility library was not produced\n' >&2
    exit 1
}
install -m 0644 "$jpeg_compat_library" \
    "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/libjpeg.so.8"
readline_compat_library="$(find "$readline_compat_install/usr/lib" -type f -name 'libreadline.so.7.*' | sort | tail -n 1)"
[ -n "$readline_compat_library" ] && [ -f "$readline_compat_library" ] || {
    printf 'error: Readline compatibility library was not produced\n' >&2
    exit 1
}
install -m 0644 "$readline_compat_library" \
    "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/libreadline.so.7"
install -m 0644 "$cairo_compat_library" \
    "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/libcairo.so.2"
install -m 0644 "$pixman_library" \
    "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/libpixman-1.so.0"
for library in libSDL2_gfx-1.0.so.0; do
    source_library="$(find /usr/lib/aarch64-linux-gnu -type f \
        -name "${library}*" | sort | tail -n 1)"
    [ -n "$source_library" ] && [ -f "$source_library" ] || {
        printf 'error: PortMaster SDL compatibility library is missing: %s\n' \
            "$library" >&2
        exit 1
    }
    install -m 0644 "$source_library" \
        "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/$library"
done
install -m 0644 "$sdl2_mixer_library" \
    "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/libSDL2_mixer-2.0.so.0"
install -m 0644 \
    "$stage_dir/plumos/apps/portmaster/upstream/PortMaster/runtimes/love_11.5/libs.aarch64/libmodplug.so.1" \
    "$stage_dir/plumos/apps/portmaster/adapter/lib/aarch64/libmodplug.so.1"
install -m 0644 "$openal_src/COPYING" \
    "$stage_dir/plumos/licenses/openal-soft-LGPL-2.0-or-later.txt"
install -m 0644 "$ffmpeg_compat_src/COPYING.LGPLv2.1" \
    "$stage_dir/plumos/licenses/ffmpeg-compat-LGPL-2.1-or-later.txt"
install -m 0644 "$libevdev_src/COPYING" \
    "$stage_dir/plumos/licenses/libevdev-MIT.txt"
install -m 0644 "$flac_compat_src/COPYING.Xiph" \
    "$stage_dir/plumos/licenses/flac-compat-Xiph-BSD.txt"
install -m 0644 "$jpeg_compat_src/README" \
    "$stage_dir/plumos/licenses/libjpeg-compat-IJG.txt"
install -m 0644 "$readline_compat_src/COPYING" \
    "$stage_dir/plumos/licenses/readline-compat-GPL-3.0-or-later.txt"
install -m 0644 /usr/share/doc/squashfs-tools/copyright \
    "$stage_dir/plumos/licenses/squashfs-tools-copyright.txt"
install -m 0644 /usr/share/doc/zip/copyright \
    "$stage_dir/plumos/licenses/zip-copyright.txt"
install -m 0644 /usr/share/doc/liblzo2-2/copyright \
    "$stage_dir/plumos/licenses/liblzo2-copyright.txt"
install -m 0644 "$theora_compat_src/COPYING" \
    "$stage_dir/plumos/licenses/libtheora-Xiph-BSD.txt"
install -m 0644 "$cairo_compat_src/COPYING-LGPL-2.1" \
    "$stage_dir/plumos/licenses/cairo-LGPL-2.1.txt"
install -m 0644 "$cairo_compat_src/COPYING-MPL-1.1" \
    "$stage_dir/plumos/licenses/cairo-MPL-1.1.txt"
install -m 0644 /usr/share/doc/libpixman-1-0/copyright \
    "$stage_dir/plumos/licenses/libpixman-copyright.txt"
install -m 0644 /usr/share/doc/libsdl2-gfx-1.0-0/copyright \
    "$stage_dir/plumos/licenses/libsdl2-gfx-copyright.txt"
install -m 0644 "$sdl2_mixer_src/LICENSE.txt" \
    "$stage_dir/plumos/licenses/sdl2-mixer-Zlib.txt"
mkdir -p \
    "$stage_dir/plumos/state/portmaster/config" \
    "$stage_dir/plumos/state/portmaster/libs" \
    "$stage_dir/plumos/state/portmaster/themes" \
    "$stage_dir/plumos/state/portmaster/home" \
    "$stage_dir/plumos/logs/apps"

generated_at="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
source_ref="$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || printf unknown)"

cat > "$stage_dir/plumos/apps/portmaster/installed.json" <<EOF
{
  "adapter_version": ${ADAPTER_VERSION},
  "channel": "stable",
  "official_md5": "${actual_md5}",
  "official_sha256": "${actual_sha256}",
  "source_url": "${URL}",
  "version": "${VERSION}"
}
EOF

mkdir -p "$stage_dir/plumos/components/portmaster"
cat > "$stage_dir/plumos/components/portmaster/manifest.json" <<EOF
{
  "name": "plumOS PortMaster for Pixel2",
  "component": "portmaster",
  "device": "pixel2",
  "architecture": "aarch64",
  "version": "${VERSION}",
  "adapter_version": ${ADAPTER_VERSION},
  "source_ref": "${source_ref}",
  "generated_at": "${generated_at}",
  "upstream_url": "${URL}",
  "upstream_sha256": "${actual_sha256}",
  "cairo_compat_version": "${CAIRO_COMPAT_VERSION}",
  "sdl2_mixer_version": "${SDL2_MIXER_VERSION}",
  "runtime_abi": "plumos-pixel2-portmaster-v1",
  "depends_on": ["frontend", "nextcommander", "pyxel"],
  "display": "SDL2 KMSDRM 640x480",
  "audio": "ALSA plumos_output",
  "input": "plumOS Pixel2 Controller",
  "mutable_data": "state/portmaster"
}
EOF

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
rsync -a "$stage_dir/plumos/" "$OUT_DIR/plumos/"

(
    cd "$OUT_DIR/plumos"
    find apps/portmaster bin licenses components/portmaster/manifest.json \
        -type f \
        \( -path 'bin/plumos-portmaster-*' \
        -o -path 'apps/portmaster/*' \
        -o -path 'licenses/openal-soft-*' \
        -o -path 'licenses/ffmpeg-compat-*' \
        -o -path 'licenses/libevdev-*' \
        -o -path 'licenses/flac-compat-*' \
        -o -path 'licenses/libjpeg-compat-*' \
        -o -path 'licenses/readline-compat-*' \
        -o -path 'licenses/squashfs-tools-*' \
        -o -path 'licenses/zip-*' \
        -o -path 'licenses/bash-*' \
        -o -path 'licenses/liblzo2-*' \
        -o -path 'licenses/libtheora-*' \
        -o -path 'licenses/cairo-*' \
        -o -path 'licenses/libpixman-*' \
        -o -path 'licenses/libsdl2-gfx-*' \
        -o -path 'licenses/sdl2-mixer-*' \
        -o -path 'components/portmaster/manifest.json' \) \
        -print |
        sort |
        while IFS= read -r path; do sha256sum "$path"; done
) > "$OUT_DIR/plumos/components/portmaster/checksums.sha256"

cat > "$OUT_DIR/portmaster.manifest" <<EOF
component=portmaster
target=gkd-pixel2
adapter_version=${ADAPTER_VERSION}
upstream=PortMaster-GUI
version=${VERSION}
channel=stable
url=${URL}
md5=${actual_md5}
sha256=${actual_sha256}
update_policy=plumos-staged-switch
openal_soft_version=${OPENAL_VERSION}
openal_soft_commit=${OPENAL_COMMIT}
openal_soft_sha256=${actual_openal_sha256}
ffmpeg_compat_version=${FFMPEG_COMPAT_VERSION}
ffmpeg_compat_sha256=${actual_ffmpeg_compat_sha256}
libevdev_version=${LIBEVDEV_VERSION}
libevdev_sha256=${actual_libevdev_sha256}
flac_compat_version=${FLAC_COMPAT_VERSION}
flac_compat_sha256=${actual_flac_compat_sha256}
jpeg_compat_version=${JPEG_COMPAT_VERSION}
jpeg_compat_sha256=${actual_jpeg_compat_sha256}
readline_compat_version=${READLINE_COMPAT_VERSION}
readline_compat_sha256=${actual_readline_compat_sha256}
meson_bootstrap_version=${MESON_BOOTSTRAP_VERSION}
meson_bootstrap_sha256=${actual_meson_bootstrap_sha256}
cairo_compat_version=${CAIRO_COMPAT_VERSION}
sdl2_mixer_version=${SDL2_MIXER_VERSION}
sdl2_mixer_sha256=${actual_sdl2_mixer_sha256}
cairo_compat_sha256=${actual_cairo_compat_sha256}
pixman_runtime_version=${actual_pixman_runtime_version}
squashfs_tools_version=${actual_squashfs_tools_version}
zip_version=${actual_zip_version}
unsquashfs_sha256=$(sha256sum /usr/bin/unsquashfs | awk '{print $1}')
zip_sha256=$(sha256sum /usr/bin/zip | awk '{print $1}')
EOF

find "$OUT_DIR" -type f ! -name checksums.sha256 -print0 | sort -z | \
    xargs -0 sha256sum | sed "s#  $OUT_DIR/#  #" > "$OUT_DIR/checksums.sha256"

(
    cd "$OUT_DIR/plumos"
    sha256sum -c components/portmaster/checksums.sha256
)

rm -rf "$stage_dir"
printf 'created: %s\n' "$OUT_DIR"
printf 'PortMaster: %s (%s)\n' "$VERSION" "$actual_sha256"
