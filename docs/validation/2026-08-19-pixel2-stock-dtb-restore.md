# Pixel2 exact-stock runtime DTB restoration

Date: 2026-08-19
Status: host implementation complete; physical ADB and Wi-Fi acceptance pending

## Reason

Pixel2 originally retained the stock kernel and initramfs but later generated a
runtime DTB with one additional property:
`/usb@ff300000/vbus-supply = <&OTG_SWITCH>`. The addition was introduced to let
a DWC2 unbind/bind power-cycle an already inserted Wi-Fi dongle.

ADB had previously passed a physical cable replug with the exact stock DTB.
Persistent ADB `waiting` failures appeared after the shared-port Wi-Fi work,
including the runtime-DTB VBUS linkage. The DTB change is therefore removed as
an explicit experiment boundary rather than treated as part of the stock boot
substrate.

## Implemented contract

- `artifacts/vendor/pixel2-stock/boot/rk3326s-gkd-pixel2.dtb` is copied directly
  to the generated `PLUMOS_BOOT` filesystem.
- The DTB builder and one-property diff gate are removed.
- The image manifest records `runtime_dtb_policy=exact-stock`.
- The image verifier compares the extracted runtime DTB byte-for-byte with the
  checksum-registered stock artifact.
- ADB mode remains exclusive: the Wi-Fi boot and USB-host re-enumeration paths
  must not unbind or bind `ff300000.usb` while ADB owns the port.

Registered stock runtime DTB identity:

```text
sha256=a7a438f705f994a9f333b2f334a803d47bc00cae6ed4556d51c730604452757a
```

## Physical acceptance order

1. Install the exact stock DTB without changing System, Runtime, user data, or
   saved network configuration.
2. Cold boot in USB Mode ADB with the Mac cable attached.
3. Require a real `adb devices -l` transport and working `adb shell`; frontend
   `RUNNING` alone is not acceptance.
4. Physically unplug/replug once and require the same ADB transport to return.
5. Switch explicitly to USB Mode Wi-Fi, insert the tested RTL8821CU dongle, and
   check enumeration, driver load, saved SSID association, DHCP, and SSH.
6. Repeat Wi-Fi from a full power-off cold boot. If an inserted dongle needs an
   additional VBUS action, recover the stockOS userspace sequence before
   considering any DTB change.

No physical result is claimed until those gates are observed on the device.

## Offline deployment to the existing card

The 62.5 GB Pixel2 card was identified as `/dev/disk4`; its mounted labels were
`PLUMOS_BOOT` and `PLUMOS_USER`. Before deployment, the live runtime DTB was the
former VBUS-linked artifact:

```text
before_sha256=89a32c94ebfae5983b1cf98209aaf8f11a6d8d2f7d29d66008d35125a2e328dc
```

The card already retained `rk3326s-gkd-pixel2.dtb.stock-a7a438f7`, and that
backup matched the registered host stock DTB. The active patched DTB was also
copied to the host evidence directory before replacement. The registered host
artifact was copied to a temporary file on `PLUMOS_BOOT`, verified, and renamed
over the runtime DTB. Final readback was byte-identical:

```text
after_sha256=a7a438f705f994a9f333b2f334a803d47bc00cae6ed4556d51c730604452757a
cmp=exact-stock
```

Before/after inventories differed only at
`rk3326s-gkd-pixel2.dtb`. `Image`, `SYSTEM`, System A/B slots, Runtime,
`PLUMOS_USER`, ROMs, BIOS, settings, and saved network state were not changed.
`diskutil verifyVolume /dev/disk4s1` completed with `fsck_msdos` exit code zero,
then all card volumes were unmounted successfully.

## First exact-stock physical boot

The card was returned to the Pixel2 and booted with the Mac USB cable and no
Wi-Fi dongle. The frontend reported ADB `waiting`. On the Mac, `adb devices -l`
was empty and neither `system_profiler SPUSBDataType` nor the IOUSB registry
contained a Pixel2/plumOS device or `18d1:4ee7` parent device.

Result: **ADB FAIL**. Restoring the exact stock DTB did not by itself restore
USB enumeration. This failure is below the ADB protocol transport: the Mac did
not receive even the gadget parent device. The one-property DTB change is no
longer required to reproduce the defect and is therefore not accepted as its
sole cause.

