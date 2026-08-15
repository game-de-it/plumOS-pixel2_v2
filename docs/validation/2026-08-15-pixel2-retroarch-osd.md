# Pixel2 RetroArch on-screen notification validation

Date: 2026-08-15
Runtime: `0.1.0-dev-70357bb`
Implementation commit: `70357bb`

## Failure boundary

The Pixel2 plain DRM OSD patch inherited from the earlier handheld port drew
glyphs in the physical 480x640 buffer coordinate system. RetroArch supplied
logical 640x480 coordinates, so notifications were rotated and clipped at the
edge of the LCD. The main and video threads also accessed the pending OSD text
and parameters without synchronization, which left an intermittent crash path
when notifications were enabled or replaced while a frame was presented.

The Pixel2 patch now maps every logical OSD pixel through the fixed panel
rotation, uses the logical viewport for placement, validates glyph/atlas
bounds, and protects the pending message with the existing DRM mutex. Factory
configuration enables the managed font and a narrow migration updates only
the known old disabled/empty values.

## Device acceptance

The signed Runtime update was applied through the normal inspect, request,
health-promotion path. The device reported `runtime_healthy`, and the managed
RetroArch and Frontend components verified as 59/59 and 191/191 files with no
checksum failures.

QuickNES was launched with the retained
`/mnt/plumos-user/roms/FC/Akumajou Densetsu.nes`. A controlled stdin command
created new state slots 7 and 8 without deleting or overwriting existing state
files. Direct DRM plane capture showed the complete horizontal message:

```text
100%: Saved state to slot: 8.
```

Five timed captures confirmed that the message remained correctly oriented
and was then removed normally. RetroArch remained alive while the message was
shown and exited with the requested SIGTERM (`rc=143`), not a segmentation
fault. The temporary stdin setting was removed by restoring the user config
byte-for-byte; its before/after SHA-256 was
`4d45e07001a0c55f2ae35839c90d17cde254686d0338279ebd63fc66a6ce11b1`.

Capture evidence is retained under the ignored developer output directory:

```text
output/live/2026-08-15-retroarch-osd-fixed/timed-strip-lcd.png
output/live/2026-08-15-retroarch-osd-fixed/osd-message-lcd.png
```

