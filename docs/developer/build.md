# Build Guide

[日本語](build.ja.md)

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
./scripts/docker-build.sh kernel-modules
./scripts/docker-build.sh retroarch
./scripts/docker-build.sh cores --filter all
./scripts/docker-build.sh core-catalog --filter all --concurrency 4
./scripts/docker-build.sh picoarch
./scripts/docker-build.sh standalone
./scripts/docker-build.sh pyxel-runtime
./scripts/docker-build.sh app-layer --strict
./scripts/docker-build.sh audit
./scripts/docker-build.sh system-rootfs
./scripts/docker-build.sh sd-image
```

Host contract checks:

```sh
./tests/test-app-layer-scripts.sh
./tests/test-system-rootfs-scripts.sh
./scripts/verify-app-layer.sh output/app-layer/pixel2/plumos
./scripts/audit-pixel2-implementation.py \
  --markdown output/validation/pixel2-implementation-audit.md \
  --json output/validation/pixel2-implementation-audit.json
```

For a complete local publication candidate, follow the
[release process](release-process.md). It adds strict content/license gates,
image reproducibility, compression round-trip verification, and a versioned
GitHub-ready bundle without publishing it.

`audit` is informational during normal port development. Its
`--release-gate` form fails while any user-visible Pixel2 setting, Apps entry,
launch profile, selectable language, or enabled-system theme asset lacks its
managed implementation. `release-image` runs this gate automatically; a
development `sd-image` remains available while the known work list is being
implemented.

`kernel-modules` pins the Pixel2 5.10.198 source and the V90S-proven `8821cu`
source. Before accepting the external module it rebuilds stock `r8188eu` and
requires its `srcversion` and `vermagic` to match the captured stock module.
`system-rootfs` and `release-image` run the same gate automatically.

## Kernel boundary

All normal targets use the registered stock Pixel2 5.10.198 boot substrate.
The superseded Linux 6.12 bring-up path lives only under
`experiments/linux-6.12/`; it is absent from `docker-build.sh` targets and
release inputs, requires `PLUMOS_ENABLE_EXPERIMENTAL_LINUX_6_12=1`, and writes
only to `output/experimental/linux-6.12/`. See Decision 0004 before changing
this boundary.

## App Layer

The app-layer root is `output/app-layer/pixel2/plumos`. It must contain:

- root `manifest.json`, `checksums.sha256`, `VERSION`, `COMPAT_VENDOR`, and
  `RUNTIME_ABI`;
- component manifests/checksums for frontend, RetroArch, libretro cores,
  PicoArch, standalone launchers, audio routing, and Pyxel;
- Pixel2 frontend binary, scanner, text UI, hardware-key daemon, safe shutdown,
  input-map files, RetroArch, PicoArch, available standalone launchers, and
  canonical libretro cores;
- bundled Pixel2 Python/Pyxel runtime plus `Pyxel Setup`, which may install
  project-specific Pyxel requirements into mutable state without replacing the
  packaged baseline.

Strict assembly must emit `complete=true` and `missing_components=[]`.

For full libretro coverage, prefer `core-catalog` over one monolithic `cores`
build. It follows the same plumOS family design intent as MF/V90S: build each
catalog core as an independent work unit, reuse validated per-core outputs, and
aggregate a single `libretro-cores` component manifest/checksum tree for the
Pixel2 app-layer.

`standalone --filter <id>` is useful for quick emulator iteration, but it
intentionally emits a filtered standalone component. Before assembling the final
app-layer or SD image, rebuild `./scripts/docker-build.sh standalone` without a
filter so built standalone binaries such as OpenBOR, DraStic, and PPSSPP are
present together.

Pyxel is also a required app-layer component. If `pyxel:pixel2` remains in the
frontend catalog, `./scripts/verify-app-layer.sh` must be able to find the
launcher, bundled Python runtime, Pyxel package, component manifest, and
checksums. Do not disable or hide Pyxel in the FE as a substitute for fixing a
missing runtime.

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
