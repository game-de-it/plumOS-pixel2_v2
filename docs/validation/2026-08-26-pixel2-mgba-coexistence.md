# Pixel2 legacy and modern mGBA coexistence

Date: 2026-08-26

## Contract

- Preserve the release-proven `mgba_libretro.so` recipe at
  `4f70b313fcf82b043bee232dd5af231a7755e1d8`.
- Add upstream HEAD observed on 2026-08-26 as the reproducibly pinned
  `mgba_modern_libretro.so` at
  `e31759b24e7a4e3899285ff720d7b573ac328ae7`.
- Expose both RetroArch profiles for GB, GBC, and GBA without changing their
  defaults.
- Share battery saves by system, but force mGBA Modern savestates out of the
  content directory into `states/<system>/mgba-modern`.

## Host evidence

```text
./scripts/docker-build.sh cores --filter mgba_modern --jobs 4 --fail-on-error 1
built: 1
failed: 0
skipped: 109

592748348f0c9d81ef69619673e52ab214fd257063f82d9bf65bcd86dbfccb27  mgba_modern_libretro.so
ELF 64-bit LSB shared object, ARM aarch64
```

The generated component manifest records ID `mgba_modern`, class `A`, the
exact upstream commit, and `cores/mgba_modern_libretro.so`. All component
checksums pass. Binary string inspection confirms:

```text
mGBA Modern
mgba_color_correction
mgba_interframe_blending
```

The generated core-info file reports `mGBA Modern` and `0.11-dev e31759b`.
`tests/test-app-layer-scripts.sh`, the Pixel2 emulator-menu contract test,
shell syntax checks, JSON validation, and catalog counts (42 standard / 110
full) pass.

## Remaining physical acceptance

On Pixel2, compare the same ROM and RetroArch settings between the legacy and
modern profiles. Confirm controls, audio, frame pacing, Color Correction,
Interframe Blending, battery-save sharing, and isolated savestate behavior.
