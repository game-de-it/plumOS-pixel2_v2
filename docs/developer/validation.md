# Validation and Evidence

[日本語](validation.ja.md)

## Evidence Rules

Validation records live under `docs/validation/`. Each record should include
the source commit, generated hashes, commands, live-device evidence, physical
observations, and remaining risk.

A host build or file existing in `output/` is not proof that it works on
Pixel2. Useful evidence includes live process identity, mount ownership,
checksums, framebuffer/input ownership, Wi-Fi/SSH status, power behavior, and
physical controls.

## Current Baseline Evidence

- [Host build](../validation/2026-08-11-plumos-host-build.md)
- [Stock boot substrate](../validation/2026-08-12-stock-boot-substrate-image.md)
- [Stock initramfs handoff](../validation/2026-08-12-stock-initramfs-handoff.md)
- [Frontend input](../validation/2026-08-12-pixel2-frontend-input.md)
- [Power management](../validation/2026-08-12-pixel2-power-management.md)
- [Global power menu and sleep](../validation/2026-08-15-pixel2-global-power-menu-sleep.md)
- [Sleep/resume machine matrix](../validation/2026-08-23-pixel2-sleep-machine-matrix.md)
- [START menu](../validation/2026-08-12-pixel2-start-menu.md)
- [Implementation audit](../validation/2026-08-13-pixel2-implementation-audit.md)
- [User BIOS staging and device placement](../validation/2026-08-13-pixel2-user-bios.md)
- [PortMaster and Ports](../validation/2026-08-17-pixel2-portmaster-ports.md)
- [PortMaster generic compatibility layer](../validation/2026-08-27-pixel2-portmaster-compatibility.md)
- [Release-candidate image](../validation/2026-08-23-pixel2-release-candidate-image.md)
- [First Wi-Fi connection](../validation/2026-08-23-pixel2-first-wifi-connect.md)
- [Neo Geo repeated launch/exit](../validation/2026-08-23-pixel2-neogeo-loop.md)
- [v0.1.3 device update](../validation/2026-08-29-pixel2-v0.1.3-device-update.md)
- [v0.1.4 release artifacts](../validation/2026-08-30-pixel2-v0.1.4-artifacts.md)
- [v0.1.4 device update and Rockbox](../validation/2026-08-30-pixel2-v0.1.4-device-update.md)
- [PortMaster GPTokeYB exit recovery](../validation/2026-08-31-pixel2-portmaster-gptokey-exit.md)

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
9. network maintenance path validation through at least one USB Wi-Fi
   dongle route.

The complete implementation and device-validation inventory is maintained in
[Pixel2 Implementation Inventory](implementation-status.md).
