#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"

if [ "${1:-}" != --inside ]; then
    DEST="${1:-$ROOT_DIR/output/adbd/pixel2/rootfs-overlay}"
    case "$DEST" in
        "$ROOT_DIR"/*) ;;
        *) printf 'error: destination must be under repository: %s\n' "$DEST" >&2; exit 2 ;;
    esac
    relative=${DEST#"$ROOT_DIR"/}
    docker image inspect "$IMAGE" >/dev/null 2>&1 || "$ROOT_DIR/scripts/build-tools-image.sh"
    exec docker run --rm --platform linux/arm64 \
        -v "$ROOT_DIR:/work" -w /work "$IMAGE" \
        ./scripts/build-adbd-overlay.sh --inside "/work/$relative"
fi

DEST="${2:?missing destination}"
VERSION=29.0.6
REVISION=28
BASE_URL=https://deb.debian.org/debian/pool/main/a/android-platform-tools
DSC="android-platform-tools_${VERSION}-${REVISION}.dsc"
ORIG="android-platform-tools_${VERSION}.orig.tar.gz"
DEBIAN="android-platform-tools_${VERSION}-${REVISION}.debian.tar.xz"
DSC_SHA=a322e569f5f4d57c4b7ef1486182431be787895f60dc00b6d3ff489f894f97e4
ORIG_SHA=dbd241642af17fe545a91c4b54d79867f94abfb64c1f4ed759aea5b85334a633
DEBIAN_SHA=ae5d25152029cbdfc5c1b846a81d23ea30067a2638a94fa3cb9a8c867cf88bae
CACHE=/work/.cache/adbd/android-platform-tools_${VERSION}-${REVISION}

mkdir -p "$CACHE"
download() {
    [ -s "$CACHE/$1" ] || curl -fsSL "$BASE_URL/$1" -o "$CACHE/$1"
}
download "$DSC"
download "$ORIG"
download "$DEBIAN"
(cd "$CACHE" && printf '%s  %s\n' "$DSC_SHA" "$DSC" | sha256sum -c - >/dev/null)
(cd "$CACHE" && printf '%s  %s\n' "$ORIG_SHA" "$ORIG" | sha256sum -c - >/dev/null)
(cd "$CACHE" && printf '%s  %s\n' "$DEBIAN_SHA" "$DEBIAN" | sha256sum -c - >/dev/null)

work=/tmp/plumos-pixel2-adbd
rm -rf "$work"
mkdir -p "$work"
trap 'rm -rf "$work"' EXIT
src="$work/android-platform-tools-$VERSION"
dpkg-source -x "$CACHE/$DSC" "$src" >/dev/null

perl -0pi -e \
  's/#if defined\(__ANDROID__\)\n    if \(access\(USB_FFS_ADB_EP0, F_OK\) == 0\) \{/#if defined(__ANDROID__) || defined(PLUMOS_ADBD_USB_FFS)\n    if (access(USB_FFS_ADB_EP0, F_OK) == 0) {/g' \
  "$src/system/core/adb/daemon/main.cpp"
grep -q PLUMOS_ADBD_USB_FFS "$src/system/core/adb/daemon/main.cpp"
build="$work/build"
mkdir -p "$build"
cxxflags=(
  -std=gnu++2a -fPIC -fno-exceptions -fno-strict-aliasing
  -no-canonical-prefixes -fmessage-length=0 -Wno-c++11-narrowing
  -DNDEBUG -UDEBUG -D_GNU_SOURCE -DADB_HOST=0 -DALLOW_ADBD_ROOT=1
  -DALLOW_ADBD_NO_AUTH -DPLUMOS_ADBD_USB_FFS -DPAGE_SIZE=4096
  "-DADB_VERSION=\"${VERSION}-plumos\""
  "-DPLATFORM_TOOLS_VERSION=\"${VERSION}\""
  -I/usr/include/android -Isystem/core/adb -Isystem/core/adb/daemon/include
  -Isystem/core/adb/adbconnection/include -Isystem/core/diagnose_usb/include
  -Isystem/core/libasyncio/include -Isystem/core/base/include -Isystem/core/include
)
sources=(
  system/core/libasyncio/AsyncIO.cpp
  system/core/diagnose_usb/diagnose_usb.cpp
  system/core/adb/adb.cpp system/core/adb/adb_io.cpp
  system/core/adb/adb_listeners.cpp system/core/adb/adb_trace.cpp
  system/core/adb/adb_unique_fd.cpp system/core/adb/adb_utils.cpp
  system/core/adb/fdevent/fdevent.cpp system/core/adb/fdevent/fdevent_poll.cpp
  system/core/adb/fdevent/fdevent_epoll.cpp system/core/adb/services.cpp
  system/core/adb/sockets.cpp system/core/adb/socket_spec.cpp
  system/core/adb/sysdeps/errno.cpp system/core/adb/sysdeps_unix.cpp
  system/core/adb/sysdeps/posix/network.cpp system/core/adb/transport.cpp
  system/core/adb/transport_fd.cpp system/core/adb/transport_local.cpp
  system/core/adb/transport_usb.cpp system/core/adb/types.cpp
  system/core/adb/adbconnection/adbconnection_server.cpp
  /work/package/adbd/adbd_auth_stub.cpp
  system/core/adb/daemon/jdwp_service.cpp system/core/adb/daemon/usb.cpp
  system/core/adb/daemon/usb_ffs.cpp system/core/adb/daemon/usb_legacy.cpp
  system/core/adb/daemon/file_sync_service.cpp system/core/adb/daemon/services.cpp
  system/core/adb/daemon/shell_service.cpp system/core/adb/shell_service_protocol.cpp
  system/core/adb/daemon/main.cpp
)
objects=()
index=0
for item in "${sources[@]}"; do
    case "$item" in /*) path=$item ;; *) path="$src/$item" ;; esac
    object="$build/$index.o"
    (cd "$src" && clang++ "${cxxflags[@]}" -c "$path" -o "$object")
    objects+=("$object")
    index=$((index + 1))
done

rm -rf "$DEST/usr/lib/plumos/adbd" "$DEST/usr/share/licenses/adbd"
rm -f "$DEST/usr/sbin/adbd"
mkdir -p "$DEST/usr/lib/plumos/adbd/lib" "$DEST/usr/sbin" \
    "$DEST/usr/share/licenses/adbd"
binary="$DEST/usr/lib/plumos/adbd/adbd.bin"
clang++ -o "$binary" "${objects[@]}" -L/usr/lib/aarch64-linux-gnu/android \
  -lbase -lcutils -llog -lcrypto -lutils -lcap -lselinux \
  -lpthread -lresolv -lutil -pie
strip "$binary"

copied=' '
copy_needed() {
    elf=$1
    readelf -d "$elf" 2>/dev/null | awk '/NEEDED/ { gsub(/[][]/, "", $5); print $5 }' | \
    while IFS= read -r soname; do
        case "$copied" in *" $soname "*) continue ;; esac
        path=
        for directory in /usr/lib/aarch64-linux-gnu/android \
            /usr/lib/aarch64-linux-gnu /lib/aarch64-linux-gnu \
            /usr/lib/p7zip /usr/lib /lib; do
            if [ -f "$directory/$soname" ]; then
                path="$directory/$soname"
                break
            fi
        done
        [ -n "$path" ] || {
            printf 'error: adbd runtime library missing: %s\n' "$soname" >&2
            exit 1
        }
        copied="$copied$soname "
        cp -L "$path" "$DEST/usr/lib/plumos/adbd/lib/$soname"
        copy_needed "$path"
    done
}
copy_needed "$binary"
cat >"$DEST/usr/sbin/adbd" <<'EOF'
#!/bin/sh
unset LD_LIBRARY_PATH
exec /lib/ld-linux-aarch64.so.1 \
  --library-path /usr/lib/plumos/adbd/lib:/lib/aarch64-linux-gnu \
  /usr/lib/plumos/adbd/adbd.bin "$@"
EOF
chmod 0755 "$DEST/usr/sbin/adbd"

for package in android-libbase-dev android-libboringssl-dev android-libcutils-dev \
    android-liblog-dev android-libutils-dev; do
    cp "/usr/share/doc/$package/copyright" \
        "$DEST/usr/share/licenses/adbd/$package-copyright"
done
cat >"$DEST/usr/share/licenses/adbd/source.manifest" <<EOF
source_package=android-platform-tools
source_version=$VERSION-$REVISION
source_url=$BASE_URL/$DSC
$DSC_SHA  $DSC
$ORIG_SHA  $ORIG
$DEBIAN_SHA  $DEBIAN
patch=enable FunctionFS daemon path outside Android framework
transport=nonblocking FunctionFS
authentication=disabled for development image only
EOF
printf 'adbd_overlay=result-ok destination=%s\n' "$DEST"
