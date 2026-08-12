# Pixel2 Saturn default route

Date: 2026-08-12

Scope:

- Avoid defaulting Saturn to an unimplemented standalone binary.
- Keep the standalone profile visible for future validation, but make the FE
  default use the already packaged libretro runtime.

Change:

```text
saturn default_launch_profile:
  from standalone:yabasanshiro
  to   retroarch:yabasanshiro
```

Host commands:

```sh
./scripts/docker-build.sh frontend
./scripts/docker-build.sh app-layer --strict
./scripts/validate-romset-routes.py \
  --app-root output/app-layer/pixel2/plumos \
  --rom-root /Volumes/public-1/02/motoki/emu/ROM/rom2 \
  --markdown docs/validation/2026-08-12-pixel2-romset-routes.md \
  --json output/validation/pixel2-romset-routes.json
```

Result:

```text
frontend_component=result-ok output=/work/output/frontend/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=1 output=/work/output/app-layer/pixel2/plumos
{"enabled_systems": 87, "route_ok": 27, "route_pending_binary": 2, "systems_with_rom": 29, "systems_without_rom": 58, "unmapped_rom_dirs": ["01", "3ds", "ATARI", "_etc", "ports", "pyxel"]}
```

Remaining ROM-set pending binaries:

```text
psp  standalone:ppsspp
nds  standalone:drastic
```

Device validation:

- Not performed in this step.
- Saturn still needs real-device launch, orientation/scaling, audio, control,
  and exit checks.
