# Pixel2 runtime USB charging recovery

Date: 2026-08-21
Status: host fixtures pass; physical acceptance pending battery charge

## Regression

With saved Wi-Fi credentials, System service `15-usb-host-reenumerate` treated
`usb/online=0` as permission to write `otg_mode=host`. Once forced, the PHY kept
the only Pixel2 connector as a VBUS source. Replacing the Wi-Fi dongle with a
charger did not restore sink/charger detection; shutdown removed the runtime
force and charging worked again.

This is the same circular ownership defect recorded during the earlier ADB
investigation: selecting host can itself keep `usb/online=0`, so that value
cannot prove that no upstream cable is present after the transition.

## Corrected contract

- Saved credentials are intent to associate, not proof of physical USB host
  ownership.
- Cold-boot host force requires both `usb/online!=1` and extcon
  `USB-HOST=1`.
- Charger detection, no host cable, and explicit release write `otg` using the
  stock Rockchip PHY sysfs ABI. Stock Image, DTB, kernel, and initramfs remain
  unchanged.
- The existing V90S-derived blocking uevent monitor releases host on USB device
  removal and reconciles extcon/power-supply changes; no polling loop is added.
- RTL8821CU storage identity `0bda:1a2b` removal is delayed and rechecked so its
  intended eject transition to `0bda:c811` is not mistaken for unplugging.

## Host acceptance

Fixtures cover physical-host-gated cold boot, upstream charger priority,
host-cable absence, immediate dongle release, delayed storage-mode release,
root-hub exclusion, and extcon/power-supply reconciliation. System rootfs and
complete app-layer script gates pass.

## Physical acceptance pending

After the battery has charged:

1. boot with RTL8821CU inserted and confirm saved Wi-Fi plus SSH;
2. remove the dongle, attach a charger without reboot, and confirm
   `/sys/class/power_supply/usb/online=1` plus increasing battery capacity;
3. remove the charger, reinsert the dongle, and confirm Wi-Fi/SSH recovery;
4. cold boot with the charger already attached and confirm charging continues.
