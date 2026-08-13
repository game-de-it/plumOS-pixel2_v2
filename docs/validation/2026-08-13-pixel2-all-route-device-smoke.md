# Pixel2 all-route device startup validation

Date: 2026-08-13

Final device app-layer: `0.1.0-dev-9410f72`

ROM source: `/Volumes/public-1/02/motoki/emu/ROM/rom2`

## Scope and acceptance rule

This run checks route resolution and early runtime startup. For every enabled
system for which the supplied ROM set contains compatible content, a
representative game/data set was copied to a temporary hidden directory and
launched through the same text-UI plan used by the frontend. The expected
emulator process had to remain alive for at least three seconds; targeted
directory-layout and dependency fixes were rechecked for five seconds. Test
content was removed and the frontend was restored after each run.

This is not a physical acceptance result for controls, orientation, aspect,
frame pacing, audio, volume, save/state, Function-menu operation, or clean exit.
Those checks are intentionally left for the subsequent user-visible hardware
gate.

## Results

| Gate | Result |
| --- | ---: |
| enabled systems / catalog systems | 87 / 97 |
| static launch profiles resolved | 181 / 181 |
| libretro catalog parallel build | 109 passed / 0 failed |
| systems with supplied compatible content | 74 |
| device profile startups | 164 passed / 1 BIOS-blocked |
| RetroArch startups | 123 passed / 1 BIOS-blocked |
| PicoArch startups | 36 / 36 |
| standalone startups | 4 / 4 |
| Pyxel startups | 1 / 1 |
| systems without matching supplied content | 13 |
| implementation-audit release blockers | 0 |
| final device app-layer checksum | 3490 passed / 0 failed |
| frontend renderer-ready after validation | pass |

The one blocked profile is Channel F through `freechaf`. The ROM set contains
compatible game content, but does not contain its three required firmware files:
`sl31253.bin`, `sl31254.bin`, and `sl90025.bin`. It is therefore not counted as
an emulator failure or a successful startup. No copyrighted firmware was
downloaded or substituted.

The per-run machine-readable evidence is stored under `output/validation/`:

- `pixel2-device-romset-smoke-final.json`: 29 systems, 97/97 profiles;
- `pixel2-device-romset-smoke-etc.json` plus targeted retry reports: 39 systems,
  effectively 56/57 profiles, with only Channel F still blocked;
- `pixel2-device-romset-smoke-shared-dirs.json` and
  `pixel2-device-romset-smoke-cps-final.json`: Lynx, Jaguar, CPS1, CPS2, and
  CPS3, effectively 10/10 current profiles;
- `pixel2-device-romset-smoke-scummvm-final.json`: ScummVM 1/1.

The host inventory and every resolved profile are recorded in
[2026-08-13-pixel2-romset-routes.md](2026-08-13-pixel2-romset-routes.md).

## Route corrections made during validation

- ROM discovery now recognizes the archival `_etc` tree and shared
  `ATARI/Lynx`, `ATARI/Jaguar`, and `mame` directories without changing the
  user's ROM set.
- Directory-backed EasyRPG, Cannonball, Cave Story/NXEngine, Dinothawr, PC-FX,
  Frodo, and ScummVM content is staged and launched with the required tree
  layout rather than as an isolated file.
- RetroArch loads core-private runtime libraries from
  `/mnt/plumos/lib/libretro`, which supplies EasyRPG's ICU dependency.
- Neo Geo CD uses the complete WAV/CUE set in `_etc/neogeocd`; the available
  MP3-track CUE is not supported by the NeoCD core.
- ScummVM receives a disposable `.scummvm` target marker alongside the complete
  game directory. The source ROM tree is not modified.
- The CPS1/CPS2-specific FBA2012 cores were removed after multiple compatible
  revisions failed on Pixel2. CPS1 and CPS2 remain exposed through the passing
  FBNeo and general FBA2012 routes. CPS3 retains its passing dedicated route.

Saturn is disabled as `unsupported_performance_rk3326`; its routes and two
cores are deliberately outside the 87-system/109-core Pixel2 surface.
Mupen64Plus-Next is also absent after all tested interpreter/video combinations
segfaulted; Nintendo 64 remains available through the passing Parallel N64
route.

## Remaining content gaps

The supplied set has no compatible representative content for these 13 enabled
systems, so an actual game startup cannot be claimed for them:

`ngp`, `wonderswancolor`, `x68000`, `tic80`, `vectrex`, `sg1000`, `sharpx1`,
`wolf3d`, `zx81`, `arduboy`, `megaduck`, `puzzlescript`, `superbroswar`.

All 181 static launch profiles, including these systems, resolve to present
runtimes and cores. Actual startup remains open until compatible content is
supplied.

## BIOS inventory

The ROM set supplied 486 firmware files, merged without deleting or replacing
user data. Of 225 required firmware declarations, five required files remain
absent: the three Channel F files above, `ecwolf.pk3`, and `kick34005.CDTV`.
Only Channel F blocks a supplied representative game in this validation;
Wolfenstein 3D has no supplied content, and the tested Amiga A500 route does not
require the CDTV ROM.

## Build and deployment evidence

The current catalog was rebuilt with:

```text
./scripts/docker-build.sh core-catalog --filter all --concurrency 4
catalog_result pass=109 fail=0
```

The signed Runtime package on the final device is
`plumos-pixel2-runtime-0.1.0-dev-9410f72.tar.gz`, SHA-256
`67cec650e75522d0cab8f0ddcaa5c4e3d44334bfd6676100717d33c3de000e73`.
After reboot, the device reported the expected version and renderer-ready state;
`sha256sum -c checksums.sha256` passed 3490 managed paths with zero failures.
