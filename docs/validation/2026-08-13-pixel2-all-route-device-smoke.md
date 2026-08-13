# Pixel2 all-route device startup validation

Date: 2026-08-13  
Final device app-layer: `0.1.0-dev-00434c1`  
ROM source: `/Volumes/public-1/02/motoki/emu/ROM/rom2`

## Scope and acceptance rule

This run checks route resolution and early runtime startup. For each enabled
system backed by the supplied ROM set, representative content was copied to a
temporary hidden directory, launched through the same text-UI plan used by the
frontend, and required to keep the expected emulator process alive for at least
three seconds. The process was then stopped and the frontend state restored.

This is not a physical acceptance result for controls, orientation, aspect,
frame pacing, audio, volume, save/state, Function-menu operation, or clean exit.
Those checks remain a separate user-visible hardware gate.

## Results

| Gate | Result |
| --- | ---: |
| enabled systems / catalog systems | 87 / 97 |
| static launch profiles resolved | 183 / 183 |
| libretro catalog parallel build | 111 passed / 0 failed |
| systems with supplied representative content | 29 |
| device profile startups | 97 passed / 0 failed |
| RetroArch startups | 63 / 63 |
| PicoArch startups | 29 / 29 |
| standalone startups | 4 / 4 |
| Pyxel startups | 1 / 1 |
| systems without matching supplied content | 58 |
| implementation-audit release blockers | 0 |
| final device app-layer checksum | pass |
| frontend renderer-ready after validation | pass |

The 97-profile run used the equivalent routed runtime `0.1.0-dev-d93d4b6`; the
subsequent `00434c1` update only removed the unexposed standalone Mupen
placeholder. The updated standalone dispatcher was rechecked with OpenBOR on
`00434c1` and passed the same three-second startup gate. The detailed
per-profile result is generated at
`output/validation/pixel2-device-romset-smoke-final.json`; the host route report
is [2026-08-13-pixel2-romset-routes.md](2026-08-13-pixel2-romset-routes.md).

Profile-specific samples avoid false failures caused by incompatible arcade
ROM revisions or content formats. In particular, the run uses `ddragon.zip` for
MAME 2000/2003, `1942a.zip` for FBNeo/FBA2012, `varthj.zip` for MBA Mini,
`Ys2-p.dsk` for BlueMSX, `XGR1Trial.rom` for fMSX, `chroQW.img` for
DuckSwanStation, `SUPERMARIO64.Z64` for Parallel N64, and
`LastEmulator.pyxapp` for Pyxel.

## Explicit exclusions

- Saturn is disabled as `unsupported_performance_rk3326`; its routes and cores
  are not part of the 87-system/111-core Pixel2 surface.
- Mupen64Plus-Next was removed from the Pixel2 catalog and N64 frontend route.
  Its pinned AArch64 build segfaulted on the stock kernel with dynarec disabled,
  cached and pure interpreters, and GLideN64, Angrylion, and ParaLLEl paths.
  Nintendo 64 remains available through the passing Parallel N64 route.

## Content gaps

The supplied set has no matching representative content for these 58 enabled
systems, so an actual game startup cannot be claimed for them:

`fds`, `sega32x`, `segacd`, `supergrafx`, `neogeo`, `neogeocd`, `ngp`,
`wonderswancolor`, `lynx`, `virtualboy`, `cps1`, `cps2`, `cps3`, `easyrpg`,
`scummvm`, `pc98`, `x68000`, `tic80`, `vectrex`, `supervision`, `odyssey2`,
`gameandwatch`, `pokemini`, `doom`, `3do`, `amiga`, `atarist`, `c64`,
`cannonball`, `cavestory`, `chailove`, `channelf`, `colecovision`, `cpc`,
`dinothawr`, `intellivision`, `j2me`, `jaguar`, `lowresnx`, `lutro`, `microw8`,
`music`, `pcfx`, `quake`, `sg1000`, `sharpx1`, `thomson`, `ti83`, `uzebox`,
`vic20`, `vmu`, `wolf3d`, `zx81`, `zxspectrum`, `arduboy`, `megaduck`,
`puzzlescript`, `superbroswar`.

Static route validation still passes for all profiles in those systems, but a
real content result remains open until compatible ROM/data is supplied.

## Deployment evidence

The signed Runtime package used for the final device surface is
`plumos-pixel2-runtime-0.1.0-dev-00434c1.tar.gz`, SHA-256
`cfaa80e0e994b02228499a06b8f1f8fa3d19cb4d226dd9443b0d464619cd5dfd`.
The preceding signed `d93d4b6` update performed the three managed deletions for
the retired Mupen core metadata/binary; `00434c1` removed its standalone
placeholder. After the final reboot, the device reported renderer-ready, no
Mupen route/core/standalone entry remained, and `sha256sum -c checksums.sha256`
passed.
