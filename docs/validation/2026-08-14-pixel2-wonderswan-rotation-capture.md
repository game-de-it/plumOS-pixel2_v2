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
