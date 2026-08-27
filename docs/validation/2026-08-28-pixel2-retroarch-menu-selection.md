# Pixel2 RetroArch Menu Selection Validation

Date: 2026-08-28  
Implementation source: `6581c54`  
Device Runtime: `0.1.2-dev-6581c54`

## Problem and cause

The contentless RetroArch app respected the user's `menu_driver`, but the game
launcher always selected plain DRM for software-rendered cores. Graphical menu
drivers require the GLES route, so a saved Ozone, XMB, or GLUI choice could not
be presented consistently while a game was running.

Ozone also copied a private matrix for its textured selection cursor after a
non-rotating viewport request. Text used the later rotated matrix, leaving the
cursor in portrait coordinates and producing a tall cyan rectangle on the
right side of the otherwise-correct landscape menu.

## Fix

The game launcher now reads the merged mutable `retroarch.cfg`. RGUI retains
the low-overhead plain DRM route; an explicit GLUI, Ozone, or XMB selection uses
GLES with `PLUMOS_GL_MENU_ROTATION=display`. It does not append `menu_driver` or
`user_language`, so user choices remain authoritative.

The Pixel2 GL backend now applies the fixed menu display rotation before Ozone,
XMB, or MaterialUI copies a private cursor/icon matrix.

## Verification

- Menu-selection fixture: PASS for RGUI, Ozone, XMB, GLUI, hardware GL RGUI,
  and hardware GL Ozone.
- Localization/menu contract and full app-layer script suite: PASS.
- Parallel formal frontend and RetroArch builds: PASS.
- Strict app-layer verification and license audit: PASS.
- Signed Runtime package SHA-256:
  `ab4ac4e4670d985302908602b569320fdaf7f0352afaee161204dc59ed1cb44e`.
- Update result: `runtime_healthy`; cold reboot retained source `6581c54` and
  the user's unchanged Ozone config SHA-256
  `1e9027253eac43c24203b5987dbab587decf641eb3fa403e88de9b569dc52960`.
- Formal Ozone scanout was upright at 640x480 with a horizontal selection
  cursor. A normal QuickNES launch selected `video_driver=gl`, rotation 1, and
  produced a non-black, upright 640x480 game scanout.

Evidence:

- `output/live/2026-08-28-pixel2-retroarch-menus/formal/ra-formal-ozone-logical.png`
- `output/live/2026-08-28-pixel2-retroarch-menus/formal/ra-formal-game-logical.png`
- `output/live/2026-08-28-pixel2-retroarch-menus/ra-xmb-matrix-fix-logical.png`
- `output/live/2026-08-28-pixel2-retroarch-menus/ra-glui-matrix-fix-logical.png`

RetroArch's own contentless-app screenshot can remain black because it captures
the empty core framebuffer before the graphical menu is composed. DRM scanout
capture verifies the image actually sent to the LCD.
