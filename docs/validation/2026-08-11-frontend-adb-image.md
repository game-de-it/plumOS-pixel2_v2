# Pixel2 frontend and ADB bring-up image

Date: 2026-08-11
Scope: host-side build and image validation; physical Pixel2 pending

## Result

The Pixel2 System now starts the plumOS frontend after mounting STATE and ROMS.
It uses `/dev/fb0`, selects the Pixel2 gamepad or gpio-keys event device, and
keeps PID 1 alive while the frontend runs. The frontend snapshot is derived
from the pinned reference implementation commit recorded in
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
  `2970250c3c2f6a47d0d2b944d3c3c2fb4977452fa2e71daa81a36a620094b672`

## Physical Pixel2 result

The first frontend image booted successfully and exposed an operational ADB
shell. Live diagnostics confirmed the DSI mode is `480x640`, the USB role is
`device`, the UDC is configured, and the controller is
`gkd-pixel2-joypad` on `/dev/input/event4`.

The initial frontend appeared sideways because the Pixel2 panel is mounted 90
degrees relative to its DRM mode. The fbdev renderer now supports quarter-turn
rotation, and the Pixel2 service selects `ccw`, yielding a logical `640x480`
frontend. This orientation was confirmed correct on the physical device using
the temporary ADB-deployed binary. The boot service now prioritizes the Pixel2
joypad over the separate generic gpio-keys device.

The corrected image was generated from implementation commit `f23dafd` and
passed the complete host image verifier. Button mapping, audio, USB Wi-Fi, SSH
and power behavior remain separate hardware gates.

The corrected System was also deployed over ADB to the live BOOT partition so
the rotation fix persists across reboot. The copy was verified before and after
activation, and BOOT was returned to read-only mode.

- active System SHA-256:
  `21c523e0093fada60a644d619835a055de2d7de2fe52cc0bcf35e6f7a84f5297`
- recoverable previous System: `/boot/SYSTEM.bak-f5af0b81`
- previous System SHA-256:
  `f5af0b817dadb94715d3ae99941ad9d044db4e20c2b8ffdd9518a0637c1f9d7b`

## Hardware gate

For subsequent hardware passes, flash the image to the test SD, cold boot the
Pixel2 and collect:

```sh
adb devices -l
adb shell plumos-diagnostics
adb pull /state/plumos/logs pixel2-logs
```

If ADB does not enumerate, mount the STATE partition on the host and recover
`plumos/logs/adbd.log`, `plumos/logs/frontend.log`, and
`plumos/logs/runtime-diagnostics.txt` for the next iteration.
