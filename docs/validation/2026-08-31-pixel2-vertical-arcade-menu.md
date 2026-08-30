# Pixel2 vertical arcade RetroArch menu validation

Date: 2026-08-31

## Reported route

- Frontend system: Arcade
- Core: `km_mame2003_xtreme_libretro.so`
- Content: `varthj.zip`
- Menu driver: RGUI

## Diagnosis

The game surface, on-screen FPS text, and the settled Quick Menu scanout were
captured from DRM. The game was upright at the core-provided aspect and the
operator confirmed 60 fps. The settled menu buffer was also coherent and
upright, while the LCD broke up only at the instant the cursor moved. Runtime
and kernel logs contained no DRM atomic-commit failure.

This isolates the remaining fault from menu rendering and geometry. The game
and menu share the same KMS plane, but patch 028 had changed every page flip to
an asynchronous atomic commit. The menu uses a two-page surface and redraws
from the frontend thread, so cursor redraw could race physical scanout even
though a later dump of the settled framebuffer was valid.

## Repair

`028-pixel2-drm-nonblocking-page-flip.patch` now selects commit policy by
surface role:

- game surface: event-driven nonblocking atomic commit, preserving 60 Hz;
- menu surface (layer 2): event-driven synchronous atomic commit, preventing
  a menu page from racing physical scanout.

The change does not modify the user RetroArch configuration, core options,
ROMs, saves, or the game-surface rotation/aspect path.

## Host verification

- `tests/test-app-layer-scripts.sh`: pass
- patched RetroArch AArch64 build: pass
- all configured RGUI, MaterialUI, Ozone, and XMB symbols: present

## Device acceptance

Pending deployment and physical cursor-motion confirmation. Acceptance must
confirm all of the following on the same Varth route:

1. game remains upright and at the correct aspect;
2. FPS remains approximately 60;
3. Quick Menu remains upright;
4. repeated up/down cursor movement no longer breaks up the LCD.
