# Pixel2 libretro standard set build

Date: 2026-08-12

Scope:

- Pixel2 libretro recipe catalog and filtered build script
- Pixel2 Docker toolchain dependencies needed by the standard A/B core set
- app-layer component manifest/checksum generation

Host commands:

```sh
./scripts/docker-build.sh image
./scripts/docker-build.sh cores --filter plumos --jobs 4 --fail-on-error 1
```

Result:

```text
created: /work/output/libretro-cores/pixel2-filtered/plumos/plumos
built: 41
failed: 0
skipped: 73
```

Component checks:

```sh
( cd output/libretro-cores/pixel2-filtered/plumos/plumos && \
  sha256sum -c components/libretro-cores/checksums.sha256 )
```

The generated component manifest reports:

```json
{
  "name": "plumOS Pixel2 libretro cores",
  "component": "libretro-cores",
  "device": "pixel2",
  "architecture": "aarch64",
  "filter": "plumos",
  "built": 41,
  "failed": 0,
  "skipped": 73
}
```

Notes:

- `pcsx_rearmed` and `scummvm` require `SOURCE_DATE_EPOCH` to be unset when it
  is exported as an empty string by the host wrapper.
- `easyrpg` requires the same class of build dependencies already present in
  the sibling plumOS toolchains, including `libexpat1-dev`, `libfmt-dev`, and
  `libpixman-1-dev`.
- `dosbox_pure` stages both the canonical filename and a compatibility alias,
  so the component has 41 catalog cores but 42 `*_libretro.so` files.

Device validation:

- Not performed in this step. Real-device launch validation will be done from
  the assembled app-layer and the ROM set under
  `/Volumes/public-1/02/motoki/emu/ROM/rom2`.
