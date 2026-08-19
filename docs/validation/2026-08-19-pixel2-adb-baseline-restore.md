# Pixel2 simple ADB baseline restore

Date: 2026-08-19

Implementation commits:

- `f30be85` restores the original ADB service and daemon-only watchdog.
- `fde06bb` prevents other boot services from taking ownership of the single
  Pixel2 OTG controller during recovery.

## Decision

Repeated changes to USB-mode policy, UDC health checks, delayed recovery,
replug handling and complete gadget recreation did not restore a usable host
transport. The frontend continued to report `waiting`, while prior captures
showed that macOS could enumerate configuration 1 but the first ADB `CNXN`
failed at the FunctionFS bulk endpoint.

The recovery baseline therefore stops adding heuristics. It restores the two
System scripts from the initial stable FunctionFS implementation at `6f022d4`
byte-for-byte:

```text
10-adbd sha256=6d18796073275d667889a9d2c5b9e2df2eae298003c2bbb94f2d937579c81d22
15-adbd-watchdog sha256=b67891ffd006701d96e82442491ec89eacd9866f65e077946e12d0fde908b876
```

`10-adbd` now has one contract: force device role, create a single FunctionFS
ADB gadget, launch the nonblocking adbd build, wait for ep1/ep2, and bind the
stock Pixel2 UDC. It does not read frontend settings, Wi-Fi state,
`services.conf`, `usb_mode`, or a recovery marker. It exposes only `start`,
`stop`, and `restart`. The watchdog only restarts a missing adbd process and
does not inspect or rebind the UDC.

## ADB-only isolation

The currently installed Runtime can still invoke System network controls at
boot. To keep this recovery test independent of Runtime version and mutable
settings, the System image contains `/etc/plumos-adb-only`. While present,
these boot services exit without changing the controller:

- `15-usb-host-reenumerate`
- `20-usb-wifi`
- `35-network-services`

The frontend and emulator Runtime remain available, but Wi-Fi and the shared
network-service bootstrap are deliberately deferred until physical ADB is
accepted. Runtime replacement is not part of this recovery step.

## Host verification

```text
pixel2_adbd_baseline=result-ok source=6f022d4
pixel2_usb_host_reenumerate=result-ok
system_rootfs_scripts=result-ok
system_dispatcher=result-ok
system_rootfs=result-ok (System A)
system_rootfs=result-ok (System B)
```

Built System version and artifacts:

```text
version=0.1.0-dev-fde06bb
SYSTEM sha256=8d1c73d401518ad28a7a1afca79f077fb30e2917942a5d0691ef62dc076a600a
system-a.squashfs sha256=7d71d2f6cf2190141b37f46c2971161a120087db666dfdb3e2e31179ce1b0787
system-b.squashfs sha256=7d71d2f6cf2190141b37f46c2971161a120087db666dfdb3e2e31179ce1b0787
```

## Pending physical acceptance

The SD card is not connected to the Mac at the end of the host build. Before
writing, inspect the active System slot and version read-only, back up the
managed boot/System files, and preserve the inactive slot, Runtime, ROMs,
BIOS, saves and settings. Then apply the complete dispatcher/System payload
and verify its SHA-256 by readback.

The first acceptance is intentionally narrow: cold boot must provide a working
`adb shell`. Frontend status text and cable-replug recovery are not substitutes
for that transport proof. Wi-Fi and USB-mode switching remain disabled until
this baseline passes.
