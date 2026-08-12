# Pixel2 DraStic standalone integration

Date: 2026-08-12

Scope:

- Nintendo DS `standalone:drastic` FE route
- steward-fu/nds Pixel2 integration library build
- closed armhf DraStic runtime packaging
- armhf ALSA `plumos_output` plugin
- app-layer manifest/checksum gate

Host commands:

```sh
./scripts/docker-build.sh standalone
./scripts/docker-build.sh audio-router
./scripts/docker-build.sh frontend
./scripts/docker-build.sh app-layer
./scripts/validate-romset-routes.py \
  --rom-root /Volumes/public-1/02/motoki/emu/ROM/rom2 \
  --markdown docs/validation/2026-08-12-pixel2-romset-routes.md \
  --json output/validation/pixel2-romset-routes.json
./scripts/docker-build.sh sd-image
```

Result:

```text
created: /work/output/standalone/pixel2/plumos
audio_router=result-ok output=/work/output/audio-router/pixel2/plumos
plugin_armhf=/work/output/audio-router/pixel2/plumos/lib/alsa-lib-armhf/libasound_module_pcm_plumos_hotplug.so
frontend_component=result-ok output=/work/output/frontend/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=0 output=/work/output/app-layer/pixel2/plumos
{"enabled_systems": 87, "systems_with_rom": 29, "route_ok": 28, "route_pending_binary": 1, "systems_without_rom": 58, "unmapped_rom_dirs": ["01", "3ds", "ATARI", "_etc", "ports", "pyxel"]}
system_rootfs=result-ok image=/tmp/plumos-pixel2-verify.68bAz1/SYSTEM
app_layer_verify=result-ok root=/tmp/plumos-pixel2-verify.68bAz1/app-layer
sd_image=result-ok image=/work/output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img
created: /work/output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img
```

Notes:

- Nintendo DS now resolves to `standalone:drastic`.
- DraStic BIOS files are excluded from the app-layer and must be user-provided
  under `/mnt/plumos-user/bios/drastic`, `/mnt/plumos-user/bios/nds`, or
  `/mnt/plumos-user/bios`.
- The app-layer contains both OpenBOR and DraStic when `standalone` is built
  without a filter. A filtered standalone build is for local iteration only.
- The remaining ROM-set pending route is PSP `standalone:ppsspp`; it should be
  ported from the MF/V90S pinned-source PPSSPP flow as a separate work unit.
- Real-device DraStic launch, display orientation, controls, audio, save
  persistence, and FE return are not validated in this host-only step.
