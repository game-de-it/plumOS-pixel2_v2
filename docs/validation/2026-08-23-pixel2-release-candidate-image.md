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

## RC1 physical readback

The candidate was written to a 64 GB SD and booted on Pixel2. The running
app-layer reported exactly the embedded release identity:

```text
VERSION=0.1.0-rc1-28aaf65
source_ref=28aaf65
kernel=5.10.198
```

First-boot provisioning completed in one session. Its durable log records the
partition-table write, online ext4 resize, user-directory seed, and final
`layout=pixel2-compact-seed-v1` result. Readback after the operation was:

```text
p1 PLUMOS_BOOT  start=32768    sectors=1048576   size=512 MiB
p2 PLUMOS_SYS   start=1081344  sectors=16777216  size=8192 MiB
p3 PLUMOS_USER  start=17858560 sectors=104280064 size=49.7 GiB
PLUMOS_SYS UUID=504c554d-5354-4154-4500-000000000002
```

`ext4-resized`, `userdata-seeded`, and `complete` markers were present. The
card still contained a correctly labelled FAT32 signature and about 4.5 GiB of
content beyond the compact image boundary, so the non-destructive provisioner
reused it instead of formatting p3. This physically proves the geometry,
resize, seed, and preservation paths. The blank-p3 formatting path remains
covered by the 16 GB host fixture rather than this reused card.

Runtime and boot readback passed:

```text
verify-runtime: result-ok
System A squashfs: 3bf414867e55a9e346b5471b4e24cb1688d664e79e6a413246856237a16977c6 match=yes
System B squashfs: 3bf414867e55a9e346b5471b4e24cb1688d664e79e6a413246856237a16977c6 match=yes
system-active=a
system-booted=a
health=system_baseline_healthy
```

The scripted frontend path showed six START entries, one consolidated `POWER`
entry, and the four-item Sleep/Reboot/Shutdown/Cancel power menu with Cancel as
the safe default. A normal safe reboot then disconnected the old session,
produced `/tmp/plumos-fe-ready` at about 20 seconds of boot, acquired the saved
UGREEN AC650 address at about 31 seconds, and restored SSH at
`192.168.10.107` at about 39 seconds.

The first manual Wi-Fi connect attempt stopped before WPA reached COMPLETED;
DHCP was never started. The second attempt reached WPA COMPLETED and obtained
the `.107` lease. A second independently initialized SD reproduced the same
sequence with the same password, disproving the original password-error
hypothesis. The manual path used the 15-second boot-recovery bound, while a
cold RTL8821CU association could take slightly longer. In addition, the failed
candidate remained alive on a fresh card with no previous config, warming the
driver/BSS state without ever requesting DHCP. This made the second attempt
systematically more likely to succeed.

Commit `dccd872` gives only explicit FE connections a 30-second WPA bound and
always stops a failed uncommitted candidate. The deployed fix connected the
same device at `.107` after 18 seconds without changing the saved config. The
active adapter readback was `0bda:c811`, driver `rtl8821cu`, RSSI -44, and
434 Mbps link speed. See
[Pixel2 first Wi-Fi connection](2026-08-23-pixel2-first-wifi-connect.md).

The image recorded by this document remains an exact artifact from `28aaf65`
and therefore does not contain `dccd872`. It is superseded for release use; a
new candidate image must be generated from `dccd872` or later after the
remaining release gates are closed.

## RC1 media smoke

Existing user content preserved on this SD allowed a release-image smoke test.
The checks prove process startup, a non-black panel-sized capture where the
renderer exposes one, and an advancing ALSA playback pointer. They do not prove
controls, orientation, exact aspect ratio, or audible quality.

| route | startup | screen capture | ALSA |
|---|---:|---:|---:|
| RetroArch QuickNES | pass | pass | pass |
| RetroArch FCEUmm | pass | pass | pass |
| RetroArch Nestopia | pass | pass | pass |
| PicoArch QuickNES | pass | pass | pass |
| PicoArch FCEUmm | pass | pass | pass |
| PicoArch Nestopia | pass | pass | pass |
| PPSSPP | pass | pass | pass |
| PICO-8 standalone | pass | pass | pass |
| OpenBOR | pass | manual | pass |
| Pyxel | pass | manual | pass |

OpenBOR and Pyxel remained alive with advancing audio, but both the DRM plane
and `/dev/fb0` readback were black on their legacy scanout path. Their physical
LCD output therefore remains an operator check. Drastic was not rerun because
this reused card's current frontend cache contains no NDS content. The
validator returned the device to a healthy FE after every route.

The mounted `PLUMOS_USER` read-only fsck reported only the expected live-mount
dirty bit and left the filesystem unchanged. An offline clean-bit assertion is
not inferred from that mounted check.

An FE-idle sleep used kernel `mem` with a five-second RTC wake. The original SSH
session timed out while suspended, as expected, but the durable log recorded
kernel resume, display unblank, RK817 speaker/headphone route re-arm, and Wi-Fi
recovery. After resume, ping had zero loss and SSH, FE, and the hardware-key
service were all running.

The remaining release acceptance is the operator-visible LCD/input/audible
audio check, a physical-power-button sleep check, and the RC1-card shutdown,
charging, and power-on sequence.

## Media dropout after the initial RC1 checks

The first physical card was later excluded from release acceptance. It became
partly unreadable after a Neo Geo session, showed `NO SD` on the next power-on,
and recovered only after physical reseating. The following boot logged an
initial `All phases bad` / SD tuning error before succeeding on retry, and the
card identifies only as generic `USD` with zero manufacturer/OEM fields.

The recovered card passed the complete Runtime, both System slots, boot FAT,
and Neo Geo ROM readback, so no persistent image corruption was found. This is
treated as a marginal card/contact incident, not an RC1 software pass. Final
acceptance must use a known-good branded SD. See
[Pixel2 RC1 SD dropout investigation](2026-08-23-pixel2-rc1-sd-dropout.md).
