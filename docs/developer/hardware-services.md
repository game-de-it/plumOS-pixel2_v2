# Hardware Services

## Input

The Pixel2 joypad event is `pixel2_joypad`. The shared input-map contract lives
at:

```text
/mnt/plumos/config/system/input-map.env
/mnt/plumos/config/system/input-map.json
```

Known face-button contract:

| Physical | evdev code | RetroArch udev button |
| --- | ---: | ---: |
| A | 305 | 1 |
| B | 304 | 0 |
| X | 307 | 2 |
| Y | 308 | 3 |

`PLUMOS_INPUT_AB_LAYOUT=east-confirm` makes the physical A button confirm in
the FE while keeping RetroArch and standalone launchers on the shared contract.
The D-pad is exposed as evdev keys (`BTN_DPAD_UP/DOWN/LEFT/RIGHT`) and maps to
RetroArch udev buttons 10/11/12/13. The kernel capabilities also advertise
`ABS_X`/`ABS_Y`, but physical D-pad presses were validated as `EV_KEY` events,
so RetroArch disables analog-to-digital D-pad mode for Pixel2.

Pixel2 uses busybox `mdev`, not a full udev daemon. Runtime launchers that rely
on libudev joystick discovery should run `plumos-ensure-udev-input-db` before
starting the emulator. It writes the minimal `/run/udev/data/cMAJ:MIN`
properties required for `pixel2_joypad` to enumerate as
`ID_INPUT_JOYSTICK=1`.

## Global Volume and Brightness

The hardware-key daemon is based on the MF/V90S service contract:

- plain volume keys adjust logical volume;
- SELECT + volume keys adjust backlight brightness;
- repeated holds repeat at a bounded interval;
- the final value is persisted after idle.

Pixel2 volume is currently `pixel2-state-only` because the RK817 mixer command
backend is not finalized. The runtime volume value is applied to emulator audio
by the Pixel2 `plumos_output` ALSA router as software gain for the RK817 output,
so global volume keys affect RetroArch/libretro playback without depending on a
device-specific mixer control. Brightness uses the kernel PWM backlight sysfs
path:

```text
/sys/class/backlight/backlight/brightness
```

## Display

The FE uses fbdev on `/dev/fb0` with `PLUMOS_FBDEV_ROTATION=ccw`. Brightness is
20 logical steps mapped to the 0..255 hardware range.

## Audio

The RK817 ALSA card is visible as `rockchiprk817`. RetroArch/libretro launches
use the Pixel2 `plumos_output` ALSA router, which opens the physical RK817 PCM
through the generated app-layer ALSA config, mirrors V90S-style immediate
runtime volume reads, and applies software gain when `/run/plumos/volume/current`
is below the maximum logical level.

Hardware mixer control remains a future enhancement for non-router clients, but
the plumOS-managed emulator path should use the router so volume keys work
during gameplay.

## Power

Reboot uses the stock-kernel sysrq path. Shutdown uses RK817 PMIC `DEV_OFF`:

```text
i2c bus 0, addr 0x20, reg 0xf4, bit 0
```

The FE power action path has been validated for reboot. FE shutdown has been
validated through dry-run plus the RK817 helper path; actual FE-menu poweroff
is still a terminal physical-device test.
