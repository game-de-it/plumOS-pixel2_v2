# Pixel2 global power menu and sleep validation

Date: 2026-08-15 to 2026-08-16
Final tested Runtime: `0.1.0-dev-12b809b`

## Result

Pixel2 now has one power contract across the FE, RetroArch, PicoArch,
standalone emulators, and Apps. The stock `rk805 pwrkey` is monitored by the
always-running hardware-key service. The normal FE handles Power directly;
outside the FE, the service starts the same FE in power-overlay mode after
pausing only the processes that own `/dev/fb0`, DRM, Mali, or `/dev/disp`.
For an active DRM owner, the service temporarily acquires a duplicate of the
owner's DRM file descriptor, disables its active KMS planes, drops DRM master,
and gives the overlay control of the display. It restores DRM master and the
exact plane state before resuming the owner.

DraStic is the one two-process exception: its native AArch64 `runner` owns DRM,
while the armhf emulator core advances the game without owning a display file
descriptor. The service validates their executable paths, runtime PID record,
and common launcher parent, then pauses and resumes both processes as one
display owner. A stale or unrelated PID is never signalled.

The menu exposes Sleep, Reboot, Shutdown, and Cancel. Cancel and sleep return
only resume processes that this overlay stopped. Reboot and shutdown resume
the owner before the terminal helper sends TERM so the runtime still has an
opportunity to save.

## Pixel2-specific findings

The validated input and power interfaces are:

```text
/dev/input/event0  rk805 pwrkey
/dev/input/event1  gpio-keys
/dev/input/event2  pixel2_joypad
/sys/power/state   freeze mem
/sys/power/mem_sleep  s2idle [deep]
```

The MF RK817 resume controls cannot be copied by name. Pixel2 exposes these
simple mixer elements instead:

```text
Headphone  playback switch
Speaker    playback switch
BCLK Ratio playback volume
Headset Mic playback switch
Main Mic    playback switch
```

`plumos-rk817-resume` therefore uses the managed ALSA configuration, carries an
app-layer-relative RUNPATH for `libasound.so.2`, and reapplies the actual
Pixel2 `Speaker` and `Headphone` switches. Both `arm` and `rearm` returned
success on the physical device.

## Kernel suspend boundary

The USB-powered stock 5.10.198 kernel advertised both `mem` and `freeze`, but
direct writes of either state returned errno 16 (`EBUSY`) before suspend entry:

```text
state=mem write_result=-1 errno=16 error=Device or resource busy
state=freeze write_result=-1 errno=16 error=Device or resource busy
suspend_stats.success=0
suspend_stats.fail=0
```

Stopping/unbinding ADB and temporarily disabling the RK817 USB, AC, and battery
wakeup flags did not change this boundary. No `PM: suspend entry` was emitted.
The Runtime therefore keeps kernel sleep as the first choice, but automatically
falls back to software standby when the stock kernel refuses entry.

Software standby:

1. presents black into both Pixel2 DRM buffers;
2. sets connector DPMS off and detaches the CRTC so the DSI panel fully powers
   down instead of retaining the last menu frame;
3. saves and blanks the Pixel2 backlight without changing persistent settings;
4. waits for the hardware-key service to consume the next Power press;
5. suppresses the duplicate queued Power event in the normal FE;
6. reconnects the CRTC/mode and restores the exact backlight, display/volume
   state, RK817 route, and display owner.

Writing `0` to the fbdev/backlight interfaces was not sufficient after the DRM
frontend became the scanout owner. `/dev/fb0` could be black while the visible
DRM plane retained the power menu, and a black DRM frame still left the DSI
panel faintly lit. The two-stage DRM black present plus connector/CRTC power
transition is therefore part of the Pixel2 sleep contract.

USB power changes are observed through `/sys/class/power_supply/usb/online`.
The hardware-key service ignores PMIC Power events for 1500 ms after a cable
transition. On reconnect it asynchronously requests the policy-aware ADB
`replug` action after 2000 ms. That action performs a bounded UDC rebind and
falls back to one clean adbd restart, allowing ADB to re-enumerate without
waking the display. Mutating ADB actions are serialized so reconnect and the
startup watchdog cannot launch competing daemons.

This provides a working sleep/wake interaction without replacing the stock
kernel. It is not equivalent to SoC/RAM power collapse while this stock-kernel
restriction remains.

## Host and live acceptance

Host checks passed:

