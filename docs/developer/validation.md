# Validation and Evidence

## Evidence Rules

Validation records live under `docs/validation/`. Each record should include
the source commit, generated hashes, commands, live-device evidence, physical
observations, and remaining risk.

A host build or file existing in `output/` is not proof that it works on
Pixel2. Useful evidence includes live process identity, mount ownership,
checksums, framebuffer/input ownership, ADB/SSH status, power behavior, and
physical controls.

## Current Baseline Evidence

- [Host build](../validation/2026-08-11-plumos-host-build.md)
- [Stock boot substrate](../validation/2026-08-12-stock-boot-substrate-image.md)
- [Stock initramfs handoff](../validation/2026-08-12-stock-initramfs-handoff.md)
- [Frontend input](../validation/2026-08-12-pixel2-frontend-input.md)
- [Power management](../validation/2026-08-12-pixel2-power-management.md)
- [START menu](../validation/2026-08-12-pixel2-start-menu.md)

## Release Gate Direction

Before a Pixel2 release, require:

1. clean git status;
2. strict app-layer and SYSTEM builds;
3. image verification and reproducible hashes;
4. no ROMs, BIOS files, credentials, or stock extraction inputs in release
   payloads;
5. cold boot to FE on real hardware;
6. LCD orientation, full input map, volume, brightness, audio, reboot, and
   shutdown validation;
7. representative emulator launch/exit and save persistence;
8. network maintenance path validation through ADB and at least one USB Wi-Fi
   dongle route.
