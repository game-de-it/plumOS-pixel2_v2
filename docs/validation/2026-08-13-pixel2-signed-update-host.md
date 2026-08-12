# Pixel2 signed update host validation

Date: 2026-08-13
Scope: signed Runtime/System updater, package builder, FE request flow, System rootfs

## Reference boundary

The transaction engine and package format are ported from the mature shared
plumOS implementation. Pixel2-specific changes are limited to the
device/vendor IDs, named System/Runtime ABIs, managed app-layer paths,
`/flash` boot mount, and FAT32-safe `/system-slots` directory. Pixel2
additionally refuses System health promotion unless the dispatcher-recorded
booted slot equals the pending slot.

No other handheld or foreign distribution product identity is stored in the
Pixel2 update package names, UI strings, System overlay, or generated rootfs.

## Security and transaction checks

`tests/test-pixel2-update.sh` generates an ephemeral Ed25519 key pair and
verifies all of the following without using the repository-local release key:

- signed Runtime request and application;
- managed file replacement while preserving user configuration;
- metadata-last commit and frontend-ready health promotion;
- one bounded rollback generation;
- automatic rollback when readiness was not confirmed before the next boot;
- recovery of an interrupted write-ahead journal;
- rejection of unsigned packages without the explicit development override;
- inactive System slot write and complete readback equality;
- reboot return code after System staging;
- refusal to promote a pending System slot that was not booted;
- promotion only after the matching booted slot is recorded;
- narrow fallback for the stock VFAT driver's errno-zero `fsync` behavior.

Result:

```text
pixel2_update=result-ok
system_rootfs_scripts=result-ok
```

## ARM64 System checks

`./scripts/docker-build.sh frontend` rebuilt the FE with the real System Update
request/safe-reboot path. `./scripts/docker-build.sh system-rootfs` then built
both A/B SquashFS images and verified in an AArch64 chroot that Python 3.11,
OpenSSL, the updater entry point, and required Python modules execute. The
rootfs verifier also requires the Pixel2 public key and rejects private signing
keys or foreign distribution identity.

The generated test System images were identical between A and B. A clean
source-ref rebuild and physical Pixel2 signed Runtime/System acceptance remain
separate gates; host success is not recorded as device validation.
