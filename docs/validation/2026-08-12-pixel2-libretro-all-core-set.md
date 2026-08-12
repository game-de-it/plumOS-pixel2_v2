# Pixel2 libretro all-core build

Date: 2026-08-12

Scope:

- Pixel2 canonical libretro recipe catalog
- full `all` filter build used for full-system frontend route coverage
- `libretro-cores` component manifest/checksum generation

Host command:

```sh
./scripts/docker-build.sh cores --filter all --jobs 4 --fail-on-error 1
```

Result:

```text
created: /work/output/libretro-cores/pixel2/plumos
built: 114
failed: 0
skipped: 0
```

Component checks:

```sh
( cd output/libretro-cores/pixel2/plumos && \
  sha256sum -c components/libretro-cores/checksums.sha256 )
```

The generated component manifest reports:

```json
{
  "name": "plumOS Pixel2 libretro cores",
  "component": "libretro-cores",
  "device": "pixel2",
  "architecture": "aarch64",
  "filter": "all",
  "built": 114,
  "failed": 0,
  "skipped": 0
}
```

Notes:

- The component contains 118 `*_libretro.so` files because several cores stage
  compatibility aliases in addition to their canonical catalog IDs.
- `ecwolf` needs a targeted submodule update. Its legacy SDL submodules point
  at unavailable upstream hosts, while the libretro target only needs
  `src/libretro/libretro-common`.

Device validation:

- Not performed in this step. Real-device launch validation will be done after
  PicoArch, standalone launchers, app-layer assembly, and ROM staging are
  updated for the same build.
