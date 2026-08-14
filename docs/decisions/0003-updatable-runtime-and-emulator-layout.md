# 0003: Pixel2 updateable runtime and emulator layout

Date: 2026-08-12
Status: Adopted; signed updater verified on 2026-08-13; compact seed implemented on 2026-08-14

## Decision

plumOS Pixel2 adopts update ownership and frontend/emulator component models
proven by existing plumOS reference implementations, while retaining the
Pixel2-specific Rockchip boot contract.

The final SD layout is:

```text
raw sectors 0..32767       16 MiB captured Pixel2 Rockchip boot prefix
p1 PLUMOS_BOOT  FAT32     512 MiB fixed boot assets and System A/B slots
p2 PLUMOS_SYS   ext4     2048 MiB release seed; expand to 8192 MiB on first boot
p3 PLUMOS_USER  FAT32    absent from seed; create through the final SD sector
```

The minimum supported physical card is 16 GB. The release image contains p1
and p2 only and is approximately 2.5 GiB. First-boot provisioning expands p2
to exactly 8 GiB and creates p3 from the following aligned sector through the
last usable MBR sector. Provisioning must be idempotent and must never format
an existing p3 containing unknown or user data.

The image builder now emits only p1 and p2 and ends at sector 5275647. The
first-boot provisioner validates the MBR and physical-card capacity, records
ownership before modifying the table, grows p2 online, and creates p3 only
when no p3 exists. If the stock handoff's mounted p2 prevents the kernel from
refreshing geometry, setup syncs and resumes after one early reboot. An
existing p3 with legacy, unknown, or unexpected geometry is preserved and is
never reformatted.

## Ownership boundary

### Rockchip boot prefix and kernel

The captured 16 MiB prefix remains an immutable, checksummed hardware input.
The stock `Image`, the Pixel2 DTB, and the stock initramfs embedded in that
`Image` remain fixed release image inputs during the first update
implementation. Updating them requires a complete SD image until a separately
recoverable boot update contract exists.

### System A/B on PLUMOS_BOOT

p1 is normally mounted read-only at `/flash` (`/boot` is a compatibility bind)
and contains:

```text
/Image
/rk3326s-gkd-pixel2.dtb
/SYSTEM                         fixed Pixel2 A/B dispatcher
/system-slots/system-a.squashfs
/system-slots/system-a.sha256
/system-slots/system-b.squashfs
/system-slots/system-b.sha256
/plumos-image.manifest
```

FAT32 is case-insensitive, so the fixed `/SYSTEM` file cannot coexist with a
`/System` directory. The stock initramfs mounts the fixed dispatcher
`/SYSTEM`; that dispatcher verifies and loop-mounts a selected image below
`/system-slots`, then hands off to the plumOS `/sbin/init` inside it. The active
System is never overwritten. A System update writes and fully reads back only
the inactive slot, commits its manifest last, records a pending boot on p2,
and reboots.
Frontend renderer readiness promotes the pending slot; a second attempt
without readiness rejects it and returns to the previous healthy slot.

The dispatcher uses SHA-256 slot integrity. Runtime/System update archives
additionally require an Ed25519 signature verified by the public key embedded
in System. The private signing key is ignored build-host material and is never
copied into System or an SD image. Unsigned packages are accepted only with an
explicit development environment override and are never a normal FE option.

The FE System Update action scans `/mnt/plumos-user/updates`, verifies the
newest compatible Pixel2 package, records the request on ext4, and performs a
safe reboot. Early plumOS init applies Runtime transactions before services or
writes System only to the inactive FAT32 slot, verifies a complete readback,
commits metadata/pending state last, and reboots again when a System slot was
staged.

### Runtime and Linux state on PLUMOS_SYS

p2 is mounted at `/mnt/plumos` and is the flat Pixel2 app-layer ABI:

```text
/mnt/plumos/
  bin/ lib/ cores/ info/ frontend/ emulator/ standalone/ apps/
  share/ licenses/ components/ factory-defaults/
  config/ state/ saves/ states/ logs/ updates/
  VERSION COMPAT_VENDOR RUNTIME_ABI manifest.json checksums.sha256
```

Managed files include the FE, launchers, RetroArch, PicoArch, cores,
standalone emulators, apps, static catalogs, factory defaults and notices.
Device-owned mutable paths include active configuration, state, saves, logs,
PortMaster downloads, Pyxel environments, credentials and host keys. Normal
builds, updates and live deployments must not overwrite those mutable paths.

