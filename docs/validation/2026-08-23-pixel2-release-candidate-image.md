# Pixel2 0.1.0 RC1 SD image

## Scope

The release-candidate image was built from clean Git source ref `28aaf65` as
version `0.1.0-rc1-28aaf65`. It includes all managed Pixel2 components built by
the repository `release-image` target. ROMs, commercial BIOS files, saves,
network credentials, and other mutable user data are not release-image inputs.

```text
image: plumOS-Pixel2-0.1.0-rc1-28aaf65.img
size: 2701131776 bytes
sha256: ca9275c3cdc352e134d3eab9b31e7313ffdc1ea13c239127bf345f610f108929
source_ref: 28aaf65
source_date_epoch: 1787480508
```

The embedded filesystem identities are:

```text
stock boot prefix: c434f3f4ba7ed3077efc13f2a22a92b4b1519ed381fbe64ad5caa34221039814
PLUMOS_BOOT FAT32: f24960289f62150127d965944be8b93ff75686326d1fca9a1d6d47f471809fc2
PLUMOS_SYS ext4: d6e2bb2ae9f126130d54a67461b4731b1c052f9f0924fe6b7b7573b5c7c1edc7
```

## Build and inventory

The candidate was produced with four concurrent libretro-core workers:

```sh
PLUMOS_PIXEL2_VERSION=0.1.0-rc1-28aaf65 \
PLUMOS_PIXEL2_CORE_BUILD_CONCURRENCY=4 \
./scripts/docker-build.sh release-image
```

The final release audit reported:

```text
enabled systems: 88 / 97
libretro cores: 109 / 109 pass
standalone: 5 built / 4 libretro-covered deferred / 0 pending
visible Apps: 7
required components: 11 / 11
release blockers: 0
```

The strict app-layer manifest records `complete=true` and
`missing_components=[]`. Frontend, RetroArch, libretro cores, PicoArch,
standalone emulators, audio routing, Pyxel, NextCommander, Music Player,
Network Services, PortMaster, System A/B, and the stock-compatible dispatcher
were rebuilt for the RC version.

## Host validation

The generated image passed all of the following gates:

- registered 16 MiB Rockchip boot-prefix SHA and stock kernel/DTB boundary;
- frontend, app-layer, and release implementation audit;
- System dispatcher and byte-identical System A/B SquashFS verification;
- FAT32 and ext4 filesystem checks, labels, fixed partition geometry, and
  embedded Runtime checksum extraction;
- 16 GB first-boot simulation: grow `PLUMOS_SYS` to 8192 MiB and create the
  remaining FAT32 `PLUMOS_USER` partition;
- update, power, Wi-Fi recovery, USB-host re-enumeration, audio volume,
  RetroArch OSD/menu/localization, emulator menu, PortMaster, BIOS preparation,
  Pyxel geometry, stock capture, and update-health contract tests.

The macOS asynchronous sleep fixture stopped once after the successful kernel
sleep body because its background recovery assertion raced the test process.
Its standalone rerun passed, as did the app-layer aggregate rerun and the
previous physical Pixel2 sleep matrix. No candidate payload was changed or
excluded because of this host-only timing fluctuation.

The SD assembly was then executed a second time from the same source and
version. Both independently generated image hashes were identical:

```text
first:  ca9275c3cdc352e134d3eab9b31e7313ffdc1ea13c239127bf345f610f108929
second: ca9275c3cdc352e134d3eab9b31e7313ffdc1ea13c239127bf345f610f108929
image_reproducibility=result-ok
```

## Physical release gate

Flash this image to a separate SD card of at least 16 GB. The remaining release
gate is a fresh-card boot, not an update of the development card:

1. confirm first-boot setup, its one permitted early reboot, and FE startup;
2. confirm `PLUMOS_SYS` is 8192 MiB and `PLUMOS_USER` owns the remaining card;
3. reboot and cold boot with the UGREEN AC650 attached, then confirm saved Wi-Fi;
4. confirm LCD orientation, controls, START -> POWER, audio, sleep/wake,
   shutdown, and charging behavior;
5. provision test ROM/BIOS content on `PLUMOS_USER` and smoke the representative
   RA, PicoArch, standalone, Apps, Pyxel, PICO-8, and PortMaster paths.

