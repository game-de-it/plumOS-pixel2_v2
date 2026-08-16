#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"

if [ "${1:-}" != --inside ]; then
    docker image inspect "$IMAGE" >/dev/null 2>&1 || "$ROOT_DIR/scripts/build-tools-image.sh"
    exec docker run --rm --platform linux/arm64 \
        -e SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-}" \
        -e PLUMOS_BUILD_JOBS="${PLUMOS_BUILD_JOBS:-}" \
        -v "$ROOT_DIR:/work" -w /work "$IMAGE" \
        ./scripts/build-rtl8821cu-pixel2.sh --inside "$@"
fi

ROOT_DIR=/work
KERNEL_RELEASE=5.10.198
KERNEL_REPO=https://github.com/christianhaitian/linux-5.10.198.git
KERNEL_REF=883a9e03084bf1a2f1769ad6b369f5090bbd6588
DRIVER_REPO=https://github.com/morrownr/8821cu-20210916.git
DRIVER_REF=96c65c58b544241178638e810b333dcc9aa26b91
EXPECTED_STOCK_SRCVERSION=33E331B2DEB16477EAAB1D6
EXPECTED_DRIVER_VERSION=v5.12.0.4-1-g9241a6516.20210916_COEX20200730-5151
CACHE="$ROOT_DIR/.cache/kernel"
KERNEL_SRC="$CACHE/linux-5.10.198-pixel2"
DRIVER_SRC="$CACHE/rtl8821cu-96c65c5"
OUT="$ROOT_DIR/output/kernel-modules/pixel2"
STOCK_MODULE="$ROOT_DIR/artifacts/vendor/pixel2-stock/kernel/extracted/usr/lib/kernel-overlays/base/lib/modules/$KERNEL_RELEASE/kernel/drivers/staging/rtl8188eu/r8188eu.ko"
JOBS="${PLUMOS_BUILD_JOBS:-4}"
SOURCE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[ -n "$SOURCE_EPOCH" ] || SOURCE_EPOCH="$(git -C "$ROOT_DIR" show -s --format=%ct HEAD)"
MODE="${2:-build}"

case "$JOBS" in
    ''|*[!0-9]*|0) printf 'error: PLUMOS_BUILD_JOBS must be a positive integer\n' >&2; exit 2 ;;
esac
case "$SOURCE_EPOCH" in
    ''|*[!0-9]*) printf 'error: invalid SOURCE_DATE_EPOCH\n' >&2; exit 2 ;;
esac
[ -f "$STOCK_MODULE" ] || {
    printf 'error: captured stock r8188eu module missing: %s\n' "$STOCK_MODULE" >&2
    exit 2
}

verify_output() {
    module="$OUT/$KERNEL_RELEASE/extra/8821cu.ko"
    test -f "$module"
    (cd "$OUT" && sha256sum -c checksums.sha256 >/dev/null)
    test "$(modinfo -F name "$module")" = 8821cu
    test "$(modinfo -F version "$module")" = "$EXPECTED_DRIVER_VERSION"
    test "$(modinfo -F vermagic "$module")" = \
        "$KERNEL_RELEASE SMP mod_unload aarch64"
    modinfo -F alias "$module" | grep -Fq 'usb:v0BDApC811'
    modinfo -F alias "$module" | grep -Fq 'usb:v0BDApC820'
    test "$(jq -r .kernel_ref "$OUT/rtl8821cu.json")" = "$KERNEL_REF"
    test "$(jq -r .driver_ref "$OUT/rtl8821cu.json")" = "$DRIVER_REF"
    test "$(jq -r .module_sha256 "$OUT/rtl8821cu.json")" = \
        "$(sha256sum "$module" | awk '{print $1}')"
}

case "$MODE" in
    build) ;;
    --verify-output)
        verify_output
        printf 'rtl8821cu_output=result-ok kernel=%s\n' "$KERNEL_RELEASE"
        exit 0
        ;;
    *) printf 'error: unknown mode: %s\n' "$MODE" >&2; exit 2 ;;