Runtime updates are journaled file transactions on p2. They stage and verify
all payloads, reject user-owned paths, move replaced managed files into one
bounded rollback backup, commit root/component manifests and checksums last,
and become healthy only after the FE renderer-ready marker. Interrupted
transactions roll back before FE startup.

The package builder may use either a complete prior app-layer directory or the
installed app-layer `checksums.sha256` as its delta baseline. The checksum
baseline is intended for content changes on a physical device; mode-only or
symlink-only transitions require a complete prior directory baseline.

### Portable content on PLUMOS_USER

p3 is mounted at `/mnt/plumos-user` and is the only normal macOS/Windows
interchange area:

```text
/mnt/plumos-user/
  roms/ bios/ Images/ Themes/ Screenshots/ Music/
  updates/ imports/ exports/ plumos-logs/
```

The runtime binds `roms`, `bios`, `Images` and user themes into stable
compatibility paths. Executables, shared-library trees, symlinks and active
Linux state are never run directly from FAT32. Update archives remain
user-owned in `updates/` and are not automatically deleted.

During migration, `/state` binds to `/mnt/plumos/state` and `/roms` binds to
`/mnt/plumos-user/roms`, so existing Pixel2 diagnostics continue to work.

## Boot sequence

```text
Rockchip prefix
  -> U-Boot loads p1 Image and DTB
  -> stock initramfs mounts p1 and p2 using the stock boot contract
  -> stock initramfs loop-mounts the plumOS-compatible SYSTEM SquashFS
  -> stock initramfs hands off to plumOS /sbin/init
  -> mount p2 and p3, recover interrupted Runtime updates
  -> establish content bindings and mutable directories
  -> start connectivity and hardware services
  -> start the app-layer FE
  -> write renderer-ready and promote pending Runtime/System state
```

If System handoff fails during development, the stock initramfs recovery shell
is acceptable as a boot-substrate diagnostic path. Release update logic must
avoid starting a stock SYSTEM userspace.

## Build graph

All release-facing builds use the Pixel2 Docker entry point and pinned source
revisions. Component outputs are assembled rather than copied from a live
device:

```text
stock boot artifacts
System rootfs
frontend component
RetroArch component
libretro core catalog
PicoArch component
standalone emulator components
apps and service components
        -> strict app-layer + manifests/checksums/licenses
        -> seeded PLUMOS_SYS ext4
System A/B + seeded PLUMOS_SYS + Rockchip prefix
        -> verified Pixel2 release SD image
```

Development filters may build one core, but a release must use the canonical
core catalog and reject missing, stale or unmanifested binaries. Every
component records its upstream URL, immutable commit, package revision,
architecture, license and checksum set.

## Frontend launch contract

The FE catalog exposes a profile only when its managed runtime exists:

```text
retroarch:<core-id>
picoarch:<core-id>
standalone:<emulator-id>
pyxel:pixel2
external:port
```

The first vertical slice is NES through `retroarch:quicknes`. It must prove the
complete lifecycle before the wider catalog is enabled:

1. FE scans a user-provided ROM from p3.
2. The text resolver validates the system, relative ROM path and core path.
3. FE releases DRM and input ownership.
4. The launcher starts the Pixel2-built RetroArch and QuickNES core.
5. Video is correctly rotated and scaled, audio is audible, and controls work.
6. Exit returns to exactly one FE process.
7. A save/state survives FE restart and reboot on p2 or the active content
   filesystem according to the documented policy.

ROMs, copyrighted BIOS files, saves, credentials and signing private keys are
never repository or release-image inputs.

## Why this shape

The reference designs demonstrate that a read-only A/B System, transactional
ext4 runtime, host-visible FAT32 content area, component manifests, pinned core
recipes, and a frontend-to-launcher lifecycle provide bounded updates and clear
ownership. Pixel2 keeps the stock boot hook because the vendor kernel and
initramfs preserve PMIC, charger and reboot behavior. plumOS owns the mounted
System and runtime after the stock handoff.

## Release gates

- reproducible System, app-layer and filesystem hashes;
- exact stock `Image` and DTB verification;
- exact Rockchip prefix verification;
- System slot and runtime/component checksum verification;
- no foreign distribution product identity in runtime artifacts;
- no ROM, BIOS, save, secret, private key or active user configuration;
- image re-extraction and partition/filesystem verification;
- real Pixel2 cold boot, FE readiness, controls, video, audio, game launch,
  clean exit, save persistence, ADB and power behavior;
- signed Runtime/System updates and rollback before public release.
