# plumOS Pixel2 Developer Guide

This guide is the technical entry point for building, modifying, deploying, and
validating plumOS Pixel2. MF and V90S are references for plumOS contracts, but
Pixel2 uses its own stock RK3326 boot substrate and hardware runtime.
For normal installation and operation, see the [User Guide](../user/README.md).

## Start Here

1. [Architecture and ownership](architecture.md)
2. [Build guide](build.md)
3. [Boot and runtime services](runtime.md)
4. [Storage and updates](storage-and-updates.md)
5. [Frontend and emulator integration](frontend-emulators.md)
6. [Hardware services](hardware-services.md)
7. [Audio routing](audio-routing.md)
8. [Validation and evidence](validation.md)
9. [Implementation inventory and release blockers](implementation-status.md)
10. [Release process](release-process.md)

All ten guides have matching Japanese pages. Dated validation records,
accepted decisions, and implementation inventories are retained as engineering
evidence and may be English-only.

Compatibility reference: [USB Wi-Fi and network services](../configuration/connectivity.md).

## Repository Map

```text
artifacts/                 untracked stock SD captures and research inputs
docs/decisions/            accepted architecture decisions
docs/developer/            current developer-facing technical contract
docs/user/                 end-user operating manual
docs/validation/           dated host and physical-device evidence
docker/                    container build inputs
package/                   Pixel2 app-layer, frontend, RetroArch, and defaults
rootfs/pixel2/             plumOS-owned init and rootfs payload for SYSTEM
scripts/                   build, image, capture, deployment, and verification tools
tests/                     host contract and regression checks
vendor/plumos-frontend/    Pixel2 frontend, text UI, scanner, and helper sources
output/                    generated artifacts; untracked
```

## Non-Negotiable Contracts

- Pixel2 is the target. MF and V90S are references, not proof of identical
  hardware behavior.
- Stock Pixel2 boot artifacts own the Rockchip boot prefix, vendor kernel,
  registered runtime DTB byte-for-byte, and stock initramfs handoff needed to
  load `/boot/SYSTEM`.
- plumOS owns `/sbin/init` inside `SYSTEM`, app-layer runtime, frontend,
  emulators, services, settings, and power policy after the stock initramfs
  hands off to `SYSTEM`.
- `/mnt/plumos` is the writable app/runtime ABI. `/mnt/plumos-user` is the
  host-readable user/content volume. `/boot` is normally read-only.
- Only one foreground owner may present on `/dev/fb0` and the joypad input
  path. Launchers must release and restore FE ownership around children.
- App-layer live deployments are atomic metadata units: deploy managed files
  with matching `checksums.sha256`, `manifest.json`, and component manifests,
  then verify SHA-256 on the device before reboot.
- Never overwrite active settings, ROMs, BIOS files, saves, states, logs,
  credentials, SSH state, or user app data just to match host metadata.
- Do not expose unimplemented Apps or emulator profiles in the FE. The catalog
  must only advertise runtimes that exist in the Pixel2 app-layer.
- Release payloads must not contain ROMs, user BIOS files, secrets, or
  untracked stock extraction inputs.

## Source of Truth

Resolve conflicts in this order:

1. current Pixel2 source and tests;
2. accepted Pixel2 decisions under `docs/decisions/`;
3. current generated manifests and checksums;
4. this developer guide;
5. dated validation records;
6. MF/V90S documentation and history.

## Japanese

- [日本語の開発者向けガイド](README.ja.md)
