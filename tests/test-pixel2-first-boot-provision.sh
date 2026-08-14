#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SEED_IMAGE="${1:-}"
PROVISIONER="$ROOT_DIR/rootfs/pixel2/usr/sbin/plumos-first-boot-provision"

for tool in sfdisk truncate mkfs.ext4 resize2fs mkfs.fat blkid mmd mdir mcopy sha256sum; do
    command -v "$tool" >/dev/null 2>&1 || {
        printf 'error: required first-boot test tool missing: %s\n' "$tool" >&2
        exit 1
    }
done
bash -n "$PROVISIONER"

tmp="$(mktemp -d /tmp/plumos-pixel2-provision.XXXXXX)"
trap 'rm -rf "$tmp"' EXIT
card="$tmp/card.img"
p2="$tmp/p2.ext4"
p3="$tmp/p3.fat"
state="$tmp/runtime"
sysfs="$tmp/sys/class/block"
disk_bytes=16000000000
disk_sectors=$((disk_bytes / 512))
user_start=17858560
user_sectors=$((disk_sectors - user_start))

if [ -n "$SEED_IMAGE" ]; then
    [ -f "$SEED_IMAGE" ] || { printf 'error: seed image missing: %s\n' "$SEED_IMAGE" >&2; exit 2; }
    dd if="$SEED_IMAGE" of="$card" bs=512 count=1 status=none
else
    truncate -s 2701131776 "$card"
    sfdisk "$card" >/dev/null <<EOF
label: dos
label-id: 0x4d554c50
unit: sectors

${card}1 : start=32768, size=1048576, type=c, bootable
${card}2 : start=1081344, size=4194304, type=83
EOF
fi
truncate -s "$disk_bytes" "$card"
boot_code_before=$(dd if="$card" bs=1 count=440 status=none | sha256sum | awk '{print $1}')

truncate -s $((4194304 * 512)) "$p2"
mkfs.ext4 -q -F -L PLUMOS_SYS "$p2"
truncate -s $((user_sectors * 512)) "$p3"
mkdir -p "$state" "$sysfs/${p2##*/}" "$sysfs/${p3##*/}"
printf '%s\n' 1081344 >"$sysfs/${p2##*/}/start"
printf '%s\n' 4194304 >"$sysfs/${p2##*/}/size"

run_provisioner() {
    set +e
    PLUMOS_PROVISION_HOST_MODE=1 \
    PLUMOS_PROVISION_DISK="$card" \
    PLUMOS_PROVISION_P2="$p2" \
    PLUMOS_PROVISION_P3="$p3" \
    PLUMOS_PROVISION_RUNTIME_ROOT="$state" \
    PLUMOS_PROVISION_SYS_CLASS_BLOCK="$sysfs" \
    PLUMOS_PROVISION_BUSYBOX=/bin/busybox \
    PLUMOS_PROVISION_SFDISK="$(command -v sfdisk)" \
    PLUMOS_PROVISION_RESIZE2FS="$(command -v resize2fs)" \
    PLUMOS_PROVISION_MKFS_FAT="$(command -v mkfs.fat)" \
    PLUMOS_PROVISION_BLKID="${PROVISION_BLKID:-$(command -v blkid)}" \
    PLUMOS_PROVISION_MMD="$(command -v mmd)" \
    PLUMOS_PROVISION_MCOPY="$(command -v mcopy)" \
        "$PROVISIONER"
    provision_rc=$?
    set -e
}

# Phase 1 writes the final MBR and stops at the same reboot boundary used when
# a mounted kernel partition cannot be refreshed in place.
run_provisioner
[ "$provision_rc" -eq 20 ]
dump=$(sfdisk -d "$card")
printf '%s\n' "$dump" | grep -Eq "^${card}2 .*start=[[:space:]]*1081344, size=[[:space:]]*16777216,"
printf '%s\n' "$dump" | grep -Eq "^${card}3 .*start=[[:space:]]*17858560, size=[[:space:]]*${user_sectors},"
test -f "$state/provision/p3-owned-by-provisioner"
test -f "$state/provision/partition-table-written"
[ "$boot_code_before" = "$(dd if="$card" bs=1 count=440 status=none | sha256sum | awk '{print $1}')" ]

# Simulate the post-reboot kernel view, then resume filesystem provisioning.
truncate -s $((16777216 * 512)) "$p2"
printf '%s\n' 16777216 >"$sysfs/${p2##*/}/size"
printf '%s\n' 17858560 >"$sysfs/${p3##*/}/start"
printf '%s\n' "$user_sectors" >"$sysfs/${p3##*/}/size"
run_provisioner
[ "$provision_rc" -eq 0 ]
test -f "$state/provision/complete"
[ "$(blkid -s LABEL -o value "$p2")" = PLUMOS_SYS ]
[ "$(blkid -s LABEL -o value "$p3")" = PLUMOS_USER ]
[ "$(blkid -s TYPE -o value "$p3")" = vfat ]
for directory in roms bios Images Themes Screenshots Music updates imports exports plumos-logs; do
    MTOOLS_SKIP_CHECK=1 mdir -i "$p3" "::/$directory" >/dev/null
