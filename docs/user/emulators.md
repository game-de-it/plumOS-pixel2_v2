# Systems and emulators

[日本語](emulators.ja.md)

plumOS exposes only launch routes that are packaged for Pixel2. The system list
depends on the folders and compatible content present under `roms/`.

## Runtime families

| Family | Typical use |
| --- | --- |
| RetroArch | Main multi-system runtime and most libretro cores |
| PicoArch | Lightweight libretro runtime for selected systems |
| DraStic | Nintendo DS standalone emulator |
| PPSSPP | PSP standalone emulator |
| PCSX-ReARMed | PlayStation standalone option |
| Mupen64Plus | Nintendo 64 standalone option |
| OpenBOR | OpenBOR game packages |
| PICO-8 | User-supplied official ARM64 runtime and cartridges |
| Pyxel | `.pyxapp` games and the bundled Pyxel environment |
| PortMaster | Supported native ports and PortMaster packages |

Saturn is intentionally not part of the Pixel2 release scope because RK3326
performance is not suitable for the required experience. Other high-end
systems may vary by game and are not guaranteed to run at full speed.

## Choose a core

Highlight a system or game and press SELECT to open Core Settings. LEFT and
RIGHT change the profile, and B returns. Leave the default unless a game needs
a different compatibility or performance profile.

GB, GBC, and GBA expose both `RA: mgba` and `RA: mgba_modern`. The first is
the release-proven legacy build; the second is the newer pinned build with
Color Correction and Interframe Blending options. Defaults remain unchanged so
the newer core cannot silently change performance. Battery saves are shared,
but mGBA Modern keeps savestates separate because states are not guaranteed to
be compatible between core versions.

Nintendo 64 keeps `RA: parallel_n64` as the release-proven default. Select
`SA: mupen64plus` in Core Settings to try the standalone Rice GLES2 route for
a game that benefits from different compatibility. Pixel2 has no analog stick,
so its physical D-pad normally acts as the N64 analog stick in this standalone
runtime. Short-press FUNCTION to switch to the N64 D-pad and short-press it
again to return to analog mode. Hold FUNCTION for 1.5 seconds to exit normally
and return to the frontend.

FUNCTION opens an emulator menu where that runtime supports it. RetroArch uses
START + SELECT for its normal exit path. Save and hotkey details are in
[Save data, states, and screenshots](save-data.md).

## PICO-8

PICO-8 is commercial software and is not bundled with plumOS. Copy the
`pico8_64` executable and `pico8.dat` from a legitimately obtained Raspberry
Pi package to:

```text
PLUMOS_USER/roms/pico-8/aarch64/
```

The launcher validates the ELF machine type instead of trusting the filename
and starts only an AArch64 executable. PICO-8 0.2.7 or later is recommended for
current Splore cartridges. Older 0.2.6b builds may display the catalog but
reject newer cartridges as a future version.

Starting with v0.1.2, a Pixel2-specific adapter routes Splore's HTTP-to-HTTPS
downloads through plumOS-managed curl and CA certificates. Downloaded carts,
settings, and cdata remain mutable user state and are not removed by Runtime
updates. Fake-08 and Retro8 remain available as alternate cores.

## BIOS and content

Some systems require BIOS or firmware in `PLUMOS_USER/bios`. plumOS does not
provide copyrighted game BIOS files. A system may appear in the frontend but
fail to launch when its required BIOS, paired track files, or correct ROM-set
revision is missing.

Arcade cores are particularly sensitive to ROM-set revisions. Keep the parent
and required BIOS archives together and do not rename files inside the zip.
