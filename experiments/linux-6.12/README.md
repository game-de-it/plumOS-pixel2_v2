# Pixel2 Linux 6.12 experiment

This directory preserves the superseded plumOS-owned Linux 6.12.79 and
initramfs bring-up path for engineering reference only.

Pixel2 production and release images use the checksum-registered stock
5.10.198 `Image`, embedded initramfs, runtime DTB, modules, and Rockchip boot
prefix described by Decision 0004. No script under `scripts/`, including
`docker-build.sh release-image`, references this directory.

The experiment is deliberately opt-in and writes only below
`output/experimental/linux-6.12/`:

```sh
PLUMOS_ENABLE_EXPERIMENTAL_LINUX_6_12=1 \
  ./experiments/linux-6.12/build-kernel.sh
```

Run its isolated contract test with:

```sh
./experiments/linux-6.12/test-build-kernel.sh
```

Artifacts produced here must not be copied into `PLUMOS_BOOT`, System A/B,
Runtime packages, SD images, or release archives without a new accepted
architecture decision that supersedes Decision 0004.
