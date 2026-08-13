# Frontend and Emulator Integration

## Frontend Data Model

The Pixel2 frontend reads:

```text
/mnt/plumos/config/frontend/systems.json
/mnt/plumos/config/frontend/menus.json
/mnt/plumos/config/frontend/apps.json
/mnt/plumos/state/frontend/library-index.json
```

The START menu follows the shared plumOS handheld top-level contract:

1. UI Settings
2. System Settings
3. Network Settings
4. Apps
5. HELP
6. Reboot
7. Shutdown

Apps only exposes tools whose app-layer component is present. The current
Pixel2 app list includes `Pyxel Setup`; do not expose launchers that are not
present in the Pixel2 app-layer.

## Launch Profile Contract

The implemented launch route families are:

```text
retroarch:<core-id>
picoarch:<core-id>
standalone:<emulator-id>
pyxel:pixel2
```

The FE catalog must only expose systems whose runtime/core exists in the
generated app-layer. `plumos-text-ui launch ... --no-scan` is the host and
device launch-plan verifier.

## RetroArch

RetroArch and the Pixel2 libretro sets are packaged for AArch64.
`./scripts/docker-build.sh cores --filter plumos --jobs 4 --fail-on-error 1`
builds 41 catalog cores and emits the `libretro-cores` component manifest and
checksums. `./scripts/docker-build.sh cores --filter all --jobs 4
--fail-on-error 1` builds the full 109-core catalog used for full-system route
coverage. `./scripts/docker-build.sh core-catalog --filter all --concurrency 4`
is the preferred full rebuild path because it runs independent per-core workers
and then aggregates the canonical Pixel2 component. The FE system catalog is
generated around those managed launch profiles rather than a single
QuickNES-only route.

Saturn is disabled on Pixel2 with `unsupported_performance_rk3326`; its two
libretro cores and YabaSanshiro route are not built or exposed. Mupen64Plus-Next
is also not part of the Pixel2 catalog: the pinned AArch64 core segfaulted on
the stock kernel with dynarec disabled and with cached/pure interpreters across
GLideN64, Angrylion, and ParaLLEl RDP paths. Nintendo 64 remains enabled through
the device-verified `retroarch:parallel_n64` route.

Factory defaults include the Pixel2 joypad autoconfig and the current
display/aspect contract.

Pixel2's stock kernel exposes the LCD as a native `480x640` DRM mode, while
plumOS presents the frontend and emulators as a logical `640x480` landscape
surface. The RetroArch plain DRM backend is patched to implement software
rotation; Pixel2 defaults to `video_rotation = "3"` (`ccw`) to match the
frontend's validated panel correction.

Pixel2 also fixes RetroArch to a 4:3 viewport (`aspect_ratio_index = "0"`,
`video_force_aspect = "true"`) instead of core auto aspect. This keeps NES and
other 4:3 cores from stretching after the DRM software rotation has converted
the native portrait panel into the logical `640x480` landscape surface.

RetroArch must use the `udev` joypad driver on Pixel2. The kernel reports the
D-pad as `ABS_X`/`ABS_Y` axes and the remaining controls as evdev keys on
`pixel2_joypad`; the generated autoconfig therefore binds D-pad directions with
axis entries. Persistent config keeps SELECT+START as the explicit exit chord,
while the controller autoconfig binds raw `BTN_TRIGGER_HAPPY1` through compact
udev index 14 directly to `input_menu_toggle_btn`. The autoconfig intentionally
does not carry `input_enable_hotkey_btn`: the dedicated Function menu must not
be gated by SELECT. Do not use a
`linuxraw` Pixel2 default unless `/dev/input/js0` button numbering has been
captured and validated on the real device.

## Runtime Families

PicoArch is packaged as a Pixel2 app-layer component. It reuses the generated
libretro core set through the shared `/mnt/plumos/cores/*_libretro.so` route
and uses the Pixel2 ALSA `plumos_output` path. Its libpicofe evdev map binds
`BTN_TRIGGER_HAPPY1` to `EACTION_MENU`/`PBTN_MENU`; `BTN_MODE` is not the
Pixel2 Function key.

Standalone has a Pixel2 launcher component and route manifest. OpenBOR,
DraStic, and PPSSPP are packaged binaries in the current Pixel2 app-layer.
DraStic uses the steward-fu/nds Pixel2 integration layer with the closed armhf
DraStic runtime, package-local armhf libraries, and an armhf ALSA plugin for
`plumos_output`. DraStic BIOS files are never packaged; the launcher copies
user-provided `drastic_bios_*.bin` files from `/mnt/plumos-user/bios/drastic`,
`/mnt/plumos-user/bios/nds`, or `/mnt/plumos-user/bios` into the mutable
per-user DraStic work directory.

