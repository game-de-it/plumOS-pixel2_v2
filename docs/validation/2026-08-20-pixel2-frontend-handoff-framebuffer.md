# Pixel2 frontend handoff framebuffer clear

Date: 2026-08-20

## Symptom

The final first-boot setup frame remained in `/dev/fb0`. Pixel2 DRM clients use
a different scanout plane, so the stale setup frame could briefly become visible
between the frontend and a game or application.

This is not limited to first boot. An updater or recovery screen can leave the
same stale fbdev plane behind before a later frontend start.

## Contract

Every `40-frontend` start clears the underlying Pixel2 framebuffer immediately
before the frontend process is created. The clear is deliberately part of the
common frontend handoff, rather than first-boot cleanup.

The System build generates `blank.raw` as native 480x640 XRGB8888 opaque black.
The rootfs verifier checks its exact size and every pixel value. A missing or
unwritable framebuffer is logged as a warning and does not prevent recovery FE
startup.

The frontend's existing pre-game framebuffer clear remains enabled. Together,
the two paths cover System-to-FE and FE-to-game/application ownership changes.

## Verification

Host gates:

```sh
./tests/test-system-rootfs-scripts.sh
./scripts/docker-build.sh system-rootfs
./scripts/verify-system-rootfs.sh output/system-rootfs/SYSTEM
```

Device acceptance still requires a fresh first boot or a deliberately staged
non-black fbdev frame, followed by FE and game/application transitions. The
screen may flash black during ownership handoff, but must never show the stale
setup/update/recovery image.