esac

export SOURCE_DATE_EPOCH="$SOURCE_EPOCH"
export KBUILD_BUILD_TIMESTAMP="@$SOURCE_EPOCH"
export KBUILD_BUILD_USER=plumos
export KBUILD_BUILD_HOST=pixel2-builder
export KBUILD_BUILD_VERSION=1

checkout_source() {
    repo="$1"
    ref="$2"
    dst="$3"
    if [ ! -d "$dst/.git" ]; then
        mkdir -p "${dst%/*}"
        git clone --filter=blob:none --no-checkout "$repo" "$dst"
    fi
    test "$(git -C "$dst" remote get-url origin)" = "$repo" || {
        printf 'error: unexpected source origin in %s\n' "$dst" >&2
        exit 1
    }
    git -C "$dst" fetch --depth=1 origin "$ref"
    git -C "$dst" checkout --detach --force "$ref"
    git -C "$dst" clean -q -fdx
    test "$(git -C "$dst" rev-parse HEAD)" = "$ref"
}

checkout_source "$KERNEL_REPO" "$KERNEL_REF" "$KERNEL_SRC"
checkout_source "$DRIVER_REPO" "$DRIVER_REF" "$DRIVER_SRC"

# An empty .scmversion suppresses the dirty/cache suffix. The resulting
# release must be byte-for-byte identical to the stock module ABI string.
: >"$KERNEL_SRC/.scmversion"
make -C "$KERNEL_SRC" CC=gcc HOSTCC=gcc ARCH=arm64 px30_linux_defconfig
"$KERNEL_SRC/scripts/config" --file "$KERNEL_SRC/.config" \
    --set-str LOCALVERSION "" --disable LOCALVERSION_AUTO \
    --module R8188EU --module LIB80211 --module LIB80211_CRYPT_WEP \
    --module LIB80211_CRYPT_CCMP
make -C "$KERNEL_SRC" CC=gcc HOSTCC=gcc ARCH=arm64 olddefconfig
make -C "$KERNEL_SRC" -j"$JOBS" CC=gcc HOSTCC=gcc ARCH=arm64 modules_prepare
test "$(make -s -C "$KERNEL_SRC" CC=gcc HOSTCC=gcc ARCH=arm64 kernelrelease)" = \
    "$KERNEL_RELEASE"

# KABI provenance gate: rebuild the stock in-tree driver first. Its source
# fingerprint and vermagic must match the captured Pixel2 stock binary before
# the out-of-tree 8821CU module is accepted.
make -C "$KERNEL_SRC" -j"$JOBS" CC=gcc HOSTCC=gcc ARCH=arm64 \
    KBUILD_MODPOST_WARN=1 M=drivers/staging/rtl8188eu modules >/dev/null
REBUILT_STOCK="$KERNEL_SRC/drivers/staging/rtl8188eu/r8188eu.ko"
stock_srcversion="$(modinfo -F srcversion "$STOCK_MODULE")"
rebuilt_srcversion="$(modinfo -F srcversion "$REBUILT_STOCK")"
stock_vermagic="$(modinfo -F vermagic "$STOCK_MODULE")"
rebuilt_vermagic="$(modinfo -F vermagic "$REBUILT_STOCK")"
[ "$stock_srcversion" = "$EXPECTED_STOCK_SRCVERSION" ] && \
    [ "$rebuilt_srcversion" = "$stock_srcversion" ] || {
    printf 'error: Pixel2 stock kernel source fingerprint mismatch\n' >&2
    exit 1
}
[ "$stock_vermagic" = "$KERNEL_RELEASE SMP mod_unload aarch64" ] && \
    [ "$rebuilt_vermagic" = "$stock_vermagic" ] || {
    printf 'error: Pixel2 stock kernel vermagic mismatch\n' >&2
    exit 1
}

