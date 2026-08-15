# Pixel2 global power menu and sleep validation

Date: 2026-08-15
Final tested Runtime: `0.1.0-dev-a9b4d2a`

## Result

Pixel2 now has one power contract across the FE, RetroArch, PicoArch,
standalone emulators, and Apps. The stock `rk805 pwrkey` is monitored by the
always-running hardware-key service. The normal FE handles Power directly;
outside the FE, the service starts the same FE in power-overlay mode after
pausing only the processes that own `/dev/fb0`, DRM, Mali, or `/dev/disp`.

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

1. leaves the current display owner stopped;
2. saves and blanks the Pixel2 backlight without changing persistent settings;
3. waits for the hardware-key service to consume the next Power press;
4. suppresses the duplicate queued Power event in the normal FE;
5. restores the exact backlight, display/volume state, RK817 route, and owner.

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

The final signed Runtime package was inspected from base
`0.1.0-dev-03295f4`, then requested through the normal updater:

```text
package=plumos-pixel2-runtime-0.1.0-dev-a9b4d2a.tar.gz
sha256=fe2992dd7bbc4326dc98c605fbe5c574b9a39c77a3259d48f364c50cb16b04fa
payload_files=12
result=ready
```

After reboot the device reported:

```text
VERSION=0.1.0-dev-a9b4d2a
frontend_ok=194 frontend_bad=0
frontend=ready
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

## Remaining physical gate

The operator still needs to validate the tactile path:

1. Power opens the menu from the FE and Cancel returns normally;
2. Power opens the overlay while each runtime family owns the screen;
3. selecting Sleep blanks the screen and the next Power press wakes it;
4. no second menu appears from the wake press;
5. input, video, and audio continue after wake.

These are physical acceptance gates, not missing implementation.
