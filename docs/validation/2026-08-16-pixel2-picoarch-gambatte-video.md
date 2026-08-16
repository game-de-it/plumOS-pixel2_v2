# Pixel2 PicoArch Gambatte video recovery

Date: 2026-08-16
Implementation commit: `d242dfc`
Runtime: `0.1.0-dev-d242dfc`

## Failure boundary

PicoArch continued playing Gambatte audio while the LCD was black and its
internal menu was not visible.  The live process still owned `/dev/fb0`, and a
direct 480x640 BGRA capture contained the current 160x144 game image.  A DRM
scanout probe reported `no mappable active plane`, proving that emulation and
frame generation were alive but the fbdev buffer was no longer connected to
an active KMS plane.  Explicit `FB_BLANK_UNBLANK` followed by
`FBIOPAN_DISPLAY` restored plane 58 and made the image visible again.

The restored image then showed the same Gambatte RGB565 byte-order symptom
previously documented by plumOS-XU20V32: the Pixel2 capture's dominant
`#d69629` color exactly matched the XU20 pre-fix evidence.  Gambatte reports
`RETRO_PIXEL_FORMAT_RGB565`, but its words require a byte swap before the
existing PicoArch scaler on this path.

## Implementation

- The PicoArch launcher enables `PLUMOS_PICOARCH_RGB565_BYTESWAP=1` only for
  `gambatte`/`gambatte_libretro.so`.  Other cores retain the existing RGB565
  path, and an explicit `0` or `1` override remains available for diagnosis.
- The Pixel2 presenter unblanks and pans the framebuffer after its first
  complete frame instead of assuming mmap writes are already scanned out.
- A `SIGCONT` handler only requests reactivation; the presenter performs the
  ioctl operations safely on its next frame.  This covers the existing global
  power-menu `SIGSTOP`/`SIGCONT` contract without adding a per-frame ioctl.
- The formerly line-number-only audio-rate patch now applies its function with
  context and validates exactly two input-rate replacements, so the added
  video conversion cannot shift audio code into the wrong function.

## Build and deployment

The clean AArch64 PicoArch build, app-layer script test, Function menu contract
test, strict app-layer assembly, and component checksums passed.  The signed
Runtime package was reduced to the six intended managed files; the existing
SDL library, ROMs, BIOS, saves, settings, and network-service state were not
replaced.

```text
package=plumos-pixel2-runtime-0.1.0-dev-d242dfc.tar.gz
sha256=7ffbd27a6a64bd3383d89a89783eac341428e9638c1678569621b6e5361ed3c7
payload_files=6
deleted_files=0
previous_version=0.1.0-dev-45b4505
status=healthy
```

The deployed PicoArch component verified every managed entry.  Gambatte logged
`rgb565_byteswap=1` and `framebuffer scanout activated`; KMS plane 58 was active
at 480x640 XR24.  The post-fix capture's dominant colors changed to `#a5baa5`
and `#212021`, and the operator reported that the physical LCD color appeared
normal.  A controlled `SIGSTOP`, framebuffer blank, and `SIGCONT` cycle kept
the same PicoArch process alive and logged a second scanout activation.

Physical Function-menu presentation on this exact Runtime remains a separate
operator check.  Its input implementation is unchanged from the previously
accepted `0.1.0-dev-45b4505` PicoArch path.
