# Pixel2 frontend and ADB bring-up image

Date: 2026-08-11
Scope: host-side build and image validation; physical Pixel2 pending

## Result

The Pixel2 System now starts the plumOS frontend after mounting STATE and ROMS.
It uses `/dev/fb0`, selects the Pixel2 gamepad or gpio-keys event device, and
keeps PID 1 alive while the frontend runs. The frontend snapshot is derived
from the pinned plumOS-MF commit recorded in
`vendor/plumos-frontend/SOURCE.manifest`. Foreign distribution names were
removed from source, configuration, themes and the generated System.

ADB startup now requests USB device role before locating the UDC. Both ADB and
frontend startup write logs to the persistent STATE partition. The
`plumos-diagnostics` command reports mounts, USB role and UDC state, DRM,
framebuffer, input devices, service PIDs, service logs and recent kernel output.
It is also captured automatically before and after service startup.

## Host validation

- implementation commit: `e491d24`
- System size: 16 MiB
- frontend: ARM64 PIE executable with the packaged runtime libraries
- frontend and library scanner: executed with `--help` in ARM64 chroot
- Dropbear and rootfs managed-file checks: passed
- foreign identity scan: passed
- SD prefix, partition boundaries, filesystem checks and embedded payload
  comparison: passed
- image: `output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img`
- image size: 2,147,483,648 bytes
- image SHA-256:
  `97c4f3309c015f1b7e37b2fd2c95a7e1d304a0e377126a0110d40e832054b621`

## Hardware gate

This result does not assert physical boot success. Flash the image to the test
SD, cold boot the Pixel2 and verify that the frontend replaces the Pixel logo.
Then connect USB and collect:

```sh
adb devices -l
adb shell plumos-diagnostics
adb pull /state/plumos/logs pixel2-logs
```

If ADB does not enumerate, mount the STATE partition on the host and recover
`plumos/logs/adbd.log`, `plumos/logs/frontend.log`, and
`plumos/logs/runtime-diagnostics.txt` for the next iteration.