The next diagnostic must preserve this single-owner boot and recover the
persistent device log from the SD card. It must not add another Off/Wi-Fi/ADB
toggle or automatic recovery action before that evidence is collected.

Wi-Fi acceptance with the stock DTB remains pending.

## Persistent log diagnosis

The post-failure card was captured read-only under
`output/live/2026-08-19-pixel2-state-capture/capture.fnZbsu`; its complete
`CAPTURE.sha256` passed. The active System and Runtime were both
`0.1.0-dev-dacbc83`, saved USB mode was `adb`, and the exact-stock DTB remained
installed.

The latest boot reached FunctionFS bind but never FunctionFS enable:

```text
udc=result-found name=ff300000.usb state=not attached
FUNCTIONFS_BIND
result=started udc=ff300000.usb
```

Earlier bounded handoff captures retained on the same card prove that this was
not an absent cable or charger-only cable. On physical insertion, the Rockchip
PHY signals changed to ID=1, BVALID=1 and USB online=1. After DWC2 rebind they
remained valid, but the controller still reported `is_a_peripheral=0`, speed
`UNKNOWN`, and UDC `not attached`. Kernel output also registered a DWC2 host
root hub and reported an endpoint disable while in host mode.

The stock DTB declares `dr_mode = "otg"` but has no `usb-role-switch` property.
The stock 5.10.198 DWC2 driver only registers `/sys/class/usb_role/*/role` when
that property exists. The historical plumOS role loop therefore matched no
files and did not request device mode. The remaining fault is a DWC2 OTG
transition stuck in host mode, below adbd and FunctionFS. This also explains
why changing monitors, endpoint I/O and the Mac ADB server did not repair the
parent USB enumeration.

## Explicit stock-compatible role correction

`plumos-pixel2-usb-role` now performs the same bounded DWC2 force-mode register
operation used by the stock kernel driver's role-switch implementation:

- ADB start preserves all `GUSBCFG` fields, clears `FORCEHOSTMODE`, sets
  `FORCEDEVMODE`, and waits at most 220 ms for device mode before creating the
  gadget.
- ADB stop clears both force bits so the exact-stock DTB, PHY and OTG logic
  resume automatic ownership for Wi-Fi host operation.
- No DTB property, stock `Image`, kernel module, cable monitor, transport
  watchdog or timed recovery loop is added.

Host tests use a fake MMIO backend and verify device force, status reporting,
and release to automatic OTG selection. Physical acceptance remains required:
cold boot with a Mac cable, a real `adb shell`, one cable replug, then explicit
ADB to Wi-Fi switching with the tested RTL8821CU.

## Offline device-role deployment

Implementation commit `8f3e5f5` produced System
`0.1.0-dev-8f3e5f5`. Both A/B rootfs verifiers passed. The signed update
package is retained at
`output/live/2026-08-19-pixel2-adb-device-role/`:

```text
package_sha256=e1fd8ca36abe76e079c62a1408962b8a7dcf287f0cfb1e3c5f312fbeedbc4626
system_sha256=dcfb7fea26b7b1892df530e901356cd96a1cc504d310681b729ea134419a31f8
source_version=0.1.0-dev-dacbc83
version=0.1.0-dev-8f3e5f5
signature=verified
```

The card remained on active slot B from the preceding persistent capture.
Before writing, all five signed slot-B files from `dacbc83` were copied to both
the host evidence directory and
`/offline-backup-system-b-dacbc83-pre-device-role/` on `PLUMOS_USER`; the two
backup sets are byte-identical. Only those five slot-B files were staged,
compared and renamed into place. Final readback passed the slot checksum,
Ed25519 signature and manifest/payload agreement. Slot A remained
`0.1.0-dev-626a1e8`.

The stock `Image` remained SHA-256 `853eb041...` and the exact-stock runtime
DTB remained `a7a438f7...`. Runtime, dispatcher, user configuration, ROMs,
BIOS, saves and installed ports were not changed. `fsck_msdos -n` completed
with exit code zero.

The physical boot of this generation failed. The frontend remained at
`waiting`, `adb devices -l` was empty, and macOS again received no
`18d1:4ee7` parent device. The direct `GUSBCFG.FORCEDEVMODE` experiment is
therefore **not accepted** as a repair. Its persistent register/result log must
still be recovered from the SD, but further role experiments are paused in
favor of reconstructing the previously working exact-stock ADB lifecycle.
