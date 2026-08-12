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

Apps is intentionally empty until app components are ported. Do not expose
launchers that are not present in the Pixel2 app-layer.

## Launch Profile Contract

The implemented libretro launch route family is:

```text
retroarch:<core-id>
```

The FE catalog must only expose systems whose runtime/core exists in the
generated app-layer. `plumos-text-ui launch ... --no-scan` is the host and
device launch-plan verifier.

## RetroArch

RetroArch and the Pixel2 libretro sets are packaged for AArch64.
`./scripts/docker-build.sh cores --filter plumos --jobs 4 --fail-on-error 1`
builds 41 catalog cores and emits the `libretro-cores` component manifest and
checksums. `./scripts/docker-build.sh cores --filter all --jobs 4
--fail-on-error 1` builds the full 114-core catalog used for full-system route
coverage. The FE system catalog is generated around those managed launch
profiles rather than a single QuickNES-only route.

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
axis entries and binds `BTN_SELECT`/`BTN_START` as hotkey/exit. Do not use a
`linuxraw` Pixel2 default unless `/dev/input/js0` button numbering has been
captured and validated on the real device.

## Pending Emulator Work

PicoArch, standalone emulators, Pyxel, PortMaster, File Manager, and Music
Player are not yet Pixel2 runtime guarantees. Add them only when each component
is built, routed, checksummed, and physically validated on Pixel2.
