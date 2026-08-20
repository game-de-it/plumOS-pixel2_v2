# Pixel2 Pyxel runtime integration

## Goal

- Do not leave Pyxel as a hidden or disabled placeholder.
- Package the runtime first, then expose the FE route and Setup app.
- Make missing Pyxel runtime files fail app-layer verification and ROM route
  validation.

## Implemented

- `./scripts/docker-build.sh pyxel-runtime`
- Pixel2 Pyxel component under `output/pyxel-runtime/pixel2/plumos`
- bundled Python 3.11 wrapper: `bin/plumos-python-pixel2`
- Pyxel launcher: `bin/plumos-pyxel-pixel2-launch`
- mutable setup tool: `bin/plumos-pyxel-setup`
- Pyxel FE system enabled with default profile `pyxel:pixel2`
- Apps menu entry: `Pyxel Setup`
- app-layer manifest/checksum integration for component `pyxel`
- verifier and ROM route validator checks for the Pyxel launcher, bundled
  Python runtime, and packaged Pyxel module

## Host validation

```sh
bash -n scripts/build-pyxel-runtime-pixel2.sh scripts/build-app-layer.sh \
  scripts/docker-build.sh scripts/verify-app-layer.sh
sh -n package/pyxel-pixel2/plumos/bin/plumos-python-pixel2 \
  package/pyxel-pixel2/plumos/bin/plumos-pyxel-pixel2-launch \
  package/pyxel-pixel2/plumos/bin/plumos-pyxel-setup
python3 - <<'PY'
from pathlib import Path
compile(Path('scripts/validate-romset-routes.py').read_text(),
        'scripts/validate-romset-routes.py', 'exec')
PY
git diff --check
./scripts/docker-build.sh frontend
./scripts/docker-build.sh pyxel-runtime
./scripts/docker-build.sh app-layer
./scripts/verify-app-layer.sh output/app-layer/pixel2/plumos
docker run --rm --platform linux/arm64 \
  -v "$PWD/output/app-layer/pixel2/plumos:/mnt/plumos:ro" \
  -w /mnt/plumos plumos-pixel2-tools:dev \
  /mnt/plumos/bin/plumos-python-pixel2 \
  -c 'import sys, pyxel, pygame, numpy, PIL; print("python", sys.version.split()[0]); print("pyxel", pyxel.VERSION); print("pygame", pygame.version.ver); print("numpy", numpy.__version__); print("Pillow", PIL.__version__)'
./scripts/validate-romset-routes.py \
  --rom-root /Volumes/public-1/02/motoki/emu/ROM/rom2 \
  --markdown docs/validation/2026-08-13-pixel2-romset-routes.md \
  --json output/validation/pixel2-romset-routes.json
```

Observed route summary:

```json
{"enabled_systems": 88, "route_ok": 30, "route_pending_binary": 0, "systems_with_rom": 30, "systems_without_rom": 58, "unmapped_rom_dirs": ["01", "3ds", "ATARI", "_etc", "ports"]}
```

Representative Pyxel route:

```text
system=pyxel
sample=pyxel/LastEmulator.pyxapp
default_profile=pyxel:pixel2
status=ok
detail=pyxel
```

Observed runtime import:

```text
python 3.11.2
pyxel 2.9.3
pygame 2.6.1
numpy 2.4.6
Pillow 12.3.0
```

`verify-app-layer.sh` also runs an aarch64 fixture that imports Pyxel through
`plumos-python-pixel2`, scans a temporary `pyxel/test.pyxapp`, and verifies that
`plumos-text-ui launch pyxel pyxel/test.pyxapp --no-scan` produces
`launch_profile: pyxel:pixel2` with `can_execute: yes`.

## Real-device validation

On 2026-08-20 all FE Pyxel routes failed before creating a window. The device
log showed Mesa falling through `zink`, `kms_swrast`, and `swrast`, followed by
`Failed to create window: EGL not initialized`. The launcher was pointing SDL
at an absent `/mnt/plumos/lib/libEGL.so.1` and forced the internal GPU name
`panfrost`, while the stock KMS device and packaged Mesa DRI payload use the
`rockchip` DRM driver name.

The launcher now follows the physically validated Pixel2 PortMaster GLES
contract:

- SDL loads EGL/GLES from `apps/pyxel/lib`.
- the packaged Mesa mega-driver is exposed as `rockchip_dri.so` in volatile
  state;
- `MESA_LOADER_DRIVER_OVERRIDE` is cleared so Mesa selects from the KMS device.

With those settings, `LastEmulator.pyxapp` remained alive for the bounded
10-second SSH launch test and the prior `EGL not initialized` failure did not
recur. The first corrected launch also exposed `Failed to initialize audio
device`: the launcher used the generic `plumos_output` PCM despite the router
already providing `plumos_pyxel` for Pyxel's fixed 22.05 kHz mono SDL format.
Both `plumos_pyxel` and the direct plughw control test remained alive until the
6-second termination boundary without that error, so the launcher now selects
`plumos_pyxel` by default.

The native DRM scanout is 480x640 although the Pixel2 is held as a landscape
device. A bounded live test therefore combined the already accepted GLES
framebuffer presenter with the Pyxel shader-fit adapter. The capture confirmed
that Last Emulator's 720x480 (3:2) canvas is fitted to 640x427 with factor
0.888889 and centered with approximately 27 pixels above and below. The complete
title and menu remained inside the 640x480 logical display without stretching.
The Pyxel component now builds its own presenter from the shared plumOS source;
it does not depend on an installed PortMaster component at runtime. The default
launcher also hides the SDL hardware cursor before presentation.

The first formal deployment showed that this shader fit cannot be placed before
Pixel2's separate rotation presenter: that presenter had already allocated a
fixed 640x480 intermediate framebuffer, so the right side of a 720x480 title
could be lost before the final 480x640 scanout. The operator saw the resulting
left-aligned crop even though the shader-fit log reported the intended
640x427 rectangle.

The corrected design follows the A30 Last Emulator solution rather than stacking
the V90S/MF shader adapter in front of a rotated framebuffer. An OS-owned Python
shim observes the public `pyxel.init()` call and publishes the real logical
canvas before SDL creates its GL context. The Pixel2 presenter allocates that
source size, keeps the complete frame, computes
`min(640/source_width, 480/source_height)`, clears the unused area, and performs
aspect fit and 270-degree rotation in the final presentation step. The official
Pyxel package remains unchanged, and the same calculation handles square,
portrait, wide, smaller, and oversized canvases.

## Still requires real-device validation

- Pyxel launch from FE after the 2026-08-20 GLES correction
- operator-visible orientation/aspect confirmation on the Pixel2 LCD
- controls and exit hotkey
- audio stability through `plumos_pyxel` (Last Emulator separately opens pygame
  mixer before Pyxel and still needs title-specific double-audio-owner handling)
- physical A/B/X/Y through Pixel2's accepted `east-confirm` SDL mapping. The
  launcher shares PortMaster's controller GUID and `a:b1,b:b0,x:b2,y:b3`
  contract; generic SDL otherwise interprets physical A as Pyxel B and closes
  Last Emulator's title screen.
- repeated large `.pyxapp` launches without exhausting the 488 MiB `/tmp`.
  The Pixel2 shim removes only dead PID-owned Pyxel extraction directories
  before unpacking and removes its own controlled extraction on exit. The
  launcher also fixes `TMPDIR` to `/run/plumos/cache/pyxel/tmp`, rather than
  allowing a full generic `/tmp` to redirect extraction into persistent HOME.
- `Pyxel Setup` behavior with a project-specific `/roms/pyxel/requirements.txt`
