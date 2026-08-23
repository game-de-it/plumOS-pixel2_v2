# Pixel2 sleep charging and Wi-Fi recovery validation

## Scope

Pixel2 has one dual-role USB port, so charging and USB Wi-Fi cannot be used at
the same time. This validation covers both charging entry orders and the
charger-to-Wi-Fi handoff after wake:

1. connect the charger before selecting Sleep;
2. select Sleep without a cable, then connect the charger while asleep;
3. wake with the physical Power button and replace the charger with the Wi-Fi
   dongle without rebooting.

The observation helper only sampled battery, PMIC, USB-role, and kernel state.
It did not wake the display or provide a product workaround.

## Implemented contract

Commit `e256391` keeps the display off when a charger wakes the stock kernel
from `mem`. If the device was unpowered before suspend and charging is detected
after kernel resume, `plumos-safe-shutdown` defers the visible resume and enters
software standby. Only the next physical Power press restores the display and
the paused display owner.

Commit `b66efd3` covers the following single-port handoff. A bounded worker runs
only after Sleep resume, for at most 120 seconds at 3-second intervals. It asks
the existing System USB-host helper to probe, starts the existing Wi-Fi recovery
path only after a WLAN interface appears, and stops immediately after IPv4 is
available. It does not add normal-boot or permanent polling.

## Charging results

| entry order | display while asleep | charging evidence | wake | result |
|---|---|---|---|---|
| charger before Sleep | remained fully off | operator battery report 15% to 72%; software standby lasted 2626 seconds | physical Power | PASS |
| charger after Sleep | remained fully off after charger-induced kernel wake | `Charging`; final observed current about +0.633 A; deferred software standby lasted 2791 seconds | physical Power | PASS |

The post-plug log shows the intended two-stage wake:

```text
power=charger-detected source=battery-status value=Charging
sleep=resume-deferred reason=charger-connected fallback=software
sleep=software-returned seconds=2791
sleep=resume-deferred-complete reason=software-standby-release
```

The battery percentage during the short post-plug observation was not used as
the sole charging proof because the RK817 fuel gauge moved from 95% to 93% while
status remained `Charging`. The simultaneous positive current, PMIC state, and
the separate 15% to 72% operator observation establish actual charging.

## Charger-to-Wi-Fi handoff

Runtime `0.1.0-dev-b66efd3` was deployed as a six-file frontend transaction:

```text
bin/plumos-safe-shutdown SHA-256:
8acb6a7f7796adbee4482047860caba026f556227a74801a9584df83617e078b
frontend checksum entries: 199 PASS
app-layer checksum entries: 11267 PASS
runtime_verify=result-ok
```

After physical Power wake, the charger was replaced with the UGREEN AC650 /
RTL8811CU dongle. The first probes still saw the charger or an empty bus. The
tenth bounded attempt forced host mode, re-enumerated the controller, and used
the normal Wi-Fi recovery path:

```text
sleep=wifi-resume-recovery-attempt attempt=10 window_sec=120
service=usb-host-reenumerate stage=host-mode-forced source=sysfs
service=usb-host-reenumerate result=reenumerated elapsed=1
service=wifi-recovery recover_begin
result=connected
ip=192.168.10.107
sleep=wifi-resume-recovery-result result=connected attempts=10
```

Final live state was `0bda:c811`, `wlan0` up, and IPv4
`192.168.10.107/24`. The operator confirmed that Wi-Fi returned after Sleep
without an OS reboot. This closes both the screen-off charging gate and the
late Wi-Fi swap regression.

## Evidence

- `output/validation/2026-08-24-pixel2-sleep-charging/device-b66efd3/device-state.txt`
- `output/validation/2026-08-24-pixel2-sleep-charging/device-b66efd3/power.log`
- `output/validation/2026-08-24-pixel2-sleep-charging/device-b66efd3/usb-host-reenumerate.log`
- `output/validation/2026-08-24-pixel2-sleep-charging/device-b66efd3/wifi-recovery.log`
- `output/validation/2026-08-24-pixel2-sleep-charging/device-b66efd3/20260823-183618-observe-plugged/`
- `output/validation/2026-08-24-pixel2-sleep-charging/device-b66efd3/20260823-201934-observe-postplug/`

These files are validation artifacts and are not included in the release image.
