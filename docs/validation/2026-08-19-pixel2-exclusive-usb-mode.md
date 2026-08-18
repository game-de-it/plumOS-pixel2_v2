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
`0.1.0-dev-21fba08`. System uses installed `1e065fb` as its required source;
the Runtime delta uses the last confirmed app-layer base `5535fa8`. The real
updater accepted the Ed25519 signatures, device/architecture, source versions,
System/Runtime ABIs, manifests, and all declared payload hashes.

- System archive SHA-256:
  `9fea195b63ce375d78a9351fa9eedc5dcac1e1baa1225760f2f422d1a1d5bc5b`;
- System SquashFS SHA-256:
  `57b3e98d534bddb525c8cf67189a37f7f8c6cf7432abff0f273d08b76515167b`;
- Runtime archive SHA-256:
  `20962355c66155228bb979442dd7b5207fa5f82549c112cf2df054c46e8ecb16`;
- strict Runtime root checksum-list SHA-256:
  `003dbd35eb0dd9a371c57e534fd3ca02290296c91d5d9df01b26968d04080992`.

The Runtime package contains 22 changed managed files and no deletion. Both
archives are retained under `output/live/2026-08-19-exclusive-usb-mode/`.
