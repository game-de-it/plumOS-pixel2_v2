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
- [Implementation audit](../validation/2026-08-13-pixel2-implementation-audit.md)

## Release Gate Direction

Before a Pixel2 release, require:

1. clean git status;
2. zero release blockers from `audit-pixel2-implementation.py --release-gate`;
3. strict app-layer and SYSTEM builds;
4. image verification and reproducible hashes;
5. no ROMs, BIOS files, credentials, or stock extraction inputs in release
   payloads;
6. cold boot to FE on real hardware;
7. LCD orientation, full input map, volume, brightness, audio, reboot, and
   shutdown validation;
8. representative emulator launch/exit and save persistence;
9. network maintenance path validation through ADB and at least one USB Wi-Fi
   dongle route.

The complete implementation and device-validation inventory is maintained in
[Pixel2 Implementation Inventory](implementation-status.md).
