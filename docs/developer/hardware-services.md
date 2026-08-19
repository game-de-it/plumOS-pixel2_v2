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
is below the maximum logical level. The internal RK817 speaker route applies a
configurable `0` through `+15 dB` software boost in `0.5 dB` steps after logical
volume scaling. Physical Pixel2 testing accepted `+15 dB` without audible
distortion, abnormal noise, or observed heating, so `+15 dB` is both the fresh
image default and the hard upper bound. Saturation prevents numeric wraparound,
but high average full-scale output still requires physical acceptance when the
audio path changes. The same physical test confirmed that logical volume `0`
still produces complete mute with the `+15 dB` calibration. The router reads
`/run/plumos/volume/speaker-boost-step` on every transfer, so
`plumos-volume-control speaker-boost apply 4.5` changes and persists gain
without rebuilding or restarting an already running emulator. Samples outside
the signed 16-bit range are saturated instead of wrapping. The boost does not
apply when the router selects a USB audio card.

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

Power is read from the stock `rk805 pwrkey` input (`/dev/input/event0` on the
validated Pixel2). The hardware-key service remains running after the FE is
stopped so the same power menu can overlay RetroArch, PicoArch, standalone
emulators, and Apps. The overlay pauses only active display owners and restores
exactly those processes on cancel or sleep return.

DraStic uses a validated companion-process rule because its AArch64 DRM
`runner` and armhf emulator core are siblings under the same launcher. When the
runner owns the display, the service verifies the recorded core executable and
common parent before pausing both. It resumes the runner first, restores DRM
master and planes, and then resumes the core. This prevents emulation and audio
from continuing invisibly during standby without allowing a stale PID record
to target an unrelated process.

The stock kernel advertises `freeze mem`, but the USB-powered validation unit
returned `EBUSY` before either suspend state began. The helper therefore tries
the requested kernel state first and falls back to Pixel2 software standby.
Software standby keeps the display owner paused, blanks the backlight, and uses
the next Power press as a wake-only event. The normal FE suppresses that queued
wake event so it cannot immediately reopen the menu.