done
MTOOLS_SKIP_CHECK=1 mdir -i "$p3" ::/.plumos-ready >/dev/null

# A completed ordinary boot must be read-only with respect to both filesystems
# and provisioning state.
sample_hash() {
    dd if="$1" bs=1M count=32 status=none | sha256sum | awk '{print $1}'
}
before_p2=$(sample_hash "$p2")
before_p3=$(sample_hash "$p3")
before_state=$(find "$state" -type f -exec sha256sum {} + | sort | sha256sum | awk '{print $1}')
run_provisioner
[ "$provision_rc" -eq 0 ]
[ "$before_p2" = "$(sample_hash "$p2")" ]
[ "$before_p3" = "$(sample_hash "$p3")" ]
[ "$before_state" = "$(find "$state" -type f -exec sha256sum {} + | sort | sha256sum | awk '{print $1}')" ]

# A missing blkid was previously misread as an unformatted provisioner-owned
# p3 and caused mkfs.fat to run on every boot. The boot-sector fallback must
# recognize the completed FAT32 volume without changing either filesystem or
# provisioning state.
before_p2=$(sample_hash "$p2")
before_p3=$(sample_hash "$p3")
before_state=$(find "$state" -type f -exec sha256sum {} + | sort | sha256sum | awk '{print $1}')
PROVISION_BLKID=/nonexistent/plumos-blkid run_provisioner
[ "$provision_rc" -eq 0 ]
[ "$before_p2" = "$(sample_hash "$p2")" ]
[ "$before_p3" = "$(sample_hash "$p3")" ]
[ "$before_state" = "$(find "$state" -type f -exec sha256sum {} + | sort | sha256sum | awk '{print $1}')" ]

# The former 4 GiB development layout contains p3 before the 8 GiB target.
# It must be preserved byte-for-byte instead of moved or formatted.
legacy="$tmp/legacy.img"
legacy_p2="$tmp/legacy-p2"
legacy_p3="$tmp/legacy-p3.fat"
legacy_state="$tmp/legacy-runtime"
legacy_sysfs="$tmp/legacy-sys/class/block"
truncate -s "$disk_bytes" "$legacy"
sfdisk "$legacy" >/dev/null <<EOF
label: dos
label-id: 0x4d554c50
unit: sectors

${legacy}1 : start=32768, size=1048576, type=c, bootable
${legacy}2 : start=1081344, size=4194304, type=83
${legacy}3 : start=5275648, size=3112960, type=c
EOF
truncate -s 67108864 "$legacy_p3"
mkfs.fat -F 32 -n PLUMOS_USER "$legacy_p3" >/dev/null
printf 'preserve-me\n' >"$tmp/preserve.txt"
MTOOLS_SKIP_CHECK=1 mcopy -i "$legacy_p3" "$tmp/preserve.txt" ::/preserve.txt
legacy_before=$(sha256sum "$legacy_p3" | awk '{print $1}')
legacy_table_before=$(sfdisk -d "$legacy" | sha256sum | awk '{print $1}')
mkdir -p "$legacy_state" "$legacy_sysfs/${legacy_p2##*/}" "$legacy_sysfs/${legacy_p3##*/}"
printf '%s\n' 4194304 >"$legacy_sysfs/${legacy_p2##*/}/size"
set +e
PLUMOS_PROVISION_HOST_MODE=1 \
PLUMOS_PROVISION_DISK="$legacy" PLUMOS_PROVISION_P2="$legacy_p2" \
PLUMOS_PROVISION_P3="$legacy_p3" PLUMOS_PROVISION_RUNTIME_ROOT="$legacy_state" \
PLUMOS_PROVISION_SYS_CLASS_BLOCK="$legacy_sysfs" \
PLUMOS_PROVISION_BUSYBOX=/bin/busybox "$PROVISIONER"
legacy_rc=$?
set -e
[ "$legacy_rc" -eq 30 ]
[ "$legacy_before" = "$(sha256sum "$legacy_p3" | awk '{print $1}')" ]
[ "$legacy_table_before" = "$(sfdisk -d "$legacy" | sha256sum | awk '{print $1}')" ]
MTOOLS_SKIP_CHECK=1 mdir -i "$legacy_p3" ::/preserve.txt >/dev/null

printf 'pixel2_first_boot_provision=result-ok disk_sectors=%s user_sectors=%s\n' \
    "$disk_sectors" "$user_sectors"
