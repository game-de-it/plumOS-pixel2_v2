# Pixel2 frontend input repair

Date: 2026-08-12
Scope: Pixel2 face-button contract, frontend action mapping, live deployment and
release-image validation

Note: this document records the live input repair performed during the
Linux 6.12 plumOS-owned-kernel bring-up. Decision 0004 later switches release
image generation back to the stock Pixel2 boot substrate. The physical button
mapping remains valid for the Pixel2 frontend and emulator configuration.

## Physical contract

Two Pixel2 units were tested with the same SD card. The first unit produced
ambiguous B-button evidence, so the second unit was used as the controlled
hardware check. The ordered second-unit capture is the authoritative contract:

1. physical A pressed three times
2. physical B pressed three times
3. physical B held for about three seconds

The resulting evdev and GPIO evidence was:

| Physical control | GPIO | Linux event code | Frontend action | RetroArch udev binding |
| --- | --- | --- | --- | --- |
| A | GPIO3 PD2 | `BTN_EAST` (`0x131`) | A / confirm | `input_a_btn = "1"` |
| B | GPIO3 PD1 | `BTN_SOUTH` (`0x130`) | B / back | `input_b_btn = "0"` |
| X | GPIO3 PA5 | `BTN_NORTH` (`0x133`) | X | `input_x_btn = "2"` |
| Y | GPIO3 PA6 | `BTN_WEST` (`0x134`) | Y | `input_y_btn = "3"` |
| Function | GPIO3 PC5 | `BTN_TRIGGER_HAPPY1` (`0x2c0`) | Function | `input_*_btn = "10"` if routed |

The key ordered log was pulled to:

`output/live/2026-08-12-input-map/unit2/input-map-unit2/ab-ordered-dump.log`

with SHA-256:

`9b798e28a6560d6c68a63531d83b03a0f9fbad2674df1608c469e3e5cda5e3b1`

The ordered log and paired GPIO sample were used to prove that both physical
switches generate events. Later frontend validation established the current
Pixel2 user-facing contract as `east-confirm`: physical A is `BTN_EAST`
(`305`) and physical B is `BTN_SOUTH` (`304`). The paired GPIO sample was
pulled to:

`output/live/2026-08-12-input-map/unit2/input-map-unit2/ab-gpio-sample.log`

with SHA-256:

`6e245507456762568b8ef5d1b276826f11ca5b655056d688f6aea1f92a22297b`

The paired IRQ counts moved from A=46/B=0 to A=83/B=17 during that ordered run.
This proves both switches work on the second unit; the current FE and emulator
contract is recorded in `/mnt/plumos/config/system/input-map.env`.

## Implementation

- `scripts/build-kernel.sh` keeps the pinned DTS GPIO/code pairs for A and B:
  `button-a` stays on `RK_PD1`/`BTN_SOUTH` and `button-b` stays on
  `RK_PD2`/`BTN_EAST`. The transform now only normalizes physical labels and
  still fails if the pinned source contract changes unexpectedly.
- `rootfs/pixel2/usr/lib/plumos/init.d/40-frontend` now prefers the stock
  Pixel2 joypad evdev names `pixel2_joypad` and `gamekiddy-joypad` before
  falling back to generic `gpio-keys`.
- `package/app-layer-pixel2/config/system/input-map.env` exports
  `PLUMOS_INPUT_AB_LAYOUT=east-confirm`, `PLUMOS_INPUT_A_CODE=305`, and
  `PLUMOS_INPUT_B_CODE=304`.
- `vendor/plumos-frontend/src/plumos_controller_ui.c` uses the shared
  `PLUMOS_INPUT_AB_LAYOUT` contract so the FE and emulator launchers agree on
  physical A/B semantics.
- `package/retroarch-pixel2/pixel2-joypad-udev.cfg` maps RetroArch's udev
  button index 1 to A and index 0 to B, matching the FE `east-confirm`
  contract and the physical report that B must not trigger RetroPad A. D-pad is
  not a button range on Pixel2; it is bound through `ABS_X`/`ABS_Y` axes.

## Generated-artifact validation

The source-level checks cover the kernel DTS transform, frontend environment,
Pixel2 input action mapping and RetroArch autoconfig:

