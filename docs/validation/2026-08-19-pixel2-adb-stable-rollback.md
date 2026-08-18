# Pixel2 ADB stable-path rollback

## Decision

Cold boot remained at frontend ADB `waiting` with System `5246728`. Git history
shows that the initial `10fc87a` implementation already used legacy
synchronous FunctionFS; it was changed to nonblocking FunctionFS by `6f022d4`
as part of the first transport stabilization. The first complete physical
cold-boot, shell, and cable-replug acceptance is recorded for `45b4505`.

The rollback therefore uses `45b4505`, not the earlier unaccepted bring-up
code, as its behavioral baseline.

## Restored contract

- build adbd with `PLUMOS_ADBD_USB_FFS` and nonblocking FunctionFS;
- remove the adbd protocol-state patch and kernel uevent monitor;
- bind one configfs FunctionFS gadget in device role at boot;
- perform one four-second startup health check;
- request one `replug` two seconds after a physical USB offline/online event;
- serialize all mutating actions with the original PID-owned lock;
- retain current `usb_mode=adb|wifi|off` policy and the FAT recovery marker;
- keep current ADB-priority guards in USB host and Wi-Fi services.

Power-menu, sleep, emulator, network-service, update, ROM, BIOS, save, and
frontend functionality outside this ADB path is unchanged.

## Host verification

- ADB recovery/replug/ownership fixture: pass;
- power-menu and sleep fixture: pass;
- exclusive USB-mode fixture: pass;
- System rootfs source contract: pass.
- AArch64 adbd build: pass, SHA-256
  `f51ed4e58ff3c0cde73a01a7fe95f0058f6ca67ab5a94848e1c8f78a4294abec`;
- strict app-layer build: pass;
- complete System A/B rootfs build and verification: pass;
- System SquashFS SHA-256
  `4ee5dcb43827ce37696de7f2cc666bab950982736f6d8c157862dbb3cc1bcd38`.

Signed packages were inspected against the versions captured from the real
card:

```text
System:  5246728 -> 11a7f94
SHA-256: f801b00125d6c9a04c783ef26425fe4ef3deffec4777e117aaa8e0934178c58d

Runtime: 21fba08 -> 11a7f94
SHA-256: 14c3721b4821be6bff4e2ed74060072f962c750b19e6d0c6fbdbfa614c7adc11
files:    9 changed, 0 deleted
```

Both updater inspections passed signature, device, architecture, vendor
runtime, exact source version, ABI, manifest, and payload validation. Packages
are retained under
`output/live/2026-08-19-pixel2-adb-stable-rollback/`.

Offline SD deployment, cold-boot shell, and physical cable replug remain
pending.
