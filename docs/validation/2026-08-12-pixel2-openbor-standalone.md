# Pixel2 OpenBOR standalone build

Date: 2026-08-12

Scope:

- Pixel2-specific OpenBOR source build
- standalone component manifest/checksum integration
- Pixel2 input-map defaults for OpenBOR
- ROM-set route validation

Implementation:

- Added `patches/openbor/openbor-v6391-pixel2-sdl.patch`.
- Extended `scripts/build-standalone-pixel2.sh` with `--filter openbor`.
- Built OpenBOR from `https://github.com/DCurrent/openbor.git` at
  `494708eb34e71d1afda237873907701c4ec3a569`.
- Kept sibling-device renderer and identity assumptions out of the Pixel2 path.

Host commands:

```sh
./scripts/docker-build.sh standalone --filter openbor
./scripts/docker-build.sh app-layer --strict
./scripts/validate-romset-routes.py \
  --app-root output/app-layer/pixel2/plumos \
  --rom-root /Volumes/public-1/02/motoki/emu/ROM/rom2 \
  --markdown docs/validation/2026-08-12-pixel2-romset-routes.md \
  --json output/validation/pixel2-romset-routes.json
```

Results:

```text
created: /work/output/standalone/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=1 output=/work/output/app-layer/pixel2/plumos
{"enabled_systems": 87, "route_ok": 26, "route_pending_binary": 3, "systems_with_rom": 29, "systems_without_rom": 58, "unmapped_rom_dirs": ["01", "3ds", "ATARI", "_etc", "ports", "pyxel"]}
```

OpenBOR route:

```text
system=openbor
sample=openbor/Crisis Evil 1.pak
profile=standalone:openbor
status=ok
detail=openbor
```

Remaining ROM-set standalone pending binaries:

```text
psp     standalone:ppsspp
nds     standalone:drastic
saturn  standalone:yabasanshiro
```

Device validation:

- Not performed in this step.
- Required next checks: launch `Crisis Evil 1.pak` from FE, confirm SDL/KMSDRM
  display orientation/scaling, Pixel2 physical controls, audio through
  `plumos_output`, and clean exit back to FE.