```text
pixel2_power_menu_sleep=result-ok
app_layer_scripts=result-ok
system_rootfs_scripts=result-ok
frontend_component=result-ok
app_layer_verify=result-ok
```

The final display-power signed Runtime package was inspected from base
`0.1.0-dev-4620c4b`, then requested through the normal updater:

```text
package=plumos-pixel2-runtime-0.1.0-dev-c5d9c16.tar.gz
sha256=fc52a942b00303e204176c2908cdf01d43e60b7d842aebdd5e2a69b512eeb46b
payload_files=9
result=ready
```

After reboot the device reported:

```text
VERSION=0.1.0-dev-c5d9c16
runtime_transaction=healthy
hardware-key service processes=1
frontend processes=1
```

A bounded three-second software-standby test blanked and restored the live
backlight exactly:

```text
sleep=software-enter reason=kernel-unavailable wakeup_sec=3
sleep=software-wake reason=timeout seconds=3
sleep=result-returned backend=mem kernel_sleep=0
backlight_before=28 backlight_after=28
```

## Physical sleep and USB hotplug acceptance

The FE path was physically accepted on `0.1.0-dev-c5d9c16`:

1. Power opened the global menu and Sleep fully powered down the display;
2. unplugging and reconnecting USB while asleep left the display off;
3. the service logged `online=0`, then `online=1`, and
   `action=adb-usb-restart online=1 rc=0`;
4. ADB re-enumerated while `/run/plumos/software-sleep` remained active;
5. one physical Power press cleared the marker, restored brightness 28 and
   `bl_power=0`, returned to the FE, and FE controls remained functional.

The RetroArch overlay path was physically accepted on
`0.1.0-dev-e9a69a9`:

1. Power paused the running game and displayed the global power menu over RA;
2. selecting Sleep fully powered down the display;
3. unplugging and reconnecting the power/USB cable left the display asleep and
   restarted the ADB gadget without waking the screen;
4. one physical Power press restored the game, video, and controls;
5. the original RA process remained alive throughout the sequence.

The live logs recorded the complete ownership handoff and recovery:

```text
action=power-menu overlay=1 rc=0
drm-planes=suspend owner=1637 count=2
drm-master=drop owner=1637 fd=5 rc=0
action=adb-usb-restart online=1 rc=0
action=software-sleep-wake rc=0
drm-master=restore owner=1637 rc=0
drm-planes=restore owner=1637 count=2 rc=0
display-owner=resume owner=1637
```

The DraStic path was physically accepted on `0.1.0-dev-12b809b` after launching
Nintendo DS content from the normal FE route. Power-menu display, Cancel,
software Sleep, USB/power disconnect and reconnect while asleep, one-press
wake, game continuation, video, controls, and audio all passed. The display
runner and armhf core were paused and resumed together:

```text
display-companion=pause owner=1385 pid=1396 rc=0
drm-planes=suspend owner=1385 count=1
drm-master=drop owner=1385 fd=7 rc=0
event=usb-power-transition online=0 guard_ms=1500 adb_restart_ms=0
event=usb-power-transition online=1 guard_ms=1500 adb_restart_ms=2000
action=adb-usb-restart online=1 rc=0
action=software-sleep-wake rc=0
drm-master=restore owner=1385 rc=0
drm-planes=restore owner=1385 count=1 rc=0
display-owner=resume owner=1385
display-companion=resume owner=1385 pid=1396
```

An earlier test launched DraStic directly below an interactive ADB shell. ADB
re-enumeration sent that diagnostic launcher `SIGHUP`, which terminated the
core and left a stopped runner waiting in cleanup. Commit `12b809b` resumes the
runner before TERM and bounds that cleanup. Physical acceptance was repeated
from the production FE launcher, which is independent of the ADB session.

The PicoArch path was physically accepted on `0.1.0-dev-45b4505`. Sleep and
wake returned to the same game, and the physical D-pad worked in both the game
and PicoArch menu after `BTN_DPAD_*` was added to the Pixel2 game/menu binds in
`d1f5ea1`.

## Remaining physical gate

The normal FE, RetroArch, PicoArch, and DraStic sleep/USB/wake paths are
complete. The operator still needs to validate:

1. Cancel returns normally from the RA overlay;
2. Power opens the overlay while the remaining standalone emulators and Apps
   each own the screen;
3. Cancel and sleep/wake return video, input, and audio for each remaining
   family.

These are physical acceptance gates, not missing implementation.
