# Pixel2 PicoArch and standalone app-layer integration

Date: 2026-08-12

Scope:

- Pixel2 PicoArch build target and component packaging
- Pixel2 standalone launcher component packaging
- app-layer assembly with frontend, RetroArch, libretro cores, PicoArch,
  standalone launcher, and audio-router components
- frontend launch-profile runtime verification

Host commands:

```sh
./scripts/docker-build.sh picoarch
./scripts/docker-build.sh standalone
./scripts/docker-build.sh app-layer --strict
```

Results:

```text
created: /work/output/picoarch/pixel2/plumos
created: /work/output/standalone/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=1 output=/work/output/app-layer/pixel2/plumos
```

Component manifest summary:

```json
{
  "picoarch": {
    "name": "plumOS Pixel2 PicoArch",
    "component": "picoarch",
    "device": "pixel2",
    "architecture": "aarch64"
  },
  "standalone": {
    "name": "plumOS Pixel2 standalone launcher",
    "component": "standalone",
    "device": "pixel2",
    "architecture": "aarch64",
    "status": "launcher-only"
  },
  "app_layer_components": [
    "frontend",
    "retroarch",
    "libretro-cores",
    "picoarch",
    "standalone",
    "audio-router"
  ]
}
```

Verification notes:

- `picoarch:*` launch profiles are verified against the packaged PicoArch
  launcher and either `picoarch/cores` or the shared libretro `cores`
  directory.
- `standalone:*` launch profiles are verified against the packaged standalone
  launcher and the standalone component manifest.
- Individual standalone emulator binaries remain pending. The launcher logs a
  clear `missing standalone emulator binary` error instead of silently failing.
- PicoArch and standalone launchers prepare the Pixel2 `plumos_output` ALSA
  route before starting runtime code.

Device validation:

- Not performed in this step. Real-device ROM validation will use the ROM set
  under `/Volumes/public-1/02/motoki/emu/ROM/rom2` after the SD image/app-layer
  is deployed.
