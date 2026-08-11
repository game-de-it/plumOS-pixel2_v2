# Pixel2 plumOS host build validation

Date: 2026-08-11
Scope: host-side build and image structure only

## Result

The repository now builds a plumOS-owned Linux kernel, embedded initramfs,
minimal System SquashFS and a complete 2 GiB SD image. The stock userspace is
not copied. The only retained inputs are the Rockchip boot prefix and selected
USB Wi-Fi firmware captured from the stock kernel overlay.

Initial image generation was exercised with an explicit 16 MiB all-zero test
prefix. That structural test image was deleted. The real prefix was then
captured read-only from `/dev/disk4` and used for the final host build below.

## Kernel result

- Linux release: `6.12.79-plumos-pixel2`
- `Image` SHA-256: `d3c55bb51c162863bfe2db4025e0297308f0071e1d9cfe5b2b7748d81e38dfdf`
- DTB SHA-256: `c97dc56e123308171ddb192f251cb42975418c0678d4a012143590ccf4f61f40`
- installed modules: 289
- initramfs owner: plumOS
- gamepad binding: standard `gpio-keys`
- panel compatible: `plumos,generic-dsi`

The kernel.org archive hash and external Pixel2 hardware-support commit are
pinned by `scripts/build-kernel.sh`. Runtime `Image` and DTB scans contain no
foreign distribution identity.

## System result

- SquashFS size: 14,417,920 bytes
- SquashFS SHA-256: `4237aedcb9beb34aeb34b37df1396d17022c94a963110e51e1200133298bf1a0`
- kernel module ABI: `6.12.79-plumos-pixel2`
- selected USB Wi-Fi firmware files: 14
- maintenance paths: development ADB, optional USB Wi-Fi and key-only SSH

The verifier re-extracted the SquashFS, checked managed file hashes and module
vermagic, executed the principal arm64 tools in chroot, and scanned paths and
content for foreign distribution identity.

## SD image structure result

| Region | Start sector | Size | Format/label |
| --- | ---: | ---: | --- |
| Rockchip prefix | 0 | 16 MiB | captured binary input |
| p1 | 32768 | 256 MiB | FAT32 `PLUMOS_BOOT` |
| p2 | 557056 | 512 MiB | ext4 `PLUMOS_STATE` |
| p3 | 1605632 | 1,325,400,064 bytes | FAT32 `PLUMOS_ROMS` |

Two builds using the same synthetic prefix produced identical hashes for every
filesystem and the complete image. Verification also checked the preserved
prefix bytes (excluding the regenerated MBR), partition boundaries, FAT and
ext4 consistency, embedded boot payload equality and embedded SquashFS.

## Real-prefix image result

- prefix size: 16,777,216 bytes
- prefix SHA-256: `c434f3f4ba7ed3077efc13f2a22a92b4b1519ed381fbe64ad5caa34221039814`
- image: `plumOS-Pixel2-0.1.0-dev.img`
- image size: 2,147,483,648 bytes
- image SHA-256: `bed189c699dcea8f013eebeb91d03351b0408c700fc76e0a7bc275683f4dfff6`
- source commit: `abdd08a`

The real image passed prefix byte comparison, partition boundary checks,
read-only FAT/ext4 checks, embedded payload comparison, SquashFS verification,
module ABI verification and foreign distribution identity gates.

## Gates still requiring hardware

1. Write only to a separate SD card.
2. Prove cold boot, LCD, controls, audio, power behavior, ADB and at least one
   supported USB Wi-Fi dongle on the physical Pixel2.
3. Replace development no-auth ADB with authentication or explicit opt-in
   before a release image.
