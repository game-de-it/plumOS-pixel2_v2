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

ARM64 binary build, signed System/Runtime packaging, offline SD deployment,
cold-boot shell, and physical cable replug remain pending.
