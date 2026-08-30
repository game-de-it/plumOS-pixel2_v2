#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CLEANUP="$ROOT_DIR/package/portmaster-pixel2/plumos/bin/plumos-portmaster-mount-cleanup"
work="$(mktemp -d /tmp/plumos-portmaster-mount-test.XXXXXX)"
trap 'rm -rf "$work"' EXIT

plumos_root="$work/plumos"
run_root="$work/run"
pm_dir="$plumos_root/state/portmaster/data/upstream/PortMaster"
mountinfo="$work/mountinfo"
track="$run_root/port.mounts"
mkdir -p "$run_root" "$pm_dir/config"

write_mountinfo() {
  cat > "$mountinfo" <<EOF
10 1 0:1 / /usr/lib/compat rw - tmpfs tmpfs rw
11 1 0:2 / $pm_dir/config rw - tmpfs tmpfs rw
EOF
}

cat > "$work/umount" <<'EOF'
#!/bin/sh
lazy=0
if [ "${1:-}" = -l ]; then
  lazy=1
  shift
fi
target=$1
printf 'lazy=%s target=%s\n' "$lazy" "$target" >> "$FAKE_UMOUNT_LOG"
if [ "$target" = "$FAKE_BUSY_TARGET" ] && [ "$lazy" -eq 0 ] && \
   [ ! -f "$FAKE_BUSY_ONCE" ]; then
  : > "$FAKE_BUSY_ONCE"
  exit 1
fi
awk -v target="$target" '$5 != target' "$FAKE_MOUNTINFO" \
  > "$FAKE_MOUNTINFO.tmp"
mv "$FAKE_MOUNTINFO.tmp" "$FAKE_MOUNTINFO"
EOF
chmod 0755 "$work/umount"

if PLUMOS_ROOT="$plumos_root" \
   PLUMOS_PORTMASTER_RUN_ROOT="$run_root" \
   PLUMOS_PORTMASTER_MOUNTINFO="$mountinfo" \
   PLUMOS_PORTMASTER_UMOUNT_BIN="$work/umount" \
     /bin/sh "$CLEANUP" "$work/unmanaged.track" 2>/dev/null; then
  printf 'mount cleanup accepted an unmanaged tracking file\n' >&2
  exit 1
fi

write_mountinfo
printf '%s\n%s\n%s\n' /usr/lib/compat "$pm_dir/config" / > "$track"
FAKE_UMOUNT_LOG="$work/umount.log" \
FAKE_BUSY_TARGET="$pm_dir/config" \
FAKE_BUSY_ONCE="$work/busy-once" \
FAKE_MOUNTINFO="$mountinfo" \
PLUMOS_ROOT="$plumos_root" \
PLUMOS_PORTMASTER_RUN_ROOT="$run_root" \
PLUMOS_PORTMASTER_MOUNTINFO="$mountinfo" \
PLUMOS_PORTMASTER_UMOUNT_BIN="$work/umount" \
PLUMOS_PORTMASTER_SLEEP_BIN=true \
  /bin/sh "$CLEANUP" "$track"

[ ! -s "$mountinfo" ]
[ ! -e "$track" ]
grep -q "lazy=0 target=/usr/lib/compat" "$work/umount.log"
[ "$(grep -c "target=$pm_dir/config" "$work/umount.log")" -eq 2 ]
! grep -q 'target=/$' "$work/umount.log"

# A mount that remains busy through bounded normal retries is lazily detached
# instead of becoming a permanent blocker for every later port.
write_mountinfo
rm -f "$work/busy-once" "$work/umount.log"
cat > "$work/umount-lazy" <<'EOF'
#!/bin/sh
lazy=0
if [ "${1:-}" = -l ]; then
  lazy=1
  shift
fi
target=$1
printf 'lazy=%s target=%s\n' "$lazy" "$target" >> "$FAKE_UMOUNT_LOG"
[ "$lazy" -eq 1 ] || exit 1
awk -v target="$target" '$5 != target' "$FAKE_MOUNTINFO" \
  > "$FAKE_MOUNTINFO.tmp"
mv "$FAKE_MOUNTINFO.tmp" "$FAKE_MOUNTINFO"
EOF
chmod 0755 "$work/umount-lazy"
printf '%s\n%s\n' /usr/lib/compat "$pm_dir/config" > "$track"
FAKE_UMOUNT_LOG="$work/umount.log" \
FAKE_BUSY_TARGET=unused \
FAKE_BUSY_ONCE="$work/busy-once" \
FAKE_MOUNTINFO="$mountinfo" \
PLUMOS_ROOT="$plumos_root" \
PLUMOS_PORTMASTER_RUN_ROOT="$run_root" \
PLUMOS_PORTMASTER_MOUNTINFO="$mountinfo" \
PLUMOS_PORTMASTER_UMOUNT_BIN="$work/umount-lazy" \
PLUMOS_PORTMASTER_SLEEP_BIN=true \
PLUMOS_PORTMASTER_UMOUNT_RETRIES=2 \
  /bin/sh "$CLEANUP" "$track"

[ ! -s "$mountinfo" ]
[ ! -e "$track" ]
grep -q 'lazy=1 target=' "$work/umount.log"

printf 'portmaster_pixel2_mount_cleanup=result-ok\n'
