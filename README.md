# plumOS Pixel2

[日本語](README.ja.md)

plumOS Pixel2 is a reproducible Linux build for the RK3326S-based GKD Pixel2,
assembled from auditable components and checksum metadata.

The accepted boot boundary keeps the stock Pixel2 Rockchip prefix, kernel
5.10.198, DTB, and embedded initramfs as the boot substrate. After the stock
initramfs hands off to `/boot/SYSTEM`, plumOS owns `/sbin/init`, the runtime,
frontend, services, settings, and emulator launch paths. The normal stock
userspace and frontend are not used.

## Current implementation

- plumOS `SYSTEM` SquashFS, init, ADB, USB Wi-Fi, SSH, and persistent logs;
- Pixel2 frontend, START menu, ROM scanner, and physical input contract;
- RetroArch with 114 libretro cores and PicoArch;
- OpenBOR, DraStic, and PPSSPP standalone runtimes;
- bundled Python 3.11, Pyxel, and pygame runtime;
- plumOS audio routing for RK817/USB plus global volume and brightness;
- component manifests/checksums, strict app-layer assembly, and a 4 GiB SD
  image builder.

A built component is not automatically hardware-proven. User-visible backend
gaps, pending Apps and standalones, updates, and full-system device validation
are tracked in the [Pixel2 Implementation Inventory](docs/developer/implementation-status.md)
and [TODO](TODO.md).

## Build

Docker Desktop or a compatible Docker engine is required, together with boot
artifacts captured read-only from a stock Pixel2. Private stock captures are
not committed to Git.

```sh
./scripts/docker-build.sh frontend
./scripts/docker-build.sh retroarch
./scripts/docker-build.sh core-catalog --filter all --concurrency 4
./scripts/docker-build.sh picoarch
./scripts/docker-build.sh standalone
./scripts/docker-build.sh pyxel-runtime
./scripts/docker-build.sh app-layer --strict
./scripts/docker-build.sh audit
./scripts/docker-build.sh system-rootfs
./scripts/docker-build.sh sd-image
```

Generated artifacts live below `output/`. The current 4 GiB seed image uses a
stock-compatible boot prefix, a 512 MiB `PLUMOS_BOOT`, a 2048 MiB ext4
`PLUMOS_SYS`, and a FAT32 `PLUMOS_USER` occupying the remainder.

`release-image` fails until the implementation audit reports zero release
blockers. Use `sd-image` for development hardware testing, and always flash a
card separate from the original stock SD.

## Validation

```sh
./tests/test-app-layer-scripts.sh
./tests/test-system-rootfs-scripts.sh
./tests/test-sd-image-scripts.sh
./scripts/verify-app-layer.sh output/app-layer/pixel2/plumos
./scripts/audit-pixel2-implementation.py --release-gate
```

A successful host build or checksum does not prove Pixel2 LCD orientation,
controls, audio, frame pacing, exit, saves, or power behavior. Physical-device
evidence belongs under `docs/validation/`.

Start with the [Developer Guide](docs/developer/README.md).
