# Pixel2 exclusive USB mode

## Problem

Pixel2 has one dual-role USB port. The frontend previously exposed independent
Wi-Fi and ADB checkboxes while System ADB, USB-host re-enumeration, boot Wi-Fi,
and Runtime recovery read separate saved values. Both sides could therefore
believe they owned the same DWC2 controller.

The latest physical result was still `waiting` at cold boot and after cable
replug. This result is not treated as proof that the ownership change fixes the
remaining FunctionFS enumeration fault; persistent ADB logs still need to be
captured from the card after deployment.

## Contract

`/mnt/plumos/config/network/services.conf` now stores one authoritative value:

```text
usb_mode=adb|wifi|off
```

- `adb`: FunctionFS ADB owns the port; all Wi-Fi host/reset/recovery actions
  are refused.
- `wifi`: USB Wi-Fi owns the port; adbd is not started.
- `off`: neither side may claim or reset the port.
- a FAT32 `plumos-enable-adb` marker overrides every value for offline
  recovery.

The frontend shows `USB Mode < ADB | Wi-Fi | Off >` under Network Settings.
The former Wi-Fi and ADB checkboxes are removed, and ADB is no longer presented
as a network daemon checkbox beside SSH/FTP/SFTP/Samba. Role changes are saved
atomically and applied after reboot so the frontend never destroys its active
maintenance transport while editing the setting.

Legacy `adb_enabled` and system `wifi_enabled` values remain synchronized for
upgrade compatibility, but new boot and runtime code consults `usb_mode` first.

## Host verification

- frontend AArch64 component cross-build: pass;
- `adb`, `wifi`, and `off` persistence/marker fixture: pass;
- ADB policy and FunctionFS recovery fixture: pass;
- USB-host re-enumeration fixture, including Off no-write: pass;
- network-control and Wi-Fi recovery ownership gates: pass;
- six translation files with identical 362-key sets: pass;
- frontend implementation contract audit: pass.

## Pending physical acceptance

1. deploy signed System and Runtime with matching manifests/checksums;
2. verify default/cold-boot ADB reaches a real host transport;
3. verify cable replug returns to ADB without an FE toggle;
4. select Wi-Fi mode, reboot with the validated 8821CU dongle, and verify LAN;
5. select ADB mode again, reboot with the Mac cable, and verify transport;
6. capture `/state/plumos/logs/adbd.log` if ADB remains `waiting`.

## Signed packages

Commit `21fba08` was built as System and strict Runtime
`0.1.0-dev-21fba08`. System uses installed `1e065fb` as its required source.
An offline read-only capture later proved that the installed Runtime was still
`8a98e3e`, not the previously assumed `5535fa8`; the Runtime delta was therefore
rebuilt against the captured `8a98e3e` checksum list. The real updater accepted
the Ed25519 signature, device/architecture, exact source version,
System/Runtime ABIs, manifest, and every declared payload hash.

- System archive SHA-256:
  `9fea195b63ce375d78a9351fa9eedc5dcac1e1baa1225760f2f422d1a1d5bc5b`;
- System SquashFS SHA-256:
  `57b3e98d534bddb525c8cf67189a37f7f8c6cf7432abff0f273d08b76515167b`;
- Runtime archive SHA-256:
  `ac393c80eb5c9a802d995941ea55088f173331b8cf56b48b18928fe8ea29183d`;
- strict Runtime root checksum-list SHA-256:
  `003dbd35eb0dd9a371c57e534fd3ca02290296c91d5d9df01b26968d04080992`.

The corrected Runtime package contains 24 changed managed files and no
deletion. Both archives are retained under
`output/live/2026-08-19-exclusive-usb-mode/`.

## Offline SD deployment

The attached 62.5 GB card was identified as `/dev/disk4` with the expected
PLUMOS_BOOT, Linux Runtime, and PLUMOS_USER partitions. System A read back at
the prior deployed SHA-256
`336a109153801b64daef1c346c0fa8956605dc838951ea3d312524b495aa0485`
and was backed up with all five managed metadata files under
`offline-backup-system-a-1e065fb/` before any replacement.

Only System A's SquashFS, slot manifest, checksum, signed update manifest, and
signature were replaced. The on-card SquashFS read back at
`57b3e98d534bddb525c8cf67189a37f7f8c6cf7432abff0f273d08b76515167b`,
its loose Ed25519 signature verified, and the manifest payload hash matched.
Inactive B remained byte-identical at
`2a6170fea9dcec458636672eb44d8256bbe9676ff6994e378b0d14e5458f3259`.

The initial Runtime delta and its checksum were added to the FAT32 update inbox,
but its assumed source version did not match the installed Runtime. The
corrected `8a98e3e -> 21fba08` delta replaced only that update-inbox package and
sidecar, then read back at
`ac393c80eb5c9a802d995941ea55088f173331b8cf56b48b18928fe8ea29183d`.
The zero-byte ADB recovery marker was preserved. ROM, BIOS, settings, saves,
installed ports, the Linux Runtime partition, and all unrelated update files
were not modified. The Runtime delta still requires the normal frontend
System Update request because merely placing a package in the inbox does not
create an update transaction.

## Offline Runtime capture

`scripts/capture-pixel2-runtime-macos.sh` captured the Linux partition
read-only after System `21fba08` had booted. The evidence under
`output/live/2026-08-19-pixel2-state-capture/capture.aLzThL/` shows:

- installed Runtime `0.1.0-dev-8a98e3e`, with its last transaction marked
  `runtime_healthy`;
- legacy `adb_enabled=1` and `wifi_enabled=true` saved simultaneously, with no
  authoritative `usb_mode` because the new Runtime had not been installed;
- repeated old-Runtime `watchdog-recover`, UDC rebind failures, and clean adbd
  restarts before later boots switched to the kernel-uevent monitor;
- the current boot reaching `FUNCTIONFS_BIND` but not `FUNCTIONFS_ENABLE`,
  matching the frontend's `waiting_usb` state.

This proves that the observed `waiting` result did not test the exclusive USB
implementation: System was current, but Runtime ownership and frontend code
were stale. Physical acceptance resumes only after the corrected Runtime is
applied and promoted healthy.
