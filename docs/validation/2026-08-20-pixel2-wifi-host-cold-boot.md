# Pixel2 Wi-Fi host cold-boot recovery

Date: 2026-08-20

## Symptom

After a System update reboot with an RTL8821CU dongle left inserted, DWC2
registered its root hub but did not enumerate the downstream device. The first
Wi-Fi recovery ended with `stage=no_usb_wifi_dongle`. Physically unplugging and
reinserting the dongle generated USB/net add events and recovered the saved
SSID, DHCP address, and SSH without another OS reboot.

The missing contract was therefore cold-boot discovery of an already-inserted
dongle, not runtime hotplug recovery.

## Evidence

The unmodified stock DTB reports `dr_mode=otg`. On the running device the
Rockchip PHY exposed a writable `otg_mode` sysfs attribute and extcon reported:

```text
USB=0
USB-HOST=1
USB_VBUS_EN=1
```

Changing `otg_mode` from `otg` to `host` kept the established WLAN route,
address `192.168.10.110`, and SSH session alive. The Rockchip kernel driver
documents `host` as its supported sysfs value and uses it to set IDDIG host
mode.

## Contract

Pixel2 keeps the stock DTB and kernel boot sequence. When saved Wi-Fi
credentials exist, the upstream USB/charger signal is not active, and extcon
reports a physical `USB-HOST=1` attachment, `15-usb-host-reenumerate` writes
`host` through the stock Rockchip sysfs ABI before it checks the downstream
device or rebinds DWC2.

The operation is idempotent and asynchronous. It is skipped for an upstream
USB cable, never resets an already enumerated downstream device, and logs a
warning rather than blocking boot if the stock sysfs ABI is unavailable.

Runtime unplug/reinsert remains handled by the V90S-derived blocking kernel
uevent monitor. Dongle removal writes `otg` through the same stock PHY ABI;
charger and extcon change events reconcile the role again. No polling loop or
custom DTB is introduced.

## Acceptance

Host fixtures verify OTG-to-host, upstream-cable skip, and already-enumerated
dongle paths. Physical acceptance requires leaving the RTL8821CU inserted,
performing a cold boot, and confirming that the saved SSID, DHCP address, SSH,
and later unplug/reinsert recovery all return without manual FE Wi-Fi toggles.
It also requires replacing the dongle with a charger, confirming battery charge
while Linux remains running, and then confirming Wi-Fi after reinsertion.
