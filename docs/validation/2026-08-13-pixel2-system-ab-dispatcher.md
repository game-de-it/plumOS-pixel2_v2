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

The corrected dispatcher and both slot images were rebuilt from clean commit
`933ca32` and installed through macOS. Payloads were copied to temporary FAT32
names, read back, then committed in payload, metadata, dispatcher order.

```text
dispatcher 703274f6433bc5da2d5587d873a715f2ed4efee0daa69d02a26fb5e261ce5564
slot-a     8d4f6eb9cafefc4a8f2011fab278df0ade04cfd8f32b38f9d14a19e0c8e4435b
slot-b     8d4f6eb9cafefc4a8f2011fab278df0ade04cfd8f32b38f9d14a19e0c8e4435b
backup     a87d7b3ff1831c67f53debad9f9eafd3f77eef83f2fa2a58ba43f5f366a804a4
```

Both slot manifests report `source_ref=933ca32`; their external SHA-256 files
match the complete slot images. `PLUMOS_BOOT` and `PLUMOS_USER` were unmounted
and `/dev/disk4` was ejected successfully. Physical boot remains the next gate.

The `933ca32` image again remained at the IUX logo. Static review found that
the dispatcher moved `/proc` before consulting `/proc/mounts` for `/sys`,
`/flash`, and `/storage`; those later mount checks therefore lost their source
of truth. The corrected order is `dev, sys, flash, storage, proc`. The
dispatcher now synchronously records every mount, slot, and pivot stage to
`/storage/plumos/logs/system-dispatcher.log`, allowing the precise stop point
to be recovered from ext4 even when ADB never starts.

The mount-order and persistent-diagnostics fix was rebuilt from clean commit
`d84d2af` and installed to both slots using staged FAT32 names and readback
verification:

```text
dispatcher 837e5183158de3900861b18f5c71a3f28afe93647e9daf8d15553ecb209c3e4c
slot-a     4b64e0b16a297d340641cb108fffd61267c77d46e76b5df5e5e1ac631d0bb262
slot-b     4b64e0b16a297d340641cb108fffd61267c77d46e76b5df5e5e1ac631d0bb262
```

Both slot manifests and the dispatcher manifest report `source_ref=d84d2af`.
The old recovery System remains intact, and both mounted volumes were cleanly
unmounted before `/dev/disk4` was ejected.

The `d84d2af` build also stopped at IUX. A read-only 2 GiB capture of p2 was
taken after unmounting the SD on macOS. `e2fsck -fn` found no structural errors;
a clone was separately journal-replayed with `e2fsck -fy`. Neither view
contained `system-dispatcher.log` or `/update-state`, proving that the
dispatcher had never executed. The fixed dispatcher SquashFS lacked `/dev`,
`/proc`, `/sys`, `/flash`, and `/storage`, which the stock initramfs must move
into its selected root before the first `switch_root`. The build now creates
and verifies all of those mountpoints plus `/newroot` and `/dev/pts`.

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
