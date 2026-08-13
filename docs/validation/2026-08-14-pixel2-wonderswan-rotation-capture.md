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
physical-device report. This task records the failure only; the interaction
between the core-requested SELECT rotation and Pixel2's final panel correction
has not yet been changed.
