# Pixel2 RGUI integer scaling

[日本語](2026-11-17-pixel2-rgui-integer-scaling.ja.md)

## Cause and fix

The plain DRM driver scales content into viewport-sized dumb buffers. Its
apply-state callback only reconfigured the plane, leaving the old buffer size
when integer scaling changed without a change in core resolution. The viewport
calculation also applied the game's integer-scale setting to RGUI surfaces.

Patch 037 marks content geometry dirty and checks the viewport on the first
non-duplicate game frame after RGUI restores content settings. Changed rectangles
recreate the software-scaled buffers. Menu surfaces (layer 2), including the
initial synthetic viewport, ignore game integer scaling and retain the existing
fixed 4:3 display policy.

During testing, Fit Screen exposed a NULL main surface after RGUI's content-aspect
restoration. The initial candidate exited with rc139. The final patch guards
the menu-to-game plane restore while the next game frame recreates that surface.

Implementation commits: `1bd48ec`, `ed79dbc`, `7f01f94`.

## Tests and live deployment

- Pinned RetroArch source and all repository patches; AArch64 build passed.
- `tests/test-pixel2-drm-integer.py` compiles the actual patched functions with
  stub viewport helpers. It verifies menu isolation, deferred changes while
  RGUI is active, duplicate frames, the first real content frame, ON/OFF, and
  the NULL-surface menu exit.
- Reverse apply check confirms patch 037 matches the built source.
- Final device binary SHA-256:
  `0026abb0c466939cfd1e2ad6985d5b9b47483b94b53b15bbc70d8906f1a7aa22`.
- Scoped deployment updated only the binary and its current device metadata.
  The preceding power-menu patch and mutable data were retained. Device rollback
  copies are under `/mnt/plumos/backups/retroarch-integer-*`.

QuickNES / Akumajou Densetsu was launched through the Pixel2 RetroArch launcher.
Real evdev menu inputs and DRM property/framebuffer captures verified:

| Action | Physical plane rectangle (x,y,w,h) |
| --- | --- |
| Integer OFF | 0,6,480,627 |
| First return after Integer ON | 112,153,256,334 |
| RGUI with Integer ON and Fit Screen | 0,0,480,640 |
| Return from Fit Screen to game | 112,153,256,334 |
| Reopen RGUI | 0,0,480,640 |
| First return after Integer OFF | 0,6,480,627 |

Decoded captures show the full logical 640x480 RGUI and the changed content
dimensions. The integer-size selection itself remains the existing RetroArch
rotation/axis/aspect policy; this change repairs update timing and menu isolation.
The operator should confirm the physical LCD. After restoring Integer OFF, the
operator exited RetroArch normally (`rc=0`); the temporary watcher restored FE
as one process. Full production `verify-runtime` passed for the final binary.
The saved cfg was unchanged by deployment; normal RetroArch exit subsequently
saved the current menu settings. No release or SD image was generated.
