#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"

if [ "${1:-}" != --inside ]; then
    docker image inspect "$IMAGE" >/dev/null 2>&1 || "$ROOT_DIR/scripts/build-tools-image.sh"
    exec docker run --rm --platform linux/arm64 \
        -e SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-}" \
        -v "$ROOT_DIR:/work" -w /work "$IMAGE" ./scripts/build-kernel.sh --inside
fi

ROOT_DIR=/work
KERNEL_VERSION=6.12.79
KERNEL_SHA256=4bfa751f33de2a5d7ecb4ff964743a027fc726a2225a76a18f92f0582aa0790b
KERNEL_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$KERNEL_VERSION.tar.xz"
SUPPORT_URL=https://github.com/ROCKNIX/distribution.git
SUPPORT_COMMIT=a4d24cf9d81bc05840773b5027751edb11c45c8b
CACHE="$ROOT_DIR/.cache/kernel"
SUPPORT="$CACHE/hardware-support"
TARBALL="$CACHE/linux-$KERNEL_VERSION.tar.xz"
BUILD=/tmp/plumos-pixel2-kernel
SRC="$BUILD/linux-$KERNEL_VERSION"
OUT="$ROOT_DIR/output/kernel/pixel2"
SOURCE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[ -n "$SOURCE_EPOCH" ] || SOURCE_EPOCH="$(git -C "$ROOT_DIR" show -s --format=%ct HEAD)"
export SOURCE_DATE_EPOCH="$SOURCE_EPOCH"
export KBUILD_BUILD_TIMESTAMP="@$SOURCE_EPOCH"
export KBUILD_BUILD_USER=plumos
export KBUILD_BUILD_HOST=pixel2-builder
export KBUILD_BUILD_VERSION=1

mkdir -p "$CACHE"
if [ ! -s "$TARBALL" ]; then
    curl -fsSL "$KERNEL_URL" -o "$TARBALL"
fi
printf '%s  %s\n' "$KERNEL_SHA256" "$TARBALL" | sha256sum -c - >/dev/null

if [ ! -d "$SUPPORT/.git" ]; then
    git clone --filter=blob:none --no-checkout "$SUPPORT_URL" "$SUPPORT"
fi
git -C "$SUPPORT" fetch --depth=1 origin "$SUPPORT_COMMIT"
git -C "$SUPPORT" sparse-checkout init --cone
git -C "$SUPPORT" sparse-checkout set \
    projects/ROCKNIX/devices/RK3326 \
    projects/ROCKNIX/packages/linux-drivers/generic-dsi
git -C "$SUPPORT" checkout --detach "$SUPPORT_COMMIT"
test "$(git -C "$SUPPORT" rev-parse HEAD)" = "$SUPPORT_COMMIT"

rm -rf "$BUILD" "$OUT"
mkdir -p "$BUILD" "$OUT/modules" "$OUT/licenses"
tar -C "$BUILD" -xf "$TARBALL"
PATCH_DIR="$SUPPORT/projects/ROCKNIX/devices/RK3326/patches/linux"
for name in 000-rk3326-dts.patch 001-panel-updates.patch \
    022-usb-role-switch.patch 023-px30s-tsadc-support.patch \
    024-px30s-gpu-power-domain-workaround.patch \
    025-px30s-dsi-dphy-timing-table.patch \
    026-px30s-cpuinfo-soc-detection.patch \
    027-px30s-io-domain-pmuio1.patch 028-px30s-usb2phy-tuning.patch \
    030-pwm-vibra-kick-stuck-boot-state.patch \
    031-px30s-suspend-wakeup-config.patch 032-px30s-otp-read-protocol.patch \
    033-px30s-ddrphy-clock-topology.patch \
    034-px30s-cru-suspend-resume-restore.patch; do
    patch -d "$SRC" -p1 --forward <"$PATCH_DIR/$name"
done

