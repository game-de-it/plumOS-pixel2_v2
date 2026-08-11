# Pixel2 stock SD boot analysis

Date: 2026-08-11
Source device at capture time: `/dev/disk4`, 62,239,277,056 bytes

## Partition layout

| Partition | Start LBA | Sectors | Observed role |
| --- | ---: | ---: | --- |
| unpartitioned prefix | 0 | 32768 | Rockchip boot payload; privileged capture pending |
| p1 FAT32 | 32768 | 5967171 | kernel, DTB and System file |
| p2 Linux | 6000001 | 6871679 | writable `/storage` |
| p3 FAT32 | 12871680 | 478335 | ROM content before first-boot expansion |

Approximately 55.4 GB remains unallocated. The embedded initramfs contains an
optional p3 expansion path.

## Boot path proven from local artifacts

1. U-Boot loads the AArch64 `Image` and `rk3326s-gkd-pixel2.dtb` from p1.
2. Linux 5.10.198 starts the gzip-compressed initramfs embedded in `Image`.
3. `/init` mounts p1 at `/flash` and loop-mounts `/flash/SYSTEM` read-only.
4. `/init` mounts p2 read-write at `/storage`.
5. The analyzed stock flow switches into its SquashFS and starts systemd.
6. Its automount layer discovers p3 and exposes its ROM tree below `/storage`.

Steps 3-6 describe the source behavior only. The target plumOS image replaces
the userspace from `/init` onward.

## Artifact identity

| File | SHA-256 |
| --- | --- |
| `Image` | `853eb041f1042a5f54ab66143cc8babb3942936f5c5209bc0c05d439ec3bd466` |
| `SYSTEM` analysis input | `a01afb14d7124a65787c6040575c309509c34146bc264d1d16c84eb7c5a26c8c` |
| Pixel2 DTB | `a7a438f705f994a9f333b2f334a803d47bc00cae6ed4556d51c730604452757a` |
| U-Boot DTB backup | `1ded3996b8fbcd9236b08f7b99fdb253c6ca97e5f7b6c307b639545b3906b245` |

## Hardware contract from DTB

- compatible: `GameKiddy,gkd-pixel2`, `rockchip,rk3326`
- four Cortex-A35 CPUs
- ST7703 two-lane MIPI DSI panel, native 480x640, rotation 90 degrees
- RK817 PMIC, codec, battery and charger
- `gamekiddy-joypad` evdev source named `pixel2_joypad`
- PWM backlight and PWM vibrator
- volume keys from `gpio-keys`
- PX30 Mali Bifrost GPU node

## Safety observation

A read-only macOS verification of p3 returned code 206 with an FSInfo free-space
mismatch and allocation warnings. No repair was requested. Preserve the source
card and perform all writes and boot tests on a duplicate.

