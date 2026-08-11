# Boot and Runtime Services

## Boot Flow

1. Stock RK3326 boot chain loads the stock kernel, DTB, and initramfs.
2. The stock initramfs mounts the boot volume and hands off to `/boot/SYSTEM`.
3. plumOS `/sbin/init` mounts proc/sys/dev, `/mnt/plumos`, `/mnt/plumos-user`,
   `/state`, and `/roms`.
4. init starts ADB, USB Wi-Fi, SSH, and frontend services from
   `/usr/lib/plumos/init.d/`.

## Service Order

```text
10-adbd      USB FunctionFS/configfs ADB maintenance path
20-usb-wifi  saved USB Wi-Fi configuration, if a dongle exists
30-ssh       Dropbear when authorized_keys exists
40-frontend  app-layer verification, hardware keys, ROM scan, FE
```

`40-frontend` verifies `/mnt/plumos/checksums.sha256` before using the app
layer. If verification fails, it falls back to the rootfs seed frontend.

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
| ADB | `/state/plumos/logs/adbd.log` |
| USB Wi-Fi | `/state/plumos/logs/usb-wifi.log` |
| hardware keys | `/run/plumos/hardware-keys/service.log` |
| power | `/state/plumos/logs/power.log` or FE-provided app-layer log |

## Safe Power Actions

`plumos-safe-shutdown` stops the hardware-key service and frontend, unmounts
content/user mounts where possible, syncs, and then:

- reboots via sysrq `b` / BusyBox fallback;
- shuts down through RK817 `DEV_OFF`, then BusyBox poweroff fallback.
