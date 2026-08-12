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

## Still requires real-device validation

- Pyxel launch from FE
- display orientation/aspect on the Pixel2 LCD
- controls and exit hotkey
- audio stability through `plumos_output`
- `Pyxel Setup` behavior with a project-specific `/roms/pyxel/requirements.txt`
