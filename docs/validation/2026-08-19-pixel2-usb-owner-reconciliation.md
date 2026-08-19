# Pixel2 USB owner reconciliation

Date: 2026-08-19

## Physical evidence

The signed `fde06bb` System was promoted through the normal A/B updater and
confirmed as both active and booted slot A.  The device still failed to expose
an ADB transport after selecting `USB Mode = ADB` and rebooting:

```text
macOS parent device: 18d1:4ee7 plumOS Pixel2 ADB
USB configuration:   selected (1)
ADB interface:       absent
adb devices:         empty
frontend state:      STOP
```

The same boot left the configfs gadget bound but without a host-visible
FunctionFS interface.  After selecting Off and then Wi-Fi, the dongle did not
enumerate until an OS reboot.  The reboot restored RTL8821CU
`0bda:c820`, SSID association and IPv4 `192.168.10.110`.

SSH inspection established that this was not a cable or Wi-Fi credential
failure.  System and Runtime had incompatible ownership contracts:

- immutable `/etc/plumos-adb-only` made System start ADB and suppress its Wi-Fi
  boot helpers regardless of saved `usb_mode`;
- Runtime `21fba08` still started Wi-Fi from the saved mode and only persisted
  USB mode changes as `apply=reboot-required`;
- Runtime queried `10-adbd status`, while the restored `6f022d4` service only
  implemented start, stop and restart, so the frontend reported STOP even when
  the daemon and all three FunctionFS endpoint descriptors were open.

On the Wi-Fi boot, both owners were visible simultaneously: adbd PID 383 held
`ep0`, `ep1` and `ep2`, while the RTL8821CU dongle was enumerated on the same
DWC2 controller.  This is the concrete ownership conflict being removed.

## Reconciled contract

System reads the persisted mode exactly once before dispatching USB services:

- `adb`: start the simple FunctionFS service and daemon-only watchdog;
- `wifi` or `off`: do not start either ADB process;
- a FAT32 `plumos-enable-adb` recovery marker overrides the saved mode to ADB;
- a missing or legacy setting defaults to ADB.

The temporary immutable `/etc/plumos-adb-only` recovery marker is removed.
Normal Wi-Fi and network boot services are therefore available again.

The simple `6f022d4` ADB lifecycle is retained.  Its only extension is a
read-only `status` command required by the Runtime API.  Status checks the
owned PID and its open FunctionFS descriptors, then reports `running` for a
configured UDC or `waiting_usb` while no host is attached.  It never restarts,
rebinds or changes USB policy.

The current Runtime applies the three modes live: Wi-Fi and Off stop ADB before
claiming or releasing host mode, and ADB stops Wi-Fi before a clean ADB
restart.  Boot adoption uses `start`, so it does not replace an already healthy
gadget.

## Host verification

```text
pixel2_adbd_baseline=result-ok source=6f022d4 status=compatible
pixel2_usb_mode=result-ok
pixel2_usb_host_reenumerate=result-ok
pixel2_network_control=result-ok
pixel2_wifi_recovery=result-ok
system_rootfs_scripts=result-ok
```

Physical acceptance remains pending for live Off to Wi-Fi, live Off to ADB,
ADB shell, cable replug and status transitions.

## Build version gate

The first signed package carried target version `0.1.0-dev-4ddb809`, but the
System build had used its default and embedded `0.1.0-dev`.  The A/B update and
USB ownership checks still completed, but this mismatch would make the next
exact-source update ambiguous.  Runtime application was stopped before any
app-layer change.

System package construction now fails if `unsquashfs` is unavailable instead
of silently skipping the embedded version and ABI check.  Normal package
generation must use the tools container.  A corrected System is built with
`PLUMOS_PIXEL2_VERSION=0.1.0-dev-4ddb809`; its package uses the live embedded
`0.1.0-dev` as the exact corrective source.
