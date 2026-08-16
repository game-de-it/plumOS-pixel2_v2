# Pixel2 Dreamcast display correction

Date: 2026-08-16
Implementation commits: `243f98c`, `e514099`
Runtime: `0.1.0-dev-e514099`

## Failure boundary

`Crazy Taxi (Japan).chd` launched through
`retroarch:flycast_xtreme`, but the final Pixel2 DRM scanout was 180 degrees
opposite the accepted landscape panel direction. `/dev/fb0` was not evidence
for this GLES-backed core, so plane 58 on `/dev/dri/card0` was captured and
decoded directly as 480x640 XR24.

Flycast Xtreme sent `RETRO_ENVIRONMENT_SET_ROTATION=0` after RetroArch had
started with Pixel2's panel correction. The Pixel2 launcher also routed
Flycast through the software-oriented DRM backend even though the core uses a
real EGL/GLES context. Correcting only the rotation exposed a second issue:
the GL viewport treated the portrait scanout's forced 4:3 as a logical 3:4
image and added 140-pixel side bars after rotation.

## Implementation

- `flycast_libretro.so` and `flycast_xtreme_libretro.so` now use RetroArch's
  KMS/GBM `gl` driver. DuckSwanStation keeps its independently validated GL
  rotation policy.
- Flycast uses `video_rotation=1`, which compensates the opposite winding of
  the Pixel2 GL and software DRM presentation paths.
- Dreamcast rejects Flycast's no-op core rotation request with
  `video_allow_rotate=false`; Naomi and Atomiswave remain free to request
  vertical-game rotation.
- Dreamcast selects RetroArch Full aspect (`aspect_ratio_index=24`). This
  fills the physical 480x640 GL surface and becomes an undistorted logical
  640x480 4:3 image after rotation.
- Source and assembled app-layer tests now gate the Flycast GL, rotation,
  Dreamcast rotation policy, and Full-aspect contracts.

## Build and deployment

Frontend and strict app-layer assembly passed from `e514099`. The host had a
stale PicoArch SDL build output, so the target tree was reconciled with the
device's already accepted `d242dfc` PicoArch binary and component checksums
before packaging. The signed Runtime therefore contained only the launcher
and its corresponding Frontend/root metadata; no ROM, BIOS, save, user
configuration, or PicoArch payload was replaced.

```text
package=plumos-pixel2-runtime-0.1.0-dev-e514099.tar.gz
sha256=0f165c77bc6c2ad400585d0810bc711bb2b3b0e17b8d740cb501dc07358d9f6f
source_version=0.1.0-dev-243f98c
payload_files=6
deleted_files=0
status=runtime_healthy
frontend_checksums=194/194
picoarch_checksums=11/11
root_checksums=4248/4248
```

The installed managed launcher produced:

```text
video_driver=gl
video_rotation=1
video_allow_rotate=false
aspect_ratio_index=24
```

The final active plane occupied the complete 480x640 scanout. Rotating that
physical buffer into the Pixel2 logical direction produced a correctly
oriented 640x480 Crazy Taxi frame with no pillarboxing. The exact Runtime is
left running Crazy Taxi for operator LCD confirmation.

