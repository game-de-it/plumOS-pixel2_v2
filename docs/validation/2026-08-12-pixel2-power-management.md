# Pixel2 power management validation

Date: 2026-08-12
Scope: stock boot substrate reboot and RK817 shutdown behavior on physical
Pixel2 hardware while USB power/ADB is connected

## Result

Reboot is valid on the stock Pixel2 boot substrate. Shutdown must not use the
generic BusyBox/kernel `poweroff -f` path on Pixel2; the device returns to the
OS while USB is online. The working Pixel2 shutdown primitive is RK817 PMIC
`DEV_OFF`, written to `SYS_CFG(3)` at I2C bus `0`, address `0x20`, register
`0xf4`, bit `0`.

The validated behavior is:

| Path | Result |
| --- | --- |
| `/mnt/plumos/bin/plumos-safe-shutdown --reboot` | PASS: ADB returned and FE restarted |
| `devmem 0xff010200 32 0x5242c30b; poweroff -f` | FAIL: OS restarted |
| `busybox poweroff` | FAIL: returned immediately |
| `busybox poweroff -f` | FAIL: OS restarted |
| `i2cset -f -y 0 0x20 0xf4 0x59` | PASS: ADB stayed offline and the screen turned off |
| deployed `/mnt/plumos/bin/plumos-safe-shutdown --shutdown` | PASS: ADB stayed offline for 60 seconds after RK817 `DEV_OFF` |

## Evidence

The live device was on the stock kernel:

```text
Linux plumos-pixel2 5.10.198 #1 SMP Thu Jan 16 15:55:36 HKT 2025 aarch64 GNU/Linux
```

USB power was online:

```text
/sys/class/power_supply/usb/online = 1
/sys/class/power_supply/ac/online = 0
```

The stock DTB exposes the reboot mode register at PMU GRF offset `0x200`.
`mode-charge` was verified as `0x5242c30b`, but using that before
`poweroff -f` still restarted into the OS:

```text
output/live/2026-08-12-power-20260812-073802/charge-mode-poweroff-test.log
poll=9 status=adb-returned
```

BusyBox `poweroff` without `-f` did not request a shutdown in the current
plumOS PID 1 environment:

```text
output/live/2026-08-12-power-20260812-073915/busybox-poweroff-no-force.log
poll=1 status=adb-online
```

The RK817 PMIC direct `DEV_OFF` test stayed offline for the full 45 second
monitoring window:

```text
output/live/2026-08-12-power-20260812-074005/rk817-dev-off-direct-force.log
before=0x58
write=0x59
result=adb-stayed-offline duration=45s
```

The user then reported that the screen was off. This confirms that RK817
`DEV_OFF` is the correct Pixel2 power-off primitive.

After committing the fix as `9314462`, the rebuilt app-layer was deployed to
the live device as a checksum-consistent unit:

```text
bin/plumos-safe-shutdown
manifest.json
checksums.sha256
```

The live device accepted the updated root checksum:

```text
sha256sum -c /mnt/plumos/checksums.sha256
```

The deployed reboot validation was captured at:

```text
output/live/2026-08-12-power-20260812-074528/deployed-safe-shutdown-reboot.log
```

The script stopped the frontend, unmounted user FAT mounts, rebooted, and ADB
returned after 10 polling seconds:

```text
power=stop-process name=frontend pid=325 signal=TERM
power=unmount-result target=/roms rc=0
power=unmount-result target=/mnt/plumos-user rc=0
poll=10 status=adb-returned
root_checksum=ok
```

The frontend was then confirmed running again as:

```text
/mnt/plumos/bin/plumos-frontend-pixel2 --renderer fbdev --fb /dev/fb0 --event /dev/input/event1
```

The deployed shutdown validation was captured at:

```text
output/live/2026-08-12-power-20260812-074600/deployed-safe-shutdown-poweroff.log
```

The script stopped the frontend, unmounted user FAT mounts, wrote RK817
`DEV_OFF`, and ADB stayed offline for the full 60 second monitoring window:

```text
power=stop-process name=frontend pid=337 signal=TERM
power=unmount-result target=/roms rc=0
power=unmount-result target=/mnt/plumos-user rc=0
power=shutdown-method method=rk817-dev-off bus=0 addr=0x20 reg=0xf4 value=0x59
result=adb-stayed-offline duration=60s
```

## Implementation

`package/app-layer-pixel2/bin/plumos-safe-shutdown` now:

1. logs power actions to `/state/plumos/logs/power.log`;
2. stops the frontend process from `/run/plumos/frontend.pid`;
3. unmounts `/roms` and `/mnt/plumos-user` before terminal power transition;
4. keeps the proven sysrq reboot path for reboot;
5. uses RK817 `DEV_OFF` through `i2cget -f`/`i2cset -f` for shutdown, with
   `busybox poweroff -f` only as an unreachable fallback.

The rebuilt app-layer passed checksum verification:

```text
app_layer_verify=result-ok root=output/app-layer/pixel2/plumos
```

Important generated SHA-256 values:

| File | SHA-256 |
| --- | --- |
| `output/app-layer/pixel2/plumos/bin/plumos-safe-shutdown` | `26c318400eca563ca89531d64a30ffc2b23b8ba38177723e1f91e1764db96879` |
| `output/app-layer/pixel2/plumos/checksums.sha256` | `6f279c2000b330bb092db887a5e2585e4e990faa41046ca280b77371965f4bc7` |
| `output/app-layer/pixel2/plumos/manifest.json` | `8d5dd08527d2c30f92deb316cd574d061245feeab1a688a66b3a84e2a9be7d9d` |

## Remaining gate

Validate reboot and shutdown from the FE Start menu on physical hardware. The
menu uses the same deployed `plumos-safe-shutdown` script that passed the ADB
validation above, but the frontend-initiated path still needs direct operator
confirmation.

## 2026-08-22 plugged-shutdown correction

The earlier RK817 `DEV_OFF` acceptance proved only that the screen and remote
transport stayed off. It did not verify the expected U-Boot charging LED and
battery animation while a charger remained inserted. Physical validation later
showed that direct `DEV_OFF` left both indications off until the cable was
removed and reinserted. The PMIC could charge after a new plug event, but the
shutdown path had not handed the already-present charger to U-Boot.

Rockchip's 5.10 `drivers/mfd/rk808.c` uses a full syscore shutdown sequence,
including RK817 sleep-pin power-down setup before its PMIC shutdown callback.
Rockchip U-Boot defines charging boot mode as `0x5242c30b`; its boot-mode setup
turns that value into the `charge` preboot command, and the captured stock
U-Boot DT enables `rockchip,uboot-charge-on`.

Pixel2 now separates the two Shutdown cases:

- with an attached charger, record `BOOT_CHARGING` and use the already-proven
  sysrq reboot path to enter the stock U-Boot charging UI;
- without a charger, retain RK817 `DEV_OFF` for a true cold power-off;
- if the charging boot-mode write fails, fall back to RK817 `DEV_OFF` instead
  of risking an ordinary OS reboot.

Charger detection accepts `battery/status=Charging`, an online power-supply,
or positive battery current. At a full battery, where those signals may be
idle, the same stock-OTG BVALID signal used by System is accepted only when no
downstream USB device is enumerated and the PHY is not forced to host. This
prevents a Wi-Fi dongle or an unplugged full battery from selecting charging
mode. Fixtures cover active charging, full-battery BVALID, and full-battery
charger-absent behavior. Physical plugged-Shutdown acceptance remains pending.
