# Pixel2 runtime USB charging recovery

Date: 2026-08-21
Status: revised fixtures pass; physical charge acceptance pending deployment

## Regression

With saved Wi-Fi credentials, System service `15-usb-host-reenumerate` treated
`usb/online=0` as permission to write `otg_mode=host`. Once forced, the PHY kept
the only Pixel2 connector as a VBUS source. Replacing the Wi-Fi dongle with a
charger did not restore sink/charger detection; shutdown removed the runtime
force and charging worked again.

This is the same circular ownership defect recorded during the earlier ADB
investigation: selecting host can itself keep `usb/online=0`, so that value
cannot prove that no upstream cable is present after the transition.

## Superseded extcon contract

The first correction required extcon `USB-HOST=1` before forcing host. A live
Pixel2 boot disproved that assumption: RTL8821CU `0bda:c820` and `wlan0` were
active in forced host mode while extcon still reported `USB-HOST=0`. Deploying
that gate would therefore disable cold-boot Wi-Fi. It was not accepted as the
final implementation.

## Corrected contract

- Saved credentials are intent to associate, not proof of physical USB host
  ownership.
- Cold-boot host probing requires `usb/online!=1` and an inactive BVALID bit
  while the PHY is in stock OTG mode. The PX30/RK3326 USB2 PHY UTMI status at
  `0xff2c0120` supplies BVALID bit 9. Host mode does not consult this bit because
  the port's own VBUS source also asserts it.
- A successful downstream enumeration retains host. Empty or failed probes
  return to OTG instead of leaving the connector as a VBUS source.
- Charger detection, no host cable, and explicit release write `otg` using the
  stock Rockchip PHY sysfs ABI. Stock Image, DTB, kernel, and initramfs remain
  unchanged.
- The existing V90S-derived blocking uevent monitor releases host on USB device
  removal and reconciles extcon/power-supply changes; no polling loop is added.
- FE Wi-Fi ON/scan/connect asks System for one bounded host probe when no USB
  device is enumerated. This is the safe explicit recovery path after OTG
  release; a missing dongle returns to OTG again.
- RTL8821CU storage identity `0bda:1a2b` removal is delayed and rechecked so its
  intended eject transition to `0bda:c811` is not mistaken for unplugging.

## Host acceptance

Fixtures cover BVALID/charger priority, bounded cold-boot and explicit probes,
empty-probe OTG release, immediate dongle release, delayed storage-mode release,
root-hub exclusion, and power-supply reconciliation. System rootfs and complete
app-layer script gates pass.

## Physical acceptance pending

After the battery has charged:

1. boot with RTL8821CU inserted and confirm saved Wi-Fi plus SSH;
2. remove the dongle, attach a charger without reboot, and confirm
   `/sys/class/power_supply/usb/online=1` plus increasing battery capacity;
3. remove the charger, reinsert the dongle, and confirm Wi-Fi/SSH recovery;
4. cold boot with the charger already attached and confirm charging continues.

## 2026-08-22 deployment race and correction

The signed Runtime transaction reached `healthy` at version
`0.1.0-dev-97bf49c`, but the following cold boot left the Wi-Fi dongle
unpowered. Offline state-partition evidence showed that the hardware and driver
had initially succeeded: `0bda:c820` enumerated at kernel time 5.78 seconds.
The asynchronous boot worker then unbound DWC2 at 6.26 seconds. Runtime treated
the resulting remove uevent as a physical unplug, released the PHY to OTG, and
a concurrent recovery probe changed the role again. Repeated host/OTG changes
prevented the adapter from remaining enumerated.

The corrected worker now serializes boot and FE probes with one runtime lock,
waits for natural enumeration after forcing host before considering DWC2
rebind, and marks an intentional controller transition. Runtime ignores USB
remove events carrying that transition marker. An actual removal during the
probe is still safe: the worker observes no downstream device and returns to
OTG itself. Fixtures reproduce late enumeration and concurrent probe attempts.