PPSSPP is built from pinned upstream source (`v1.20.4`,
`fa50bb1976065c4f8b1b47af227d367fe9771555`) as a Pixel2 SDL2/GLES/EGL
standalone binary. The app-layer includes PPSSPP assets and factory
`ppsspp.ini`/`controls.ini` under
`factory-defaults/standalone/ppsspp/PSP/SYSTEM`; the launcher seeds those files
to the mutable PPSSPP config directory on first launch and keeps user changes
out of the immutable app-layer. The factory config disables touch controls and
uses the Pixel2 state directory. Pixel2's physical 480x640 portrait panel is
presented as a logical 640x480 landscape display: the landscape layout uses
`DisplayAspectRatio = 0.562500` before the final CCW scanout rotation, while
`UIScaleFactor = -2` keeps the pause-menu text readable. The launcher migrates
only the exact older Pixel2 values (`1.000000` and `-8`) and preserves all
unrelated user settings. Physical controls, audio, menu/exit, and save-state
validation remain separate acceptance gates.

PCSX-ReARMed is built from pinned upstream r26l commit
`9f8b6f248e073f03c530efda7c4cc60a7e2ecafc` with libpicofe commit
`dd11f2d723162eb1cf8e6db9f40de7db0d0b6bba` and sdl12-compat commit
`fc2ec0c128197f1f5050e48359bc41e618f3abfb`. Its default renderer is the
threaded built-in NEON GPU. Pixel2 does not use a foreign-device GLES profile:
the SDL surface is presented directly to `/dev/fb0` as a logical 640x480 frame,
counter-clockwise rotated onto the physical 480x640 framebuffer and scaled to
4:3.

PCSX gameplay and menu input use libpicofe evdev so the captured raw contract
is authoritative: D-pad is `ABS_X`/`ABS_Y`, physical A/B are
`BTN_EAST`/`BTN_SOUTH`, and Function is `BTN_TRIGGER_HAPPY1`. SDL remains a
fallback, but menu entry no longer depends on a guessed SDL button number. In
the emulator menu, physical A confirms and B returns. Audio is required to open the plumOS ALSA
route and is resampled from the PSX 44.1 kHz stream to the RK817 route's 48 kHz
contract. The launcher seeds `~/.pcsx/pcsx.cfg`, preserves user changes and
save data, and imports user-provided `scph*.bin` files from the Pixel2 BIOS
directories without packaging BIOS content.

The host build and dependency gates are complete. Physical Pixel2 launch,
rotation/aspect, D-pad/ABXY/START/SELECT/shoulders/FUNCTION, sound, save, menu
exit, and frontend reacquisition remain required before this alternate profile
is release-proven.

## Function menu contract

Every implemented emulator family must expose its native menu on the physical
Function button. The Pixel2 source contract is raw evdev code 704
(`BTN_TRIGGER_HAPPY1`); framework-specific indices are derived from that
captured value, not copied from another device.

- RetroArch: compact udev button 14 -> `input_menu_toggle_btn`.
- PicoArch and PCSX-ReARMed: direct libpicofe evdev menu action.
- DraStic: SDL joystick button 8 -> `1024 + 8 = 1032`; the launcher migrates
  only the known stale value 1154.
- PPSSPP: SDL Guide -> Android-style Back/Pause code `10-4`; the launcher adds
  that code to an existing Pause list without replacing user bindings.
- OpenBOR: SDL button 10 -> its Escape/menu action; D-pad remains the first
  four axis-derived OpenBOR joystick inputs.

`tests/test-pixel2-emulator-menu-contract.sh` prevents any of these mappings
from silently disappearing or reverting to `BTN_MODE`.

Do not expose a Nintendo DS libretro DeSmuME route on Pixel2 unless a real
Pixel2 build-system component exists. The current DS route is
`standalone:drastic`.

Remaining standalone emulator binaries are intentionally marked
`pending-binary` until each binary is built, packaged, and physically validated
on Pixel2.

Pyxel is packaged as a Pixel2 app-layer component with a bundled Python 3.11
runtime, pinned baseline Pyxel/pygame/numpy/Pillow wheels, SDL2 KMSDRM/GLES
libraries, a Pixel2 display-fit shim, and the Pixel2 ALSA `plumos_output` path.
The FE route is `pyxel:pixel2` and must resolve to
`bin/plumos-pyxel-pixel2-launch`. `Pyxel Setup` installs optional project
requirements from `/roms/pyxel/requirements.txt` into mutable app-layer state;
it must not overwrite packaged runtime files.

PortMaster, File Manager, and Music Player are not yet Pixel2 runtime
guarantees. Add them only when each component is built, routed, checksummed, and
physically validated on Pixel2.
