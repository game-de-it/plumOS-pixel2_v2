# Pixel2 NES ROM Staging

Date: 2026-08-12

## Goal

Place a small NES ROM set on the live Pixel2 user partition so the frontend can
launch a real game and physical audio can be checked.

ROMs are user content. They are intentionally not copied into the repository,
app-layer, SYSTEM image, or release artifacts.

## Source

Host ROM set:

```text
/Volumes/public-1/02/motoki/emu/ROM/rom2/nes
```

## Target

Live Pixel2 user storage:

```text
/mnt/plumos-user/roms/nes
```

## Transferred ROMs

```text
Pac-Man [V1.0].nes
Super Mario Bros..nes
TwinBee.nes
```

On-device hashes:

```text
c67e20d7144dac97ba9d4cef1ad90f13d6504e8492e60a6193c33695d68c4b22  /mnt/plumos-user/roms/nes/Pac-Man [V1.0].nes
0b3d9e1f01ed1668205bab34d6c82b0e281456e137352e4f36a9b2cfa3b66dea  /mnt/plumos-user/roms/nes/Super Mario Bros..nes
1abd84018fcb65492d622e6f2d5bb3a3537394c1805a90213cd96a3f8cb4451c  /mnt/plumos-user/roms/nes/TwinBee.nes
```

## Frontend Catalog Validation

The live scanner found all three ROMs:

```text
system nes                roms=3 thumbnails=0
summary alias_dirs=1 files_seen=3 matched=3 roms=3 thumbnails=0
```

Top-level frontend catalog:

```text
No.  V   System              Default profile
---  -   ------              ---------------
  1.     NES                 retroarch:quicknes
  2. *   Favorites           internal:favorites
```

NES ROM list:

```text
No.  Fav Title                              Path
---  --- -----                              ----
  1.     Pac-Man [V1.0]                     nes/Pac-Man [V1.0].nes
  2.     Super Mario Bros.                  nes/Super Mario Bros..nes
  3.     TwinBee                            nes/TwinBee.nes
```

## Launch Plan Validation

`Super Mario Bros..nes` resolves to the Pixel2 NES route:

```text
kind: retroarch
system: nes
rom: nes/Super Mario Bros..nes
title: Super Mario Bros.
path: /mnt/plumos-user/roms/nes/Super Mario Bros..nes
launch_profile: retroarch:quicknes
retroarch: /mnt/plumos/bin/plumos-retroarch-launch (exists)
core: /mnt/plumos/cores/quicknes_libretro.so (exists)
rom_exists: yes
can_execute: yes
```

## Frontend Refresh

The frontend was restarted after the ROM scan so the visible UI can pick up the
new cache:

```text
frontend=1888
hardware=646
pidfile=1888
frontend=result-started pid=1888
```

## Remaining Check

Launch one of the NES entries from the physical frontend and confirm:

- game video appears with correct orientation and scaling;
- D-pad, ABXY, START/SELECT, shoulder buttons, and exit hotkey behave as
  intended;
- audio output works, then validate volume up/down against real game audio.
