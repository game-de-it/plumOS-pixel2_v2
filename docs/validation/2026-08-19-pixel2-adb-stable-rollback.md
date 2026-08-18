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
- keep that initial FunctionFS/adbd instance alive while a slow host attaches;
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

## Offline SD deployment

The attached 62.5 GB Pixel2 card was identified as `/dev/disk4`. Before any
write, System A read back as failed synchronous build `5246728` with SHA-256
`99ce6ed925d7a510b0feff966ff470613827f8677a5ff58a8f72b02f33e64592`.
Its five managed files were copied to
`/offline-backup-system-a-5246728-pre-stable-adb/` on PLUMOS_USER and the old
signature verified. Inactive System B was
`2a6170fea9dcec458636672eb44d8256bbe9676ff6994e378b0d14e5458f3259`.

The signed System and Runtime packages were copied to the FAT32 update inbox
and read back at the documented hashes. System A was then installed through
temporary filenames. Final verification passed its Ed25519 signature,
manifest source/version, manifest payload SHA, and SquashFS readback at
`4ee5dcb43827ce37696de7f2cc666bab950982736f6d8c157862dbb3cc1bcd38`.
System B and the zero-byte ADB recovery marker remained unchanged.

The Runtime package is staged but intentionally not requested yet. This keeps
the first physical test limited to the restored System adbd path. After a
successful cold-boot shell, Runtime `21fba08 -> 11a7f94` will restore the
physically accepted hardware-key replug path with app-layer metadata kept
consistent. Cold-boot shell and physical cable replug remain pending.

## First physical result

The System-only cold boot still reported frontend ADB `waiting`. macOS did,
however, enumerate the complete new gadget rather than only its parent:

- product `plumOS Pixel2 ADB`, `18d1:4ee7` at 480 Mbit/s;
- interface class/subclass/protocol `255/66/1`;
- bulk OUT endpoint 2 and bulk IN endpoint 1, both 512-byte packets;
- current USB configuration 1.

Restarting host ADB 36.0.2 with the Apple native backend and without a cable
replug did not create a transport. Its trace opened both endpoints and queued
CNXN, but the first bulk read and write immediately failed with macOS status
`e00002ed` (`kIOReturnNotResponding`). This rules out a stale frontend status
label and a merely missing interface. The device has exposed FunctionFS
descriptors but the bulk endpoint path is not responding.

## Persistent log result

The Runtime partition was captured read-only after the failed boot under
`output/live/2026-08-19-pixel2-state-capture/capture.vEKt93`. The exact
`22:47` boot proves that the restored four-second startup check, rather than
the nonblocking transport itself, destroyed the valid initial instance:

```text
22:47:02 FUNCTIONFS_BIND
22:47:02 result=started
22:47:06 action=watchdog-recover reason=udc-not attached
22:47:07 result=rebind-failed action=clean-restart
22:47:08 result=stopped
22:47:10 FUNCTIONFS_BIND
```

There is no `FUNCTIONFS_ENABLE` after the restart. The host may legitimately
leave the UDC at `not attached` longer than four seconds before selecting the
configuration, so this state is not a boot failure and must not trigger a
timer-based recovery. It explains the complete USB descriptors followed by
immediate `kIOReturnNotResponding` on both bulk endpoints.

System change `c41698a` keeps the physically proven nonblocking FunctionFS,
explicit physical-replug action, PID lock, current `usb_mode` ownership, and
ADB-priority guards. It removes only the autonomous startup timer and its
worker PID lifecycle. The recovery fixture now holds a cold-boot UDC in
`not attached` for five seconds and verifies that the original adbd PID stays
alive with no watchdog action. Explicit `recover` and `replug` behavior remain
covered separately.
