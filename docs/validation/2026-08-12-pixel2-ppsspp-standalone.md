# Pixel2 PPSSPP standalone build

Date: 2026-08-12

## Scope

- Build PPSSPP for Pixel2 from pinned upstream source.
- Package the binary, assets, launcher factory defaults, license, runtime
  libraries, component manifest, and checksums through the Pixel2 standalone and
  app-layer build system.
- Make the PSP frontend route resolve to `standalone:ppsspp`.
- Keep the app-layer ROM/BIOS-content safety gate active while allowing the
  PPSSPP shader asset `standalone/ppsspp/assets/shaders/smiley_16x16_rgba.bin`,
  which is not ROM or BIOS content.

## Build inputs

| item | value |
| --- | --- |
| upstream | `https://github.com/hrydgard/ppsspp.git` |
| ref | `v1.20.4` |
| commit | `fa50bb1976065c4f8b1b47af227d367fe9771555` |
| patch | `patches/ppsspp/ppsspp-1.20.4-pixel2-no-sdl2-ttf.patch` |
| target | Pixel2 AArch64, Cortex-A35, SDL2/GLES/EGL |

The Pixel2 patch follows the existing plumOS pinned-source approach: do not
import an opaque generated emulator tree. It skips the SDL2_ttf requirement for
the Pixel2 build and records the patch hash in PPSSPP's build manifest.

## Packaged config

Factory defaults are installed under:

```text
factory-defaults/standalone/ppsspp/PSP/SYSTEM/controls.ini
factory-defaults/standalone/ppsspp/PSP/SYSTEM/ppsspp.ini
```

The launcher seeds those files into the mutable PPSSPP config directory on first
launch. Important factory settings:

```text
CurrentDirectory = /mnt/plumos/state/standalone/ppsspp
ShowTouchControls = False
MacAddress =
DisplayAspectRatio = 1.000000
InternalScreenRotation = 0
```

## Host validation

```sh
./scripts/docker-build.sh standalone --filter ppsspp
./scripts/docker-build.sh standalone
./scripts/docker-build.sh app-layer
./scripts/validate-romset-routes.py \
  --rom-root /Volumes/public-1/02/motoki/emu/ROM/rom2 \
  --markdown docs/validation/2026-08-12-pixel2-romset-routes.md \
  --json output/validation/pixel2-romset-routes.json
bash -n scripts/build-standalone-pixel2.sh scripts/docker-build.sh scripts/verify-app-layer.sh
sh -n package/standalone-pixel2/plumos/bin/plumos-standalone-launch
git diff --check
./scripts/verify-app-layer.sh output/app-layer/pixel2/plumos
```

Results:

```text
standalone=result-ok output=/work/output/standalone/pixel2/plumos
app_layer_verify=result-ok root=/work/output/app-layer/pixel2/plumos
app_layer=result-ok strict=0 output=/work/output/app-layer/pixel2/plumos
{"enabled_systems": 87, "route_ok": 29, "route_pending_binary": 0, "systems_with_rom": 29, "systems_without_rom": 58, "unmapped_rom_dirs": ["01", "3ds", "ATARI", "_etc", "ports", "pyxel"]}
app_layer_verify=result-ok root=output/app-layer/pixel2/plumos
```

PPSSPP binary:

```text
output/app-layer/pixel2/plumos/standalone/ppsspp/bin/PPSSPPSDL:
  ELF 64-bit LSB pie executable, ARM aarch64, stripped
sha256:
  2ef9ec741b2a0ac546641bd0a39b53e698b5b265fce9014f32ab944c8a2ae010
needed libraries:
  libSDL2-2.0.so.0
  libGLESv2.so.2
  libEGL.so.1
  libz.so.1
  libstdc++.so.6
  libm.so.6
  libgcc_s.so.1
  libc.so.6
  ld-linux-aarch64.so.1
```

The dependency gate rejected unintended desktop/Vulkan dependencies during the
build. The inspected binary has no `libX11`, `libXext`, or Vulkan dependency.

## Route validation

The PSP representative ROM now resolves through the frontend route table:

```text
psp/Star Soldier (Japan)/Star Soldier (Japan).iso
  default profile: standalone:ppsspp
  route status: ok
  route detail: ppsspp
```

After this integration, all 29 systems with representative ROMs in the local ROM
set have host-valid launch routes, and `route_pending_binary` is `0`.

## Real-device validation still required

Not performed in this step. On physical Pixel2, validate:

1. PPSSPP starts from FE using the PSP ROM list.
2. LCD orientation and aspect are correct.
3. D-pad, ABXY, START/SELECT, shoulders, and emulator menu/exit hotkeys match
   the Pixel2 physical button contract.
4. Audio goes through `plumos_output`, volume keys affect real output, and no
   audio dropouts occur.
5. PPSSPP exits cleanly and FE re-acquires display/input.
