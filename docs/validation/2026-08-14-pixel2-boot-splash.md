# Pixel2 plumOS boot splash

Date: 2026-08-14

## Finding

The retained stock `Image` contains a gzip-compressed initramfs at offset
`0xdc09ac`. Its embedded fallback is a 480x640 RGB PNG at
`/splash/splash-1080.png`; visual inspection identifies this as the IUX image.
Its SHA-256 is
`d6c789c0c2ca1ead26675c7f657988f15d3d5b2bd9ce99d277ce973d723bfd18`.

The initramfs `load_splash()` function searches the mounted boot volume first:

```text
/flash/oemsplash-${vres}.png
/flash/oemsplash-1080.png
/flash/oemsplash.png
/splash/splash-${vres}.png
/splash/splash-1080.png
```

This provides a supported override boundary. plumOS does not need to patch or
repack the retained kernel/initramfs.

## Implementation

The repository-owned logical 640x480 plumOS artwork is converted to a 480x640
8-bit RGB PNG with a 90-degree counter-clockwise storage rotation. The SD image
builder installs it as `PLUMOS_BOOT:/oemsplash-1080.png`.
The repository boot-ready asset SHA-256 is
`0db4a864a46b1e2000a55d7d7d1671e877364db4c1a2132d8a9a838b9e694082`.

The image verifier now requires all of the following:

- exact 480x640 RGB8 non-interlaced PNG format;
- byte-for-byte equality between the repository asset and the boot partition;
- `boot_splash`, geometry, and SHA-256 entries in `plumos-image.manifest`.

## Boundary

This changes the initramfs-stage IUX splash only. The earlier bootloader Pixel
logo and the charging UI may use separate artifacts in the Rockchip prefix and
are intentionally unchanged. Physical validation must confirm the plumOS logo
orientation during normal boot without changing charging/reboot behavior.

## Host validation

The following checks passed from clean commit `aa0fafc`:

```text
./tests/test-sd-image-scripts.sh
./tests/test-stock-capture-scripts.sh
./scripts/docker-build.sh sd-image

pixel2-boot-splash: PASS geometry=480x640 format=png-rgb8
sd_image=result-ok
```

The complete generated image identity is:

```text
output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img
size=4294967296
sha256=fb1a0d40b3039236c2cfb73cc26e905f14fed5b80a74daf9e0d4539878ebac10
source_ref=aa0fafc
```

Physical normal-boot orientation remains the final acceptance gate.
