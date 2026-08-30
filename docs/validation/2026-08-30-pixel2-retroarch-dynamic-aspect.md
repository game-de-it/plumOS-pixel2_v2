# Pixel2 RetroArch dynamic aspect validation

## Scope

On Pixel2, changing RetroArch to `Core Provided` while a game was running did
not change the game surface. Opening the menu afterwards created only the menu
at the new aspect, which made the menu appear distorted relative to the game.
The issue was reproduced on device `192.168.10.107` with Gambatte and
`Aretha (Japan).gb`.

With the saved 4:3 setting (`aspect_ratio_index = "0"`), the initial game DRM
plane was physical `480x640 RG16`. Selecting Core Provided (index 22) at runtime
left that game plane at `480x640`; opening the menu then created a
`480x533 XR24` menu plane.

## Cause and repair

The Pixel2 plain DRM backend renders the game and menu into dumb buffers sized
for the selected aspect. Runtime `drm_set_aspect_ratio()` updated the cached
aspect but did not recreate existing surfaces. The old 4:3 game buffer therefore
survived while the newly created menu buffer used the Game Boy aspect.

Commit `6777ec6` now frees both main and menu surfaces when the runtime aspect
changes, invalidates the cached core dimensions, and recreates each surface on
its next frame. It deliberately preserves the current menu-open state.

Commit `0292cc5` changes the fresh-install and Factory Reset default to Core
Provided (index 22). Normal-core launcher append configuration does not override
the aspect. Config merging also leaves any existing `aspect_ratio_index`
untouched, so later user choices such as 4:3, Full, or Custom remain authoritative.

## Device result

| Test | DRM game plane | Result |
| --- | --- | --- |
| Before repair, start at 4:3 | `480x640 RG16` | baseline |
| Before repair, switch to Core Provided | `480x640 RG16` | change ignored |
| After `6777ec6`, switch at runtime | `480x533 RG16` | GB aspect recreated |
| After `0292cc5`, start with saved index 22 | `480x533 RG16` | Core Provided at startup |

The operator visually accepted both the repaired game and RGUI menu aspect.
The final game capture had a non-black ratio of `1.0`, and the active setting
remained index 22 after returning to the frontend. Evidence is stored under
`output/live/2026-08-30-pixel2-retroarch-aspect-regression/`.

## Build and deployment

- Runtime: `0.1.4-dev-0292cc5`
- Dynamic surface repair: `6777ec6`
- Factory default change: `0292cc5`
- Clean managed-delta SHA-256:
  `3247de985761de9f1a1aff17625a87eb2c075793f5ec458d1b59ee3dc0ff4771`
- Strict app-layer build: PASS
- License audit: PASS (110 libretro cores)
- Device RetroArch component: 7,060 / 7,060 checksums passed
- Device app layer: 11,333 / 11,333 checksums passed

The deployment contained seven managed files plus root `checksums.sha256`.
The active `retroarch.cfg` SHA-256 remained
`df46c83539f2269f0d7247240c4c042097f84577add3d0df16a1237658635c50`
before and after deployment. User settings, ROMs, BIOS files, saves, and states
were not overwritten.
