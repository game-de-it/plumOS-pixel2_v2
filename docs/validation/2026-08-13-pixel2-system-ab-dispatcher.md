# Pixel2 System A/B dispatcher validation

Date: 2026-08-13

## Scope

The stock Pixel2 `Image`, DTB, and embedded initramfs remain unchanged. The
stock initramfs still opens `/flash/SYSTEM`; that file is now a small static
BusyBox dispatcher owned by plumOS. It verifies and mounts one of:

- `/flash/system-slots/system-a.squashfs`
- `/flash/system-slots/system-b.squashfs`

The slot directory deliberately is not named `/System`: FAT32 is
case-insensitive, so `/SYSTEM` and `/System/` cannot coexist.

## Boot state contract

State is stored on the ext4 Runtime partition below `/update-state`:

- `system-active`: last renderer-ready generation;
- `system-pending`: inactive slot requested for the next boot;
- `system-pending-attempted`: proves that the pending slot already received
  its single boot attempt;
- `system-booted`: slot selected by the dispatcher.

The dispatcher hashes the complete slot image against its boot-volume SHA-256
file before mounting it. A pending slot gets one attempt. If the following boot
still finds the attempted marker, it removes pending state and selects the
previous active slot. An invalid selected image falls back to another verified
slot; if neither verifies, the dispatcher stops in a local recovery shell.

## Host validation

Commands:

```text
./tests/test-system-rootfs-scripts.sh
./tests/test-system-dispatcher-state.sh
./scripts/build-system-rootfs.sh
./scripts/build-sd-image.sh
./scripts/verify-sd-image.sh output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img artifacts/vendor/pixel2-stock-source/rockchip-boot-prefix.bin
```

Results:

- dispatcher syntax/static identity gate: pass;
- active, first pending attempt, second-boot rollback, invalid pending fallback,
  and invalid active fallback state tests: pass;
- AArch64 dispatcher and both complete System slot SquashFS builds: pass;
- rebuilt 4 GiB image checksum: pass;
- image extraction, FAT/ext4 checks, dispatcher verification, both slot
  rootfs/kernel ABI verifications, and app-layer verification: pass.

The rebuilt image was produced from source ref `f5a3bfc` while this worktree was
dirty, so it is a development validation artifact, not a release artifact.
Cold boot, renderer-ready promotion, and forced rollback on a physical Pixel2
remain explicit hardware gates.
