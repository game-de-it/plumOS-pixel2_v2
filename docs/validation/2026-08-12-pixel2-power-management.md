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

The first 2026-08-22 implementation wrote `BOOT_CHARGING` directly and then
used sysrq `b`. Physical testing showed that the OS booted normally. This is
consistent with the stock kernel reboot-mode notifier running later and
writing `mode-normal`, and also repeated the earlier documented result where a
pre-written charge flag followed by forced poweroff returned to the OS.

Pixel2 therefore separates the two Shutdown cases as follows:

- with an attached charger, call Linux `RESTART2` with the exact command
  `charge`; the built-in stock `CONFIG_REBOOT_MODE` and
  `CONFIG_SYSCON_REBOOT_MODE` drivers then resolve DT `mode-charge` and write
  `BOOT_CHARGING` during the kernel restart notifier sequence;
- without a charger, retain RK817 `DEV_OFF` for a true cold power-off;
- if the restricted reboot helper is missing, rejected, or unexpectedly
  returns, fall back to RK817 `DEV_OFF` instead of risking an ordinary OS
  reboot.

Charger detection accepts `battery/status=Charging`, an online power-supply,
or positive battery current. At a full battery, where those signals may be
idle, the same stock-OTG BVALID signal used by System is accepted only when no
downstream USB device is enumerated and the PHY is not forced to host. This
prevents a Wi-Fi dongle or an unplugged full battery from selecting charging
mode. Fixtures cover active charging, full-battery BVALID, and full-battery
charger-absent behavior. The helper accepts only `charge`, is compiled for
AArch64 in the frontend component, and is covered by the component checksum.
Physical plugged-Shutdown acceptance passed for the corrected `RESTART2` path.

### Corrected Runtime deployment

The corrected implementation was built from `16150d5` as a signed Runtime
delta against the installed `0.1.0-dev-724965f` generation:

```text
package=plumos-pixel2-runtime-0.1.0-dev-16150d5.tar.gz
sha256=7db1bace357f7430b47bcf1156d216a016589561c28848145d9b7bf44bb3b268
payload_files=10
deleted_files=0
```

Host and live-device updater inspection both accepted the Ed25519 signature,
Pixel2 device/vendor, named ABI, and exact source version. The normal Runtime
transaction returned with FE ready, no request or pending marker, and:

```text
Runtime=0.1.0-dev-16150d5
System=0.1.0-dev-4993d8c
last-result=runtime_healthy
plumos-reboot-mode=df1d479d6c14a00c1275482326155c043b8b712c11aac906f1a9f5940028ca09
plumos-safe-shutdown=780aea75e758ac6a315966a537bdcfaf14e6961161ae23470814650f88225fd1
```

Frontend component and full Runtime checksum verification passed after the
reboot.

The operator then replaced the Wi-Fi dongle with the charger and selected FE
Shutdown. Without removing or reinserting the cable, Pixel2 entered the stock
charging screen instead of booting the OS. This closes the plugged-Shutdown
acceptance that the direct register-write implementation had failed. The
operator then repeated FE Shutdown without a charger and confirmed complete
power-off. The charge-present `RESTART2("charge")` branch and charger-absent
RK817 `DEV_OFF` branch are both physically accepted.
