# Pixel2 plumOS boot splash

Date: 2026-08-14

## Finding

The retained stock `Image` contains a gzip-compressed initramfs at offset
`0xdc09ac`. Its embedded fallback is a 480x640 RGB PNG at
`/splash/splash-1080.png`; visual inspection identifies this as the IUX image.
Its SHA-256 is
`d6c789c0c2ca1ead26675c7f657988f15d3d5b2bd9ce99d277ce973d723bfd18`.

The initramfs `load_splash()` function lists the boot-volume override first:

```text
/flash/oemsplash-${vres}.png
/flash/oemsplash-1080.png
/flash/oemsplash.png
/splash/splash-${vres}.png
/splash/splash-1080.png
```

However, the stock init execution order is `load_splash()` followed by
`mount_flash()`. The lookup therefore runs while `/flash` is still empty and
always selects the embedded IUX fallback. Merely adding the OEM file to the
boot FAT is not an effective override.

## Implementation

The repository-owned logical 640x480 plumOS artwork is converted to a 480x640
8-bit RGB PNG with a 90-degree counter-clockwise storage rotation. The SD image
builder installs it as `PLUMOS_BOOT:/oemsplash-1080.png`. The retained
initramfs sources `/flash/post-flash.sh` immediately after mounting the boot
volume. That hook uses the still-available initramfs `/usr/bin/ply-image` to
redraw the plumOS asset without modifying the stock `Image`, DTB, or bootloader.
The repository boot-ready asset SHA-256 is
`0db4a864a46b1e2000a55d7d7d1671e877364db4c1a2132d8a9a838b9e694082`.

The image verifier now requires all of the following:

- exact 480x640 RGB8 non-interlaced PNG format;
- byte-for-byte equality between the repository asset and the boot partition;
- `boot_splash`, geometry, and SHA-256 entries in `plumos-image.manifest`.

## Boundary

The embedded fallback can be visible briefly before `/flash` is mounted. The
hook replaces the sustained initramfs-stage IUX display at about 2.2 seconds;
eliminating even that early interval would require repacking the initramfs
inside the stock `Image`. The earlier bootloader Pixel logo and charging UI may
use separate artifacts in the Rockchip prefix and are intentionally unchanged.

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

This original host validation proved image packaging only; it did not prove
that stock init could see the override at splash-selection time.

## Existing-card deployment

After the six-icon Runtime update, the physical device still displayed IUX.
Read-only inspection showed that the running card predated the boot-splash
image build: `/flash/oemsplash-1080.png` was absent and its older
`plumos-image.manifest` had no `boot_splash` metadata. The Runtime updater had
correctly changed only `/mnt/plumos`; it cannot update the separate boot FAT.

The repository asset and updated boot manifest were pushed as temporary files
while `/flash` was briefly read-write. Both device-side SHA-256 values matched
the host before rename. After rename, sync, and returning `/flash` read-only:

```text
/flash/oemsplash-1080.png
sha256=0db4a864a46b1e2000a55d7d7d1671e877364db4c1a2132d8a9a838b9e694082
geometry=480x640
format=PNG RGB8
mount=/flash vfat ro
```

The stock `Image`, DTB, System dispatcher, and System slots were not changed.
A normal reboot returned to Runtime `0.1.0-dev-c808952`, ADB, and frontend at
device uptime 9.16 seconds, but the LCD still showed IUX. Kernel timestamps
then established the actual order:

```text
[    1.556129] init: ### Loading bootsplash
[    1.640087] init: ### Mounting flash
[    2.224341] plumos-stock-initramfs=post-flash flash-mounted=1
```

The repository hook now redraws the plumOS asset at the last event and emits
`plumos-stock-initramfs=boot-splash result=plumos` on success. Direct LCD
confirmation remains the final gate because `/dev/fb0` exposes a stale buffer
rather than reliable live-display evidence on this device.
