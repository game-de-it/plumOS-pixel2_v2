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
process ownership validation, plumOS ALSA routing, isolated mutable config,
runtime data and screenshots, component metadata, checksums, and license
material. Rice's writable ROM-option database is staged under `/run` for each
launch; the packaged data directory is never passed to the plugin as writable
runtime state.

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
standalone checksums: 6862960baacaf2e8a9b839e481c3b62cd00ff88714ecf89fde486c72cf9a6457
root checksums:       ab1280690eca94707080cf82e84765a6adc5ad66aec9cebc8bc291b64826932c
```

## Device deployment and automated acceptance

The signed Runtime delta from `0.1.2-dev-198acfa` to
`0.1.2-dev-d4ab428` was deployed through the normal updater. Its SHA-256 is
`9beceea8c4dbffb79eccd678ff082afb7c1dfda92c44a4b9841e3c33424d6df2`.
The device accepted the signature and exact source version, rebooted, removed
the request and pending state, and reported the target version and source ref.
The complete installed Runtime verifier passed before the launch test and
again after it.

The first device pass exposed two issues that host checks could not prove:

- the GL adapter was loaded but had not resolved the real
  `SDL_GL_GetProcAddress` when Rice used a different lookup order, leaving the
  captured game sideways;
- Rice rewrote packaged `RiceVideoLinux.ini` on exit, causing one failure in
  the 11,328-file Runtime checksum set.

Commit `9495fa5` makes the presenter resolve the SDL helper independently of
client lookup order. Commit `d4ab428` stages the five shared Mupen64Plus data
files in a launch-private cache directory and removes that directory on exit.
After deployment, the log contains:

```text
[plumOS] Mupen64Plus GL rotation context: OpenGL ES 3.1 Mesa 22.3.6
[plumOS] Mupen64Plus GL rotation: logical=640x480 present=480x640+0+0 scanout=480x640 @ 270
```

The device media validator launched `N64/SUPERMARIO64.Z64` with
`standalone:mupen64plus` and reported `startup=pass`, `screen=pass`, and
`audio=pass` at 84% battery. Visual review confirmed an upright 640x480 4:3
logical frame and the required rotated 480x640 physical scanout. ALSA playback
advanced, the `pixel2_joypad` profile was selected, and the FUNCTION helper
monitored `BTN_TRIGGER_HAPPY1`. The test stopped the emulator and restored the
frontend. No launch-private data directory remained, packaged
`RiceVideoLinux.ini` retained SHA-256
`ec89fe5ab5760b94b822b41dc2889afc280a138fe1cb43811e1346b539850be9`,
and the final full Runtime verification passed.

## Remaining operator acceptance

Physical D-pad/buttons, audible quality, FUNCTION exit, and a second launch
from the frontend remain operator checks. The profile remains an optional N64
route; `retroarch:parallel_n64` is still the default.