```text
tests/test-kernel-scripts.sh
tests/test-app-layer-scripts.sh
```

The corrected generated artifacts were:

| Artifact | SHA-256 |
| --- | --- |
| `output/frontend/pixel2/plumos/bin/plumos-frontend-pixel2` | `0afdd7bbdf6a22f3f4c9331067681c94727d0a235dd9d82148d961bb1dd7dcba` |
| `output/app-layer/pixel2/plumos/manifest.json` | `7376bffb8cb33d6c6ed49a9587963889c60d79ead87e5e19dbe574c7c87c18fb` |
| `output/app-layer/pixel2/plumos/checksums.sha256` | `788eadc7a2ab2d00e922cbdebfeb54dd7b4134ec791c2694a0a557753c2656a4` |
| `output/kernel/pixel2/rk3326s-gkd-pixel2.dtb` | `7496b63430ffaeafc4d5c093b7608ebe04b5126a5bc1e8e63977fc68b6d0ece1` |

The complete app-layer was deployed to `/mnt/plumos` with `manifest.json`,
component manifests and `checksums.sha256` in the same update unit. The device
passed all four checksum gates before reboot:

```text
root_checksum=ok
frontend_checksum=ok
retroarch_checksum=ok
cores_checksum=ok
```

The corrected DTB was staged through `/state/plumos/deploy-input-fix`, verified
before copy, and installed to `/boot/rk3326s-gkd-pixel2.dtb`. The previous live
DTB remains backed up at:

`/boot/rk3326s-gkd-pixel2.dtb.bak-input-map-20260812`

After reboot, `/proc/device-tree/gkd-pixel2-joypad` reported:

| Node | Label | Code | GPIO pin |
| --- | --- | --- | --- |
| `button-a` | `A` | `0x130` | GPIO3 PD1 |
| `button-b` | `B` | `0x131` | GPIO3 PD2 |
| `button-x` | `NORTH` | `0x133` | GPIO3 PA5 |
| `button-y` | `WEST` | `0x134` | GPIO3 PA6 |

The post-deploy ordered dump was pulled to:

`output/live/2026-08-12-input-map/after-fix/input-map-after-fix/ab-dump.log`

with SHA-256:

`9457886eed8c6d1f5f0333c816d079ff464bda5800c3bebc18a08484756ca932`

It records physical A as three `code=304 action=A` press/release pairs and
physical B as three `code=305 action=B` press/release pairs.

## Stock-substrate input event selection repair

After the stock boot substrate migration and a power-management reboot cycle,
the frontend started on the wrong evdev node:

```text
/mnt/plumos/bin/plumos-frontend-pixel2 --renderer fbdev --fb /dev/fb0 --event /dev/input/event1
/dev/input/event1 gpio-keys
/dev/input/event2 pixel2_joypad
```

This made the FE appear alive while physical game controls no longer operated.
The root cause was that `40-frontend` matched `*gkd-pixel2*` and `*gamepad*`
but did not match the stock kernel's actual joypad name, `pixel2_joypad`.

The live FE was manually restarted on `/dev/input/event2` to restore operator
control, then `SYSTEM` was regenerated from commit `9662680` and deployed to
`/boot/SYSTEM`:

```text
format=plumos-pixel2-system-v1
source_ref=9662680
image_sha256=aefeb583398eb4ccdcb29db46df497560dafe6911af169661f8a57bdca67ea46
```

The previous live SYSTEM was retained as:

```text
/boot/SYSTEM.BAK-20260812-input-event
```

After reboot, the frontend selected the Pixel2 joypad on its own:

```text
/mnt/plumos/bin/plumos-frontend-pixel2 --renderer fbdev --fb /dev/fb0 --event /dev/input/event2
frontend=result-starting renderer=fbdev input=/dev/input/event2 fb=/dev/fb0
```

## Remaining physical gate

Raw frontend button mapping is now validated. The following behavior gates
remain intentionally unchecked until observed on the physical unit:

1. physical A opens the selected TOP, ROM or menu entry.
2. physical B returns to the previous screen.
3. START opens the Start menu and physical A confirms its entries.
4. Function produces the expected frontend action.
5. A/B/X/Y and the exit hotkey work inside RetroArch.
