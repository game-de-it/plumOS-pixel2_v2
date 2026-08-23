# Frontend and Emulator Integration

[日本語](frontend-emulators.ja.md)

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
6. POWER

`POWER` opens the shared Sleep/Reboot/Shutdown/Cancel menu. Reboot and
Shutdown are not duplicated as START entries.

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

WonderSwan and WonderSwan Color are the exception to frontend-managed content
rotation. Beetle WonderSwan disables its internal rotation when
`RETRO_ENVIRONMENT_SET_ROTATION` succeeds, but Pixel2's DRM rotation also owns
the fixed native-panel correction. Their launch profiles therefore use
`video_rotation = "0"`, `video_allow_rotate = "false"`, and
`PLUMOS_DRM_PANEL_ROTATION=3`. RetroArch calculates an unrotated logical
viewport, Beetle WonderSwan performs the SELECT-controlled software rotation,
and the DRM presenter applies only the fixed Pixel2 panel correction last.
Rejected core rotation requests are not retained in RetroArch's frontend
rotation state; otherwise the already-rotated 144x224 frame is inverted to
14:9 a second time. This follows the proven plumOS portrait-panel separation
without importing another device's runtime identity. These two profiles use
`aspect_ratio_index = "22"` (core provided), instead of the global Pixel2 4:3
policy, preserving both the initial 224x144 (14:9) layout and the
SELECT-switched 144x224 (9:14) layout.

RetroArch must use the `udev` joypad driver on Pixel2. The kernel reports the
D-pad as `ABS_X`/`ABS_Y` axes and the remaining controls as evdev keys on
`pixel2_joypad`; the generated autoconfig therefore binds D-pad directions with
axis entries. The dedicated Function button is compact udev index 14 and opens
the menu directly. START+SELECT is the Pixel2 direct RetroArch exit chord:
SELECT enables hotkeys and START is `input_exit_emulator_btn`. Gameplay hotkeys
use the same SELECT enable key:
L/R load and save state, X captures a screenshot, Y toggles FPS, D-pad left/right
changes the state slot, and L2/R2 toggles slow/fast motion. Pixel2 reports L2/R2
as buttons 6/7, so another device's trigger-axis bindings must not be copied.
Do not use a
`linuxraw` Pixel2 default unless `/dev/input/js0` button numbering has been
captured and validated on the real device.

The RetroArch factory contract is a bundle, not only `retroarch.cfg`:

- `retroarch.cfg` owns global input, saving, display, audio, and menu settings;
- `retroarch-core-options.cfg` owns shipped core defaults;
- `remaps/<core>/<core>.rmp` owns core-specific controller remaps.

`plumos-retroarch-config-merge` installs all three and only appends absent keys
to user-owned auxiliary files. The migration from the earlier incomplete
Pixel2 factory changes ten known save/trigger defaults only when the recorded
factory generation and old values both match. A separate two-key migration
restores START+SELECT direct exit for the short-lived regressed factory;
unrelated user values are kept.

Pixel2 builds all four upstream menu drivers: RGUI, GLUI (MaterialUI), Ozone,
and XMB. RGUI remains the factory default. The native RetroArch settings under
`Settings -> Drivers -> Menu` and `Settings -> User -> Language` are the
authoritative selectors; a normal RetroArch exit persists them and the new
choice is used on the next start. The app launcher must not place
`menu_driver` or `user_language` in its append config, because append values
would silently override the mutable user config on every launch.

Graphical menu media is app-layer-owned at
`/mnt/plumos/retroarch/assets`. It includes every shipped XMB theme plus the
Ozone/GLUI icons and the upstream `pkg` fallback fonts for Arabic/Persian,
Chinese, and Korean. The previous empty mutable assets path is migrated only
when it exactly equals `/mnt/plumos/config/retroarch/assets`; custom asset
paths, menu choices, language choices, hotkeys, saves, and states are not
overwritten. RGUI uses its smaller built-in font set, so Ozone or GLUI should
be selected for languages whose script RGUI does not support.

Pixel2 uses a native 480x640 DRM scanout but presents graphical RetroArch menus
as a logical 640x480 surface. The GL menu contract therefore applies the same
logical dimensions to the menu layout callback, viewport rectangles, and font
coordinates. Fixing only the final rotation leaves XMB icons and labels laid
out by different dimensions and makes them overlap. The factory menu scale is
1.5, with Ozone global font scaling enabled at 1.35; migration changes only the
three exact values from the first small-menu factory and preserves the selected
driver, language, hotkeys, saves, and states.

Normal saves and save states use the active ROM filesystem with content-folder
and core sorting. `/mnt/plumos/saves/<system>` and
`/mnt/plumos/states/<system>` remain fallback paths when content-local saving
is disabled, and old files in those fallback paths are never deleted by the
migration.

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

The host build and dependency gates are complete. Physical Pixel2 testing has
accepted launch, rotation/aspect, D-pad and face-button input, Function-menu
navigation, sound, menu exit, and frontend reacquisition. Save persistence and
BIOS provenance remain part of the general release validation contract rather
than PCSX-specific implementation gaps.

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

ScummVM, EasyRPG, Flycast, and NXEngine-Evo use their existing libretro routes
as the Pixel2 product path. Their optional standalone binaries are marked
`libretro-route` / `standalone_policy=deferred`; they are not actionable Pixel2
implementation gaps unless the product scope is changed explicitly.

Pyxel is packaged as a Pixel2 app-layer component with a bundled Python 3.11
runtime, pinned baseline Pyxel/pygame/numpy/Pillow wheels, SDL2 KMSDRM/GLES
libraries, a Pixel2 display-fit shim, and the Pixel2 ALSA `plumos_output` path.
The FE route is `pyxel:pixel2` and must resolve to
`bin/plumos-pyxel-pixel2-launch`. `Pyxel Setup` installs optional project
requirements from `/roms/pyxel/requirements.txt` into mutable app-layer state;
it must not overwrite packaged runtime files.

PortMaster, File Manager, and Music Player are packaged Pixel2 components with
managed manifests and checksums. Their foreground ownership, display rotation,
input, audio where applicable, exit, and FE reacquisition paths have been
physically exercised. PortMaster-installed games remain mutable user data and
are never replaced by app-layer assembly or updates.
