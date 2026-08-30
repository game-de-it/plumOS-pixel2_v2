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

The first `b97935a` device trial retained the reported break-up on the first
cursor movement. A second movement did not restore the LCD, which rejects both
the asynchronous-commit-only hypothesis and a single defective framebuffer.
The captured settled buffer remained coherent.

The follow-up repair aligns menu ownership with the already stable three-page
game path. A new surface presents page zero as its initial scanout page and
advances the producer to page one before the first render. Returning from the
menu presents the main surface's last completed page instead of its next
writable page. The menu now has three pages, leaving a full extra frame before
an old scanout buffer can be reused by the producer.

Pending deployment and physical cursor-motion confirmation. Acceptance must
confirm all of the following on the same Varth route:

1. game remains upright and at the correct aspect;
2. FPS remains approximately 60;
3. Quick Menu remains upright;
4. repeated up/down cursor movement no longer breaks up the LCD.

The `fd69baf` device trial also retained the break-up, rejecting page ownership
and the number of menu buffers as the cause. The remaining transition has two
different geometries: RGUI's first frame is produced against the running
vertical core viewport, then `viewport_info` begins reporting the active menu
viewport. The DRM menu surface retained the first frame's width, height, and
pitch while reading the later frame, so the first cursor redraw was the first
visible mismatched read.

Patch 030 makes the display UI independent of content geometry. Every plain-DRM
RGUI surface uses the Pixel2's fixed landscape 4:3 viewport, while the game
surface continues to honor Core Provided. If RGUI changes its input width,
height, or pitch during the game-to-menu transition, the menu surface is freed
and recreated before that frame is read. This applies to every rotated
RetroArch core, rather than naming an arcade core or title.

The operator accepted the fixed 4:3 aspect and confirmed that repeated cursor
movement no longer corrupts the LCD on `3a1c045`. A single blink could remain
either on menu entry or the first cursor movement. This is the expected visible
boundary where the geometry guard replaces the provisional first surface.

Patch 031 removes the provisional geometry. While the menu is active but its
surface has not yet been allocated, `viewport_info` now reports the same fixed
4:3 viewport that the eventual menu surface will use. RGUI therefore renders
its first frame at the final dimensions, while the width/height/pitch guard is
retained as a safety check for unexpected later changes.

Ozone and XMB use the Pixel2 GLES presentation path rather than this plain-DRM
RGUI surface. After the RGUI transition is accepted, both graphical drivers
must be launched with temporary overrides against the same vertical Varth
content. Validate orientation, aspect, repeated cursor movement, and return to
the running game without replacing the user's saved menu driver or config.

The first temporary Ozone trial reproduced a 90-degree menu rotation in the
final 480x640 DRM scanout. Graphical UI drawing still used `gl->mvp`, whose
rotation had been changed by the vertical core's `SET_ROTATION` request. Patch
032 temporarily selects the Pixel2 display's fixed 90-degree projection for the
entire graphical menu frame, including private cursor and icon matrices, then
restores the content projection. Horizontal content already using 90 degrees is
left unchanged. Ozone and XMB both require device acceptance after deployment.
