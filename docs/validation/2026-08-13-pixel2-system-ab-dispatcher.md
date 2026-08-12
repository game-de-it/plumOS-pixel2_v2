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

The dispatcher exposes `/usr/lib/systemd/systemd -> /init` because that is the
fixed executable selected by the retained stock initramfs. It also exposes
`/sbin/init -> /init` for direct recovery invocation. A first live deployment
without those compatibility entrypoints stopped before ADB enumeration; the
omission was reproduced by inspecting the proven stock handoff shim and added
to the dispatcher verification gate.

The next live attempt reached the stock IUX framebuffer logo but did not start
ADB. The dispatcher had incorrectly called BusyBox `switch_root` a second time.
At that point the current root is already the block-backed dispatcher SquashFS,
not initramfs/rootfs, so the second handoff must use `pivot_root`. The corrected
contract pivots into a verified slot through its pre-created
`/.plumos-dispatcher-old` mountpoint. The slot `/sbin/init` immediately detaches
that old read-only mount before starting normal services.

`tests/test-pivot-root-handoff.sh` reproduces this with a privileged isolated
mount namespace: it pivots from the container root into a fresh mounted root,
executes the new root's init, and requires that init to unmount the old root.

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
- renderer-ready promotion refuses a booted/pending mismatch and atomically
  promotes a matching slot while removing its attempt state: pass.

The rebuilt image was produced from source ref `f5a3bfc` while this worktree was
dirty, so it is a development validation artifact, not a release artifact.
Cold boot, renderer-ready promotion, and forced rollback on a physical Pixel2
remain explicit hardware gates.

## First-device recovery

The first dispatcher deployment stopped before ADB because the stock handoff
entrypoint was absent. With the SD attached to macOS as `/dev/disk4`, the boot
volume contained the expected failed dispatcher, intact A/B slots, and the
original `SYSTEM.pre-ab-a87d7b3f` recovery image. Only `/SYSTEM` was replaced
with the corrected dispatcher from commit `b19ff96`.

Readback results before eject:

```text
dispatcher cb545b5c7e8c40a65a6a0c6de2dc78871511586a9e3e8d40655788d4efae9103
slot-a     d1634c22f874da943b4ae361e3c1e32b25089f769aadac163539a688e7638a99
slot-b     d1634c22f874da943b4ae361e3c1e32b25089f769aadac163539a688e7638a99
backup     a87d7b3ff1831c67f53debad9f9eafd3f77eef83f2fa2a58ba43f5f366a804a4
```

The complete `/dev/disk4` device was ejected successfully. No slot, Runtime,
user content, or original recovery image was replaced during this recovery.
