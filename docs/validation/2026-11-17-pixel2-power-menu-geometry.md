# Pixel2 power-menu DRM geometry restoration

[日本語](2026-11-17-pixel2-power-menu-geometry.ja.md)

Runtime: public v0.1.4 with scoped live patch `9dd4d9c`.
Device: GKD Pixel2 at 192.168.10.107.

## Reproduction and cause

Gambatte / Zelda DX was already running through the normal FE route with RGUI.
The game plane (81) had destination rectangle `0,54,480,533`. Opening the
production power menu and cancelling with the physical B input code 304 changed
it to `0,0,480,533`. Opening/closing RetroArch's menu restored `y=54`.
The portrait scanout is rotated on Pixel2, so this displacement appears horizontal.

`drmModeGetPlane()` returned zero in its legacy x/y fields despite the actual
atomic `CRTC_Y=54`. The old handoff also inferred destination and source sizes
from framebuffer dimensions, which cannot preserve arbitrary scaling/cropping.

The hardware-key service now snapshots all eight CRTC/SRC rectangle properties
before disabling each plane and restores those same values before SIGCONT.
Source coordinates retain 16.16 precision and signed destination positions retain
their bit representation. Incomplete geometry is rejected before disabling a plane.

## Validation

- AArch64 build passed with `-Wall -Wextra`.
- `tests/test-pixel2-plane-geometry.c` passed: unordered properties, unrelated
  properties, negative destination, fractional source offset, scaled rectangle,
  missing properties, and zero dimensions.
- `tests/test-pixel2-power-menu-sleep.sh` passed.
- Three real power-menu/B-cancel cycles preserved both planes' full geometry.
- RetroArch PID 12019 remained alive; ALSA was RUNNING and hw_ptr advanced from
  539024 to 587320 over approximately one second.
- Frontend component checksums and production `verify-runtime` passed.
- RetroArch cfg SHA-256 was unchanged:
  `67f9219b577c6a4488cadf1700bdc729994ddf753f3d2e02cce8a6c6c1d7c0b6`.

The live deployment changed the hardware-key binary and only its associated
metadata entries. Existing metadata was read from the device, backed up under
`/mnt/plumos/backups/power-geometry-1794925367`, and updated with atomic renames.
The root/component/license manifests record the scoped live patch rather than
claiming an entire new build. The service was restarted without stopping the game.
Binary SHA-256:
`9c6d631a5d35c64fc66ae581685366a523bac910bb49072ce7cec2bea3149077`.

Physical LCD acceptance after this fix remains for the operator. No release was
published and no SD image was rebuilt.
