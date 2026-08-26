# Pixel2 Implementation Inventory

[日本語](implementation-status.ja.md)

Last updated: 2026-08-26. The detailed task ledger is `TODO.md`; dated evidence
is under `docs/validation/`. This inventory distinguishes implementation,
machine checks, physical acceptance, and release readiness.

## Status Definitions

| Status | Required evidence |
| --- | --- |
| Implemented | pinned source, build target, runtime, FE route, metadata, host test |
| Host verified | reproducible build, artifact audit, app-layer/image route checks |
| Device verified | real LCD, controls, audio, exit, FE return, persistence as applicable |
| Release ready | device evidence plus update, legal, CI, packaging, and re-download gates |
| Accepted deferred | deliberately outside the Pixel2 product scope, not silently missing |

## Implemented Product Surface

- stock RK3326 boot prefix, stock 5.10.198 kernel/DTB/initramfs substrate, and
  plumOS-owned `SYSTEM` handoff from `/sbin/init` onward;
- compact three-volume image, first-boot `PLUMOS_SYS` expansion, and creation of
  host-readable `PLUMOS_USER` without reformatting an existing third partition;
- frontend, ROM scanner, six-item START menu, global POWER overlay, settings,
  help, scraping, thumbnail gallery, and six-system grid;
- RetroArch, 110 libretro cores including coexisting legacy and modern mGBA,
  PicoArch, PCSX-ReARMed, DraStic, PPSSPP,
  OpenBOR, Pyxel, and user-supplied PICO-8 runtime routes;
- File Manager, Music Player, RetroArch App, Pyxel Setup, PortMaster, Update
  PortMaster, and installed Ports launch routing;
- Pixel2 physical input map, Function-menu contracts, rotation/aspect fixes,
  RetroArch menus/languages/assets, saves, states, hotkeys, and OSD;
- RK817/USB ALSA routing, global volume and brightness, and accepted internal
  speaker boost up to +15 dB;
- global power menu, software standby fallback, emulator-specific pause/resume,
  reboot, unplugged shutdown, and stock plugged-shutdown charging mode;
- Wi-Fi-priority single-port OTG policy, `r8188eu`, Pixel2-ABI `8821cu`, UGREEN
  `0bda:1a2b -> c811`, hotplug recovery, SSH, SFTP, FTP, and Samba;
- signed Runtime transactions, immutable System dispatcher, A/B System slots,
  readback hashes, health promotion, and rollback state machines.

All visible FE Apps and routes are covered by the implementation release gate.
The product does not ship ADB, FunctionFS, or a USB Mode selector.

## Accepted Scope Decisions

- Saturn is not supported on RK3326 for performance reasons.
- The other eight disabled systems are outside the Pixel2 product scope.
- ScummVM, EasyRPG, Flycast, and NXEngine-Evo use their libretro product routes;
  additional standalone binaries are accepted deferred.
- Systems without compatible test content and the two currently unavailable
  firmware sets are accepted deferred until legally supplied test inputs exist.
- Experimental Linux 6.12 work is isolated from the release stock-5.10 path.
- RTL8821CU transfer-speed tuning is closed with the measured hardware limit;
  no speculative driver power changes are planned.

## Remaining Release Work

The current unchecked source of truth is `TODO.md`. At this update it contains:

1. select and add a top-level project license;
2. complete release-payload third-party notices and DraStic redistribution
   review;
3. add CI gates for tests, forbidden content/identity, implementation audit,
   manifests, and checksums;
4. produce versioned artifacts, `SHA256SUMS`, archive inspection, and GitHub
   re-download verification;
5. finish the remaining RTL8811CU/RTL8821CU release-acceptance matrix, including
   the outstanding adapter/band scenarios recorded in `TODO.md`;
6. complete enabled-system BIOS/firmware inventory checks when inputs exist;
7. repeat the final FE power path and cloned-SD cold-boot hardware acceptance on
   the exact release image.

Do not treat a successful build or `release-image` command as completion of
these physical, legal, and distribution gates.

## Automated Gates

```sh
./scripts/docker-build.sh audit --release-gate
./scripts/verify-app-layer.sh output/app-layer/pixel2/plumos
./tests/test-app-layer-scripts.sh
./tests/test-system-rootfs-scripts.sh
./tests/test-sd-image-scripts.sh
```

The audit fails when a visible FE surface lacks its runtime, backend, launch
profile, language/theme asset, component manifest, or checksum. It does not
replace physical display, controls, audio, Wi-Fi, power, and persistence tests.
