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
