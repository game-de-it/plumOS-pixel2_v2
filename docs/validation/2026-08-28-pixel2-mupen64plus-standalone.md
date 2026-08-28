# Pixel2 standalone Mupen64Plus validation

Date: 2026-08-28

## Scope

Pixel2 now packages standalone Mupen64Plus as an additional Nintendo 64
profile. The existing, device-proven `retroarch:parallel_n64` profile remains
the default. This work does not restore the rejected Mupen64Plus-Next libretro
core.

The standalone build pins all six upstream 2.6.0 components by full commit:

- console UI;
- core;
- SDL audio;
- SDL input;
- HLE RSP;
- Rice video.

The Pixel2 integration adds a Rice GLES2 final-present adapter for the native
480x640 panel, a `pixel2_joypad` input profile, FUNCTION-button exit with
process ownership validation, plumOS ALSA routing, isolated mutable config and
screenshots, component metadata, checksums, and license material.

## Host evidence

The filtered build and the complete six-emulator standalone build both
completed. The four independent Mupen64Plus plugins were compiled concurrently
after the core API build. The complete app-layer reports:

```text
standalone: 6 built / 4 libretro-covered deferred / 0 pending
release blockers: 0
```

The following checks passed:

```text
./tests/test-app-layer-scripts.sh
./scripts/build-standalone-pixel2.sh --filter mupen64plus
./scripts/build-standalone-pixel2.sh
./scripts/build-frontend-component.sh
./scripts/build-app-layer.sh --strict
./scripts/audit-pixel2-implementation.py --release-gate
```

The strict app-layer verifier checked both standalone component checksums and
the root checksum index. Its license audit found 168 packaged license files.
An AArch64 container runtime check resolved every dynamic dependency for the
console, core, and four plugins and successfully loaded the pinned core for
`mupen64plus --help`.

```text
standalone checksums: 9906b53fb48d0ccbfb93694f2773e0aab261ef451b95c83678ec5d3f0387523b
root checksums:       9ea508d63560f8c11418e8fbb62bba694fb70bc73663df1fcfab9063e9818e20
```

## Device acceptance boundary

The device did not answer SSH at the known Pixel2 addresses during this pass.
Deployment and the physical N64 checks remain open: launch, orientation and
aspect, D-pad and buttons, audio, FUNCTION exit, frontend return, and a second
launch with persisted user configuration.