DTS_SRC="$SUPPORT/projects/ROCKNIX/devices/RK3326/linux/dts/rockchip/rk3326s-gkd-pixel2.dts"
DTS_DST="$SRC/arch/arm64/boot/dts/rockchip/rk3326s-gkd-pixel2.dts"
cp "$DTS_SRC" "$DTS_DST"
python3 - "$DTS_DST" <<'PY'
from pathlib import Path
import re, sys
p = Path(sys.argv[1])
s = p.read_text()
s = s.replace('compatible = "rocknix-singleadc-joypad";', 'compatible = "gpio-keys";')
s = s.replace('rocknix,generic-dsi', 'plumos,generic-dsi')
s = s.replace('ROCKNIX', 'plumOS').replace('rocknix', 'plumos')

# Live evdev capture is authoritative for the Pixel2 switch wiring.  The stock
# DTB labels PD1 as A and PD2 as B, but the labeled physical A switch actually
# drives PD2 and the labeled physical B switch drives PD1.  Normalize both the
# GPIO and Linux positional code so every userspace sees an unambiguous pad:
# physical A/right is BTN_EAST; physical B/bottom is BTN_SOUTH.
def set_button_contract(node, expected_gpio, expected_code, corrected_gpio,
                        corrected_label, corrected_code):
    global s
    pattern = (r'(\n\s*' + re.escape(node) + r'\s*\{\s*\n)'
               r'(\s*)gpios = <&gpio3 ' + re.escape(expected_gpio) +
               r' GPIO_ACTIVE_LOW>;\s*\n'
               r'\s*label = "[^"]+";\s*\n'
               r'\s*linux,code = <' + re.escape(expected_code) + r'>;'
               r'(\s*\n\s*\};)')
    replacement = (r'\1\2gpios = <&gpio3 ' + corrected_gpio +
                   r' GPIO_ACTIVE_LOW>;\n\2label = "' + corrected_label +
                   r'";\n\2linux,code = <' + corrected_code + r'>;\3')
    s, count = re.subn(pattern, replacement, s, count=1)
    if count != 1:
        raise SystemExit(f'expected exactly one source contract for {node}')

set_button_contract('button-a', 'RK_PD1', 'BTN_SOUTH',
                    'RK_PD2', 'A', 'BTN_EAST')
set_button_contract('button-b', 'RK_PD2', 'BTN_EAST',
                    'RK_PD1', 'B', 'BTN_SOUTH')
for key in ('joypad-name', 'joypad-vendor', 'joypad-product', 'joypad-revision',
            'amux-count', 'poll-interval'):
    s = re.sub(r'^\s*' + re.escape(key) + r'\s*=.*?;\n', '', s, flags=re.M)
p.write_text(s)
PY
printf '\ndtb-$(CONFIG_ARCH_ROCKCHIP) += rk3326s-gkd-pixel2.dtb\n' \
    >>"$SRC/arch/arm64/boot/dts/rockchip/Makefile"

PANEL_SRC="$SUPPORT/projects/ROCKNIX/packages/linux-drivers/generic-dsi/sources/panel-generic-dsi.c"
PANEL_DST="$SRC/drivers/gpu/drm/panel/panel-plumos-generic-dsi.c"
sed 's/rocknix,generic-dsi/plumos,generic-dsi/g' "$PANEL_SRC" >"$PANEL_DST"
printf '\nobj-y += panel-plumos-generic-dsi.o\n' >>"$SRC/drivers/gpu/drm/panel/Makefile"

INITRAMFS="$BUILD/initramfs"
mkdir -p "$INITRAMFS/bin" "$INITRAMFS/sbin" "$INITRAMFS/dev" \
    "$INITRAMFS/proc" "$INITRAMFS/sys" "$INITRAMFS/run" \
    "$INITRAMFS/boot" "$INITRAMFS/newroot"
install -m 0755 /bin/busybox "$INITRAMFS/bin/busybox"
install -m 0755 "$ROOT_DIR/initramfs/pixel2/init" "$INITRAMFS/init"
ln -s /bin/busybox "$INITRAMFS/sbin/mdev"
mknod -m 0600 "$INITRAMFS/dev/console" c 5 1
mknod -m 0666 "$INITRAMFS/dev/null" c 1 3

