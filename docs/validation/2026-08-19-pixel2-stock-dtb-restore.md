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
