# Pixel2 WonderSwan rotation capture

Date: 2026-08-14

## Device state

The frontend launched the supplied WonderSwan sample through the expected
route:

```text
system=wonderswan
core=/mnt/plumos/cores/mednafen_wswan_libretro.so
rom=/mnt/plumos-user/roms/WS/Puzzle Bobble.ws
```

The live RetroArch append configuration selected the Pixel2 DRM software
rotation path with `video_rotation = "3"`. Pressing SELECT in the core switches
between its horizontal and vertical layouts, but the operator reported that
the resulting game image was upside down.

## DRM capture

`/dev/fb0` and the first active DRM plane contained a black frontend frame, not
the game. Enumerating every active KMS plane found the WonderSwan image on the
overlay plane:

```text
plane=58 fb=98  width=480 height=640 format=XR24 zpos=0
plane=81 fb=101 width=480 height=360 format=RG16 zpos=1
```

Plane 81 was read as little-endian RGB565 and rotated 90 degrees into the
Pixel2 physical viewing direction. The capture is retained outside Git at:

```text
output/live/2026-08-14-wonderswan-rotation/
  wonderswan-overlay-plane-81.display.png
```

The captured WonderSwan logo and text are rotated 180 degrees, confirming the
physical-device report.

## Diagnosis and repair

Beetle WonderSwan disables its internal software rotation when the frontend
accepts `RETRO_ENVIRONMENT_SET_ROTATION`. RetroArch initially passed the final
Pixel2 correction (`video_rotation = "3"`) to the DRM presenter, but a later
core rotation request replaced that state with the core-only value. The fixed
native-panel correction was therefore lost after the SELECT-controlled layout
change.

The Pixel2 launch profile now appends:

```text
video_rotation = "3"
video_allow_rotate = "false"
```

This is restricted to `wonderswan` and `wonderswancolor`. The core performs its
own horizontal/vertical content rotation first, while the unchanged Pixel2 DRM
presenter performs the final CCW panel correction. Other RetroArch systems keep
`video_allow_rotate = "true"`.

## Managed Runtime deployment

Commit `465b957` was rebuilt as Runtime `0.1.0-dev-465b957`. The frontend
component was regenerated after its checksum correctly rejected the changed
launcher, then the strict app-layer assembly passed. The signed delta contained
eight managed files, no deletions, and was read back from the device with the
same SHA-256:

```text
package=plumos-pixel2-runtime-0.1.0-dev-465b957.tar.gz
source_version=0.1.0-dev-612822f
sha256=a6ecefa9511d786943fef98219fe21586bcc5b67deb37f1810c46801b4c4eb4d
result=runtime_healthy
status=healthy
```

The installed launcher SHA-256 matched the host app-layer:

```text
73cdf2a0d688a9bb014a6b986b1129d9850349720d8ab5372cc4dcbd564772a1
```

The managed Runtime launched the same `Puzzle Bobble.ws` sample with
`video_allow_rotate = "false"`. The vertical game plane became `480x640`, and
the physical-direction capture at
`output/live/2026-08-14-wonderswan-rotation/wonderswan-managed-1.display.png`
shows readable, correctly directed text without the previous 180-degree
inversion. The SELECT-switched horizontal state remains a physical input and
capture gate; it was not triggered during the automated observation interval.

## Aspect-ratio follow-up

The direction repair exposed the global Pixel2 4:3 policy on WonderSwan. With
`aspect_ratio_index = "0"` and `video_aspect_ratio = "1.333333"`, the
core-rotated frame was expanded across a `480x640` game plane and appeared
stretched. WonderSwan's native display is `224x144`, or 14:9, and the selected
geometry also changes when SELECT rotates the content.

Commit `e327fb9` changes only `wonderswan` and `wonderswancolor` to RetroArch's
core-provided aspect entry:

```text
video_rotation = "3"
video_allow_rotate = "false"
aspect_ratio_index = "22"
```

A live comparison changed the game plane from the forced `480x640` layout to
`480x309`; `480/309 = 1.553`, within framebuffer rounding of `224/144 = 1.556`.
The signed Runtime `0.1.0-dev-e327fb9` was then applied from
`0.1.0-dev-465b957`:

```text
package=plumos-pixel2-runtime-0.1.0-dev-e327fb9.tar.gz
sha256=1450fc692eefe2767649b7fa305b654a08a362ad3d16a46624027b27c3118dbe
payload_files=8
deleted_files=0
result=runtime_healthy
status=healthy
```

The first managed Runtime capture produced a `411x640` RGB565 plane, or
`640x411` in the physical viewing direction. That numerical comparison was a
false positive: the operator correctly rejected the visibly compressed image.
It captured the SELECT-switched 144x224 core frame after RetroArch had inverted
the core-provided aspect a second time for Pixel2's odd display rotation.

## SELECT-switched aspect repair

Beetle WonderSwan changes its software-rendered frame from 224x144 to 144x224
after SELECT when `video_allow_rotate = "false"`. RetroArch's generic
core-aspect lookup also inverts the reported 9:14 aspect because Pixel2 has
`video_rotation = "3"`. The Pixel2 DRM presenter already applies that fixed
panel rotation after calculating its logical viewport, so the generic
inversion incorrectly turns the SELECT-switched aspect back into 14:9.

Commit `f7bd277` adds a Pixel2 DRM correction restricted to
`ASPECT_RATIO_CORE`: when the DRM software rotation is odd, it cancels the
generic lookup inversion before calculating the viewport. Fixed 4:3 systems
and non-core aspect entries are unaffected.

The signed Runtime update was applied from `0.1.0-dev-e327fb9`:

```text
package=plumos-pixel2-runtime-0.1.0-dev-f7bd277.tar.gz
sha256=58eceac3149a41b0b37d68d7e428b6acadac22315730c15eff29919c35f9ecb2
payload_files=8
deleted_files=0
result=runtime_healthy
```

After one physical SELECT press during gameplay, the managed core emitted a
144x224 portrait frame and the official DRM overlay was `480x309`. Rotating
that plane into the physical viewing direction gives `309x480`;
`309 / 480 = 0.64375`, matching `144 / 224 = 0.64286` within framebuffer
rounding. The decoded capture is retained outside Git at:

```text
output/live/2026-08-14-wonderswan-post-select-aspect/
  wswan-managed-f7bd277-2.display.png
```

This capture is specifically the post-SELECT state; an initial boot frame is
not used as evidence for the repair. Final acceptance of the physical LCD
appearance remains the operator's visual gate.
