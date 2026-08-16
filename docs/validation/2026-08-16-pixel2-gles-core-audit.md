# Pixel2 GLES core presentation audit

Date: 2026-08-16
Implementation commit: `2944596`
Runtime: `0.1.0-dev-2944596`

## Scope

The app-layer core manifest, frontend launch profiles, core metadata, binaries,
and launcher policy were compared. The enabled Pixel2 libretro payload contains
four hardware-GLES cores:

| core | system route | status |
| --- | --- | --- |
| `flycast_libretro.so` | Dreamcast alternate | enabled |
| `flycast_xtreme_libretro.so` | Dreamcast default | enabled |
| `parallel_n64_libretro.so` | N64 default | enabled |
| `km_duckswanstation_xtreme_amped_libretro.so` | PSX alternate | enabled |

YabaSanshiro is built by the wider catalog as hardware GLES but is excluded
from the active app-layer with Saturn's `unsupported_performance_rk3326`
policy. `vecx_libretro.info` advertises `hw_render=true`, but the Pixel2 build
manifest classifies the installed VecX core as software and its binary does not
link or identify an OpenGL/GLES renderer. It is therefore outside this GL
presentation contract.

## Failure boundary

ParaLLEl N64 was compiled for AArch64 GLES and reported `hw_render=true`, but
the launcher omitted it from the GL case. It consequently started with:

```text
video_driver=drm
video_rotation=3
aspect_ratio_index=0
```

The final XR24 plane showed both errors: the logical image was upside down and
the 4:3 frame became a 3:4 column after the portrait scanout was rotated.
RetroArch's menu texture also used a separate orientation path.

## Implementation

All enabled hardware-GLES cores now share one Pixel2 launcher contract:

```text
video_driver=gl
video_rotation=1
aspect_ratio_index=24
PLUMOS_GL_MENU_ROTATION=content
```

The N64, PSX, and Dreamcast routes reject no-op core rotation changes so the
fixed Pixel2 panel correction remains authoritative. Full aspect fills the
physical 480x640 GL scanout and becomes an undistorted logical 640x480 image
after rotation. The environment-gated RetroArch patch applies the same content
matrix to RGUI without changing software-rendered systems.

`verify-app-layer.sh` now reads the libretro component manifest and fails if a
future `rendering=hardware-gles` core is absent from the Pixel2 launcher. This
turns the omission that affected N64 into a release-gate failure.

## Device captures

Every capture below reads the final active plane on `/dev/dri/card0`, not
`/dev/fb0`. XR24 480x640 data is rotated into the accepted Pixel2 logical
640x480 direction before inspection.

| core | representative content | game | RGUI |
| --- | --- | --- | --- |
| ParaLLEl N64 | `SUPERMARIO64.Z64` | upright, full 4:3 | upright over game |
| DuckSwanStation Xtreme | `chroQW.img` | upright, full 4:3 | upright over game |
| Flycast | `Crazy Taxi (Japan).chd` | upright, full 4:3 | shared GL/RGUI path |
| Flycast Xtreme | `Crazy Taxi (Japan).chd` | upright, full 4:3 | physically accepted on `f46740d`; policy unchanged |

Evidence roots:

- `output/live/2026-08-16-gles-core-audit/n64-official/`
- `output/live/2026-08-16-gles-core-audit/duckswan-official/`
- `output/live/2026-08-16-gles-core-audit/flycast-standard-after/`
- `output/live/2026-08-16-dreamcast-menu-rotation/`

The N64 and DuckSwan RGUI frames were initially toggled through RetroArch's
standard `MENU_TOGGLE` command while the same managed binary, config, core,
content, and presentation variables were active. On the exact official Runtime
`0.1.0-dev-2944596`, the operator then pressed the physical Function button
while N64 was running and confirmed that Quick Menu was upright on the LCD.
The final active XR24 plane 58 was captured at 480x640 and its logical view is
upright at 640x480:

- `output/live/2026-08-16-gles-core-audit/n64-official/physical-menu-logical.png`

The physical Function mapping is unchanged by this patch, and its N64 menu
orientation acceptance check is complete.

## Signed deployment

Only the launcher and corresponding Frontend/root metadata were deployed. No
core binary, RetroArch binary, ROM, BIOS, save, state, or user configuration
was replaced.

```text
package=plumos-pixel2-runtime-0.1.0-dev-2944596.tar.gz
sha256=74a6b92d9812644db07c534f26dce588277df7b2da619d837f979fb1ffef7b1d
source_version=0.1.0-dev-f46740d
payload_files=6
deleted_files=0
status=runtime_healthy
frontend_checksums=194/194
retroarch_checksums=59/59
core_checksums=357/357
root_checksums=4248/4248
```
