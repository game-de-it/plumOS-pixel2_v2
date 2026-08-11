# Pixel2 NES app-layer and SD image host validation

Date: 2026-08-12

## Scope

This record covers host-side build and image validation only. It does not claim
that RetroArch video, audio, controls, exit hotkeys, or save persistence have
passed on physical Pixel2 hardware. No commercial ROM or BIOS content was used
or included.

## Built components

- frontend, library scanner, and `plumos-text-ui` for AArch64
- RetroArch 1.22.2 from pinned commit
  `69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576`
- QuickNES from pinned commit
  `058d66516ed3f1260b69e5b71cd454eb7e9234a3`
- strict Pixel2 app layer with component manifests and SHA-256 lists
- plumOS-owned SYSTEM and a transitional 2 GiB SD image

The strict app-layer manifest reports `complete=true` and exposes only
`retroarch:quicknes`. An ephemeral, non-functional NES-shaped fixture was
scanned; the text resolver reported `can_execute: yes` for the generated
QuickNES launch plan. RetroArch `--version`, the text resolver, frontend
diagnostics, and the core ELF architecture were executed or inspected in the
AArch64 tools container.

## Image result

Command:

```sh
./scripts/docker-build.sh release-image
./scripts/docker-build.sh sd-image
```

Output:

```text
output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img
SHA-256 0115a1d57ad906181750061c237a0c33425d956c59052672aeb5f0da8eb1e03a
source_ref c814221
```

The verifier confirmed the captured 16 MiB Rockchip prefix, MBR boundaries,
FAT/ext4 filesystem checks, kernel, DTB, SYSTEM contents, app-layer root and
component checksums, AArch64 executable format, user-data directories, and the
foreign-distribution identity gate.

The current bring-up image still uses the transitional geometry:

- 16 MiB raw Rockchip prefix
- 256 MiB `PLUMOS_BOOT`
- 512 MiB `PLUMOS_SYS`, seeded with the app layer
- remaining space as `PLUMOS_USER`

The final 512 MiB System A/B boot partition, 2048 MiB runtime seed, and
first-boot expansion/provisioning contract remain open in `TODO.md`.

## Reproducibility

Two consecutive `sd-image` builds at source ref `c814221` produced the same
complete image SHA-256:

```text
repeat_before=0115a1d57ad906181750061c237a0c33425d956c59052672aeb5f0da8eb1e03a
repeat_after=0115a1d57ad906181750061c237a0c33425d956c59052672aeb5f0da8eb1e03a
```

The ext4 population step normalizes inode timestamps and runs `debugfs` under a
fixed clock. This removes host copy time from the filesystem hash.

## Physical-device gate

Before this becomes the default image, flash a separate test SD and confirm:

- frontend scans a user-provided NES ROM and releases display/input ownership
- QuickNES video orientation, scaling, and frame pacing
- ALSA audio and all required controls, including the exit hotkey
- return to the frontend after emulator exit
- save and state persistence after reboot
- ADB remains available for log capture