build_log="$CACHE/rtl8821cu-pixel2-build.log"
make -C "$DRIVER_SRC" -j"$JOBS" ARCH=arm64 KSRC="$KERNEL_SRC" \
    CC=gcc HOSTCC=gcc KBUILD_MODPOST_WARN=1 >"$build_log" 2>&1
DRIVER_MODULE="$DRIVER_SRC/8821cu.ko"
test -f "$DRIVER_MODULE"
[ "$(modinfo -F name "$DRIVER_MODULE")" = 8821cu ]
[ "$(modinfo -F version "$DRIVER_MODULE")" = "$EXPECTED_DRIVER_VERSION" ]
[ "$(modinfo -F vermagic "$DRIVER_MODULE")" = "$stock_vermagic" ]
modinfo -F alias "$DRIVER_MODULE" | grep -Fq 'usb:v0BDApC811'
modinfo -F alias "$DRIVER_MODULE" | grep -Fq 'usb:v0BDApC820'
if readelf -S "$DRIVER_MODULE" | grep -q __versions; then
    printf 'error: unexpected CONFIG_MODVERSIONS data in 8821cu.ko\n' >&2
    exit 1
fi

# The published kernel tree has a partial Module.symvers. Keep the exact two
# known host-side warnings visible; any new unresolved export is a build stop.
warnings="$(sed -n 's/^WARNING: modpost: "\([^"]*\)".*undefined!$/\1/p' "$build_log" | LC_ALL=C sort -u)"
expected_warnings="$(printf '%s\n' __raw_spin_lock_init rcu_read_unlock_strict | LC_ALL=C sort)"
[ "$warnings" = "$expected_warnings" ] || {
    printf 'error: unexpected 8821cu modpost warning set:\n%s\n' "$warnings" >&2
    exit 1
}

rm -rf "$OUT"
mkdir -p "$OUT/$KERNEL_RELEASE/extra" "$OUT/licenses/rtl8821cu"
install -m 0644 "$DRIVER_MODULE" "$OUT/$KERNEL_RELEASE/extra/8821cu.ko"
strip --strip-debug "$OUT/$KERNEL_RELEASE/extra/8821cu.ko"
install -m 0644 "$DRIVER_SRC/LICENSE" "$OUT/licenses/rtl8821cu/LICENSE"
nm -u "$OUT/$KERNEL_RELEASE/extra/8821cu.ko" | awk '{print $2}' | \
    LC_ALL=C sort -u >"$OUT/rtl8821cu.required-kernel-symbols"
module_sha="$(sha256sum "$OUT/$KERNEL_RELEASE/extra/8821cu.ko" | awk '{print $1}')"
cat >"$OUT/rtl8821cu.json" <<EOF
{
  "name": "rtl8821cu",
  "module": "8821cu",
  "kernel_release": "$KERNEL_RELEASE",
  "kernel_source": "$KERNEL_REPO",
  "kernel_ref": "$KERNEL_REF",
  "stock_kabi_probe": "r8188eu",
  "stock_kabi_srcversion": "$stock_srcversion",
  "driver_source": "$DRIVER_REPO",
  "driver_ref": "$DRIVER_REF",
  "driver_version": "$EXPECTED_DRIVER_VERSION",
  "usb_ids": ["0bda:c811", "0bda:c820"],
  "power_saving": true,
  "usb_autosuspend": false,
  "module_sha256": "$module_sha"
}
EOF
(
    cd "$OUT"
    find "$KERNEL_RELEASE" licenses rtl8821cu.json \
        rtl8821cu.required-kernel-symbols -type f -print0 | \
        LC_ALL=C sort -z | xargs -0 sha256sum
) >"$OUT/checksums.sha256"
(cd "$OUT" && sha256sum -c checksums.sha256 >/dev/null)
verify_output

printf 'rtl8821cu=result-ok kernel=%s driver_ref=%s module_sha256=%s\n' \
    "$KERNEL_RELEASE" "$DRIVER_REF" "$module_sha"
