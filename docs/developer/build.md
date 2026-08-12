# Build Guide

## Requirements

- Git
- Docker Desktop or compatible Docker engine
- enough disk for ARM64 build outputs and generated images
- stock Pixel2 boot artifacts already captured under `artifacts/`

Generated artifacts live under `output/`. Stock captures and private inputs
stay out of git.

## Main Commands

```sh
./scripts/docker-build.sh frontend
./scripts/docker-build.sh retroarch
./scripts/docker-build.sh cores --filter all
./scripts/docker-build.sh picoarch
./scripts/docker-build.sh standalone
./scripts/docker-build.sh app-layer --strict
./scripts/docker-build.sh system-rootfs
./scripts/docker-build.sh sd-image
```

Host contract checks:

```sh
./tests/test-app-layer-scripts.sh
./tests/test-system-rootfs-scripts.sh
./scripts/verify-app-layer.sh output/app-layer/pixel2/plumos
```

## App Layer

The app-layer root is `output/app-layer/pixel2/plumos`. It must contain:

- root `manifest.json`, `checksums.sha256`, `VERSION`, `COMPAT_VENDOR`, and
  `RUNTIME_ABI`;
- component manifests/checksums for frontend, RetroArch, libretro cores,
  PicoArch, standalone launchers, and audio routing;
- Pixel2 frontend binary, scanner, text UI, hardware-key daemon, safe shutdown,
  input-map files, RetroArch, PicoArch, available standalone launchers, and
  canonical libretro cores.

Strict assembly must emit `complete=true` and `missing_components=[]`.

## SYSTEM

`system-rootfs` creates:

```text
output/system-rootfs/pixel2/payload/SYSTEM
output/system-rootfs/pixel2/payload/SYSTEM.manifest
```

The manifest records `source_ref`, `image_size`, and `image_sha256`. Replacing a
live device `SYSTEM` requires remounting `/boot` writable only for the copy and
returning it to read-only afterward.

## Live Deployment Rule

Never deploy one app-layer binary by itself. Deploy the changed managed files
with matching root/component checksums and manifests, verify in staging, extract
to `/mnt/plumos`, then run `sha256sum -c /mnt/plumos/checksums.sha256`.
