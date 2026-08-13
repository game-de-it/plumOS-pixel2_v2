# Pixel2 PPSSPP aspect and UI scale

Date: 2026-08-14

## Failure

`Telegraph Crosswords.cso` launched in the Pixel2 PPSSPP runtime after the
portrait-panel rotation repair, but the game image was visibly compressed in
the vertical direction. The PPSSPP pause-menu text was also too small for the
2.4-inch display.

Reading `/dev/fb0` did not capture the failure: PPSSPP owns a KMS/DRM primary
plane directly, while fb0 retained an older plumOS frontend frame. A temporary
ARM64 diagnostic opened `/dev/dri/card0`, queried the active plane and mapped
its framebuffer:

```text
plane=58 crtc=80 fb=98 width=480 height=640
format=XR24 pitch=1920 modifier=0
```

The raw XRGB8888 scanout was rotated to the physical viewing direction on the
host. The before image is retained outside Git at
`output/live/2026-08-14-ppsspp-aspect-ui/ppsspp-game-scanout.display.png`.

## Diagnosis

PPSSPP's rectangle trace reported:

```text
frame=481.0x641.0 orig=480.0x272.0
aspect=1.0000 out=481.0x272.6 rotation=1
```

After the final CCW scanout transform, this became approximately `640x204`, or
3.14:1, instead of the PSP native `480x272`, or 1.76:1. MF and V90S use an
aspect correction factor of `1.000000` because their display path is already
landscape. The same 480x640 portrait-panel presenter is already proven on the
Miyoo A30; its landscape PPSSPP layout applies the reciprocal 16:9 correction
factor `0.562500` before rotating the scanout.

Pixel2 also inherited `UIScaleFactor = -8` from the MF/V90S configuration.
PPSSPP converts that to multiplier 0.5, producing a logical UI of 1280x960 on
the Pixel2 path. The A30 factory uses `-2` (multiplier about 0.841), which is
appropriate for the same small 640x480-class display.

## Repair and device proof

The Pixel2 landscape defaults are now:

```text
UIScaleFactor = -2
DisplayAspectRatio = 0.562500
InternalScreenRotation = 1
```

The portrait fallback keeps aspect `1.000000` and uses rotation `2`. The
launcher migrates only exact values shipped by the earlier Pixel2 package:
UI scale `-8`, landscape aspect `1.000000`, and invalid rotation `0`. All other
PPSSPP settings, saves, states, ROMs, and game-specific configuration remain
untouched.

The live corrected trace reported:

```text
ui_scale=0.841 dp=761x571
frame=481.0x641.0 orig_ratio=0.9926
aspect=0.5625 out=481.0x484.6 rotation=1
```

The corrected DRM capture is
`output/live/2026-08-14-ppsspp-aspect-ui/ppsspp-aspect-ui-fixed-title.display.png`.
Its game region is approximately `640x363`, matching the native PSP aspect
within scanout rounding. Host source gates pass:

```text
PASS: Pixel2 emulator FUNCTION menu contract
app_layer_scripts=result-ok
```

The corrected menu font still requires a physical pause-menu observation after
the managed Runtime is rebuilt and deployed. This is intentionally separate
from the DRM game-aspect proof.
