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

Ozone was accepted on `60f256a`. XMB was upright, but its labels and entries
were clipped at logical x=480 inside the 640x480 scanout. The graphical menu
frame dimensions had been swapped again after the fixed projection was
selected, so XMB laid out a 480-pixel-wide UI. Patch 033 derives a stable
landscape width and height from the GL panel dimensions and passes 640x480 to
all graphical menus, independent of core geometry. XMB requires another device
trial; Ozone must be checked for regression afterwards.

The `af7567e` device trial remained clipped. Patch 033 corrected the dimensions
used by `xmb_frame()`, which fixed the final draw and clip surface, but the
earlier `xmb_render()` phase still received the unconditional width/height swap
from patch 020. XMB therefore cached its ticker widths and item geometry for a
480-pixel-wide portrait layout, then drew that layout into a valid 640x480
frame. Patch 034 normalizes both render-phase dimensions to the panel long and
short sides. This makes `xmb_render()` and `xmb_frame()` consistently receive
640x480 without changing the content projection or saved user configuration.

The `89de0dc` device trial still showed a hard vertical boundary. Increasing
the XMB scale factor exposed more characters only because it made each glyph
smaller; it did not move the boundary. The remaining fault was therefore not
XMB's ticker or thumbnail reservation. Patch 019 normalized font vertices with
the swapped physical dimensions even after the menu viewport became a stable
logical 640x480. Text vertices beyond logical x=480 were outside clip space,
while backgrounds and icons used the correct 640-pixel coordinate system.
Patch 035 keeps font normalization at 640x480 and leaves the existing menu MVP
to rotate all primitives into the physical 480x640 scanout. Validate complete
XMB labels at a readable scale, then repeat the Ozone orientation and clipping
check.

The `b2a9dc0` trial removed the hard boundary, but the font's absolute X
coordinates were compressed to 75 percent of the graphical-menu coordinates.
The captured Quick Menu placed labels around x=276 instead of XMB's intended
x=368, overlapping the icons. Batched font vertices do not pass through the
rectangle rotation used by the other menu primitives, so sharing the rotated
MVP does not preserve the same coordinate system. Patch 036 leaves font layout
and glyph construction in logical 640x480 coordinates, explicitly maps each
vertex to physical 480x640 coordinates, and draws it with the unrotated MVP in
the native viewport. This must restore both placement and glyph aspect without
reintroducing the x=480 clip wall.

## Final device acceptance

Commit `14f3efe` was deployed as the complete seven-file managed RetroArch
delta. All 7,060 component entries and 11,334 Runtime entries verified, and the
saved RetroArch configuration retained SHA-256
`b55e47acfb3b02ac9ccd7a96526e4b99736802ef8db659b5897fc85d6518801c`.

On the same vertical Varth route, the operator accepted XMB orientation, icon
and text placement, glyph aspect, and text visibility across the landscape
width. A temporary Ozone launch then passed orientation, text placement, and
cursor movement without modifying the saved menu driver. Together with the
earlier RGUI acceptance, all three Pixel2 menu routes now pass this rotated-core
regression.
