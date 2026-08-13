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

The initial corrected DRM capture is
`output/live/2026-08-14-ppsspp-aspect-ui/ppsspp-aspect-ui-fixed-title.display.png`.
Its game region is approximately `640x363`, matching the native PSP aspect
within scanout rounding. Host source gates pass:

```text
PASS: Pixel2 emulator FUNCTION menu contract
app_layer_scripts=result-ok
```

Commit `612822f` was rebuilt as Runtime `0.1.0-dev-612822f`. The standalone
component checksum inventory and strict app-layer assembly passed. The signed
delta contained 15 managed files, no deletions, and was read back from the
device with matching SHA-256:

```text
package=plumos-pixel2-runtime-0.1.0-dev-612822f.tar.gz
source_version=0.1.0-dev-44051cc
sha256=8160770eb7930e26db9394ef7be143da6762c0fd9891516906be2fafcab29fbe
result=runtime_healthy
status=healthy
```

After the managed update, PPSSPP was launched again with
`Telegraph Crosswords.cso`. The final primary-plane capture is retained at
`output/live/2026-08-14-ppsspp-aspect-ui/ppsspp-final-title.display.png`.
ImageMagick measured the non-black game region as exactly `640x363`, a ratio
of 1.7631. The source ROM SHA-256 remained
`eb155f0f8812ac9fdde6b8a882d564c5d55bea2aded18f36d448446de51138a3`
before and after the update, and the mutable PPSSPP configuration retained the
corrected values.

The physical Function button opened PPSSPP's pause menu on the managed Runtime.
The resulting DRM capture is retained at
`output/live/2026-08-14-ppsspp-aspect-ui/ppsspp-final-menu.display.png`; it
shows the larger save-state labels and right-side menu entries without clipping.
The operator confirmed that both the game presentation and menu text size were
acceptable. PPSSPP was then stopped through `plumos-standalone-stop`, and one
frontend process was restored on `/dev/input/event2`.
