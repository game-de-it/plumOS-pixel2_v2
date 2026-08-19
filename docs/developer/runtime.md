# Boot and Runtime Services

## Boot Flow

1. Stock RK3326 boot chain loads the stock kernel, DTB, and initramfs.
2. The stock initramfs mounts the boot volume and hands off to `/boot/SYSTEM`.
3. plumOS `/sbin/init` mounts proc/sys/dev, `/mnt/plumos`, `/mnt/plumos-user`,
   `/state`, and `/roms`, then brings up IPv4/IPv6 loopback.
4. init reserves the single Pixel2 USB port for USB host/Wi-Fi, migrates
   retired ADB settings, then starts network/frontend services.

## Service Order

```text
15-usb-host-reenumerate  bounded host-controller recovery for a saved dongle
20-usb-wifi  saved USB Wi-Fi configuration, if a dongle exists
30-ssh       compatibility slot; delegates policy to network services
35-network-services  saved SSH/FTP/SFTP/Samba state
40-frontend  app-layer selection, hardware keys, ROM scan, FE
```

On a fresh image, SSH defaults to ON. Pixel2 has one USB port and plumOS
reserves it for a USB Wi-Fi dongle. ADB, FunctionFS, the USB Mode selector,
and the recovery marker are not shipped. Existing `usb_mode`, `adb_enabled`,
and marker state is removed during migration without changing Wi-Fi
credentials. SSH uses the common plumOS
`root / plumos` initial credential, generated as a device-local salted shadow
entry. The public initial password can be changed without an OS update
overwriting it; public-key authentication remains available.

`40-frontend` does not hash the complete app layer during a normal boot. Signed
updates and live deployment verify the complete Runtime before reboot; a
healthy unchanged generation is selected using constant-time metadata presence
checks. If required metadata is absent, boot falls back to the rootfs seed
frontend.

A complete operator-requested audit remains available without putting it in the
normal startup path:

```sh
/usr/sbin/plumos-system-update verify-runtime
```

An update request, unconfirmed Runtime, or interrupted transaction still enters
the Python updater before services start. With none of those states present,
init skips the updater entirely.

## Foreground Lifecycle

The frontend normally owns `/dev/fb0` and `/dev/input/event2`. Launchers must
stop or suspend frontend ownership before starting an emulator/app and restore
one frontend instance afterward. This remains a required real-device validation
item for Pixel2.

## Global Hardware Keys

`plumos-hardware-keys-service` starts `plumos-hardware-keys`. The daemon reads:

- `pixel2_joypad` for `BTN_SELECT`;
- `gpio-keys` for `KEY_VOLUMEUP` and `KEY_VOLUMEDOWN`.

Volume keys call `plumos-volume-control runtime-up/down`. Holding SELECT while
pressing volume calls `plumos-display-control runtime-up/down`. Completed
adjustments are persisted after a short idle delay.

## Runtime Logs

| Area | Path |
| --- | --- |
| init | `/mnt/plumos/logs/init.log` |
| frontend | `/mnt/plumos/logs/frontend.log` |
| USB Wi-Fi | `/state/plumos/logs/usb-wifi.log` |
| hardware keys | `/run/plumos/hardware-keys/service.log` |
| power | `/state/plumos/logs/power.log` or FE-provided app-layer log |

## Safe Power Actions

`plumos-safe-shutdown` stops the hardware-key service and frontend, unmounts
content/user mounts where possible, syncs, and then:

- reboots via sysrq `b` / BusyBox fallback;
- shuts down through RK817 `DEV_OFF`, then BusyBox poweroff fallback.

The physical `rk805 pwrkey` is owned continuously by the hardware-key service.
While the normal FE owns the display, the FE opens the power menu directly.
When an emulator or App owns the display, the service starts
`plumos-power-menu-overlay`, pauses only the current display owners, and uses
the same Sleep/Reboot/Shutdown/Cancel menu. Reboot and shutdown resume owners
before TERM so runtimes can save normally.

Sleep first requests the stock kernel's `mem` state. If the vendor kernel
rejects entry, Pixel2 falls back to software standby: the foreground owner
remains paused, the backlight is set to zero, and the next physical Power press
wakes the device without opening a second power menu. Resume reapplies display,
volume state, and the RK817 `Speaker`/`Headphone` route switches.
