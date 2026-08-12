# Frontend and Emulator Integration

## Frontend Data Model

The Pixel2 frontend reads:

```text
/mnt/plumos/config/frontend/systems.json
/mnt/plumos/config/frontend/menus.json
/mnt/plumos/config/frontend/apps.json
/mnt/plumos/state/frontend/library-index.json
```

The START menu follows the MF/V90S top-level contract:

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

The current implemented launch route is:

```text
retroarch:quicknes
```

The FE catalog must only expose systems whose runtime/core exists in the
generated app-layer. `plumos-text-ui launch ... --no-scan` is the host and
device launch-plan verifier.

## RetroArch

RetroArch and QuickNES are packaged for AArch64. Factory defaults include the
Pixel2 joypad autoconfig and the current display rotation contract.

Pixel2's stock kernel exposes the LCD as a native `480x640` DRM mode, while
plumOS presents the frontend and emulators as a logical `640x480` landscape
surface. The RetroArch plain DRM backend is patched to implement software
rotation; Pixel2 defaults to `video_rotation = "3"` (`ccw`) to match the
frontend's validated panel correction.

## Pending Emulator Work

The MF/V90S developer guides include full baseline libretro cores, PicoArch,
standalone emulators, Pyxel, PortMaster, File Manager, and Music Player. These
are not yet Pixel2 runtime guarantees. Add them only when each component is
built, routed, checksummed, and physically validated on Pixel2.