cp "$SUPPORT/projects/ROCKNIX/devices/RK3326/linux/linux.aarch64.conf" "$SRC/.config"
config_set() {
    (cd "$SRC" && scripts/config "$@")
}
config_set --set-str CONFIG_LOCALVERSION -plumos-pixel2
config_set --disable CONFIG_LOCALVERSION_AUTO
config_set --set-str CONFIG_DEFAULT_HOSTNAME plumos-pixel2
config_set --set-str CONFIG_INITRAMFS_SOURCE "$INITRAMFS"
config_set --disable CONFIG_INITRAMFS_PRESERVE_MTIME
config_set --disable CONFIG_INITRAMFS_COMPRESSION_LZO
config_set --enable CONFIG_INITRAMFS_COMPRESSION_NONE
config_set --set-str CONFIG_SYSTEM_TRUSTED_KEYS ''
config_set --set-str CONFIG_SYSTEM_REVOCATION_KEYS ''
for option in CONFIG_BLK_DEV_LOOP CONFIG_DEVTMPFS CONFIG_DEVTMPFS_MOUNT \
    CONFIG_SQUASHFS CONFIG_SQUASHFS_XZ CONFIG_VFAT_FS CONFIG_EXT4_FS \
    CONFIG_KEYBOARD_GPIO CONFIG_PWM_VIBRA CONFIG_USB_DWC2 \
    CONFIG_USB_DWC2_DUAL_ROLE CONFIG_USB_CONFIGFS CONFIG_USB_CONFIGFS_F_FS \
    CONFIG_USB_F_FS; do
    config_set --enable "$option"
done
make -C "$SRC" ARCH=arm64 olddefconfig
BUILD_LOG="$ROOT_DIR/output/kernel/pixel2-build.log"
mkdir -p "${BUILD_LOG%/*}"
if ! make -C "$SRC" -j"$(nproc)" --output-sync=target ARCH=arm64 Image \
    rockchip/rk3326s-gkd-pixel2.dtb modules >"$BUILD_LOG" 2>&1; then
    tail -n 200 "$BUILD_LOG" >&2
    exit 1
fi
make -C "$SRC" ARCH=arm64 INSTALL_MOD_PATH="$OUT/modules" modules_install
rm -f "$OUT/modules/lib/modules/"*/build "$OUT/modules/lib/modules/"*/source

install -m 0644 "$SRC/arch/arm64/boot/Image" "$OUT/Image"
install -m 0644 "$SRC/arch/arm64/boot/dts/rockchip/rk3326s-gkd-pixel2.dtb" \
    "$OUT/rk3326s-gkd-pixel2.dtb"
install -m 0644 "$SRC/.config" "$OUT/kernel.config"
cp "$SRC/COPYING" "$OUT/licenses/linux-COPYING"
cat >"$OUT/source.manifest" <<EOF
kernel_url=$KERNEL_URL
kernel_version=$KERNEL_VERSION
kernel_sha256=$KERNEL_SHA256
hardware_support_url=$SUPPORT_URL
hardware_support_commit=$SUPPORT_COMMIT
hardware_support_scope=Pixel2 DTS, PX30S patches, generic DSI driver
functional_branding=plumOS
gamepad_driver=gpio-keys
initramfs=plumOS Pixel2
source_date_epoch=$SOURCE_EPOCH
EOF
(cd "$OUT" && sha256sum Image rk3326s-gkd-pixel2.dtb kernel.config \
    source.manifest >checksums.sha256)
if strings "$OUT/Image" "$OUT/rk3326s-gkd-pixel2.dtb" | \
    grep -Eiq '(rocknix|emuelec|batocera|knulli)'; then
    printf 'error: foreign distribution identity in kernel boot artifacts\n' >&2
    exit 1
fi
printf 'kernel=result-ok output=%s\n' "$OUT"
