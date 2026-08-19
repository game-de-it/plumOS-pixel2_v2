# Architecture and Ownership

## Layer Model

```text
Pixel2 stock boot substrate
  Rockchip boot prefix / stock Image 5.10.198 / stock initramfs
  exact checksum-registered stock runtime DTB
                       |
plumOS SYSTEM on PLUMOS_BOOT
  /sbin/init / init.d services / ADB / USB Wi-Fi / rootfs diagnostics
                       |
plumOS app layer on PLUMOS_SYS
  /mnt/plumos/bin / cores / config / factory-defaults / manifests
                       |
Pixel2 user/content volume
  /mnt/plumos-user/roms / bios / Images / Screenshots / updates
```

The stock userland is not part of the normal runtime. The accepted boundary is
the stock initramfs handoff to `SYSTEM`; from `/sbin/init` onward, plumOS owns
the booted OS behavior.

## Process and Mount Ownership

| Path or resource | Owner | Rule |
| --- | --- | --- |
| `/boot` | stock-mounted PLUMOS_BOOT | normally read-only; holds `Image`, DTB, `SYSTEM`, and manifests |
| `/mnt/plumos` | PLUMOS_SYS | app-layer ABI, managed runtime, active config, logs, saves/states fallback |
| `/mnt/plumos-user` | PLUMOS_USER | user ROMs, BIOS, media, screenshots, and update inbox |
| `/roms` | bind from `/mnt/plumos-user/roms` | frontend and launchers use this as ROM root |
| `/state` | bind from `/mnt/plumos/state` | early and runtime log compatibility path |
| `/run/plumos` | tmpfs | PIDs, locks, transient input/audio/display state |
| `/dev/fb0` | one foreground owner | FE, emulator, app, or power action screen |
| `/dev/input/event2` | Pixel2 joypad | FE and hardware-key service may read; launchers must avoid duplicate foreground owners |
| `gpio-keys` event | hardware-key service | volume keys and SELECT+volume brightness |
| RK817 PMIC | safe power helper | shutdown uses RK817 DEV_OFF |

## Persistent and Mutable Data

Managed replaceable files include binaries, libraries, cores, frontend catalogs,
themes, factory defaults, notices, manifests, checksums, and `SYSTEM`.

Device-owned mutable files include active settings, ROMs, BIOS files, artwork,
saves, states, logs, Wi-Fi credentials, SSH state, Pyxel project state, and
future app state such as PortMaster. Updates and live deploys must preserve
these paths.

## Current Implementation Boundary

Implemented:

- stock boot substrate plus plumOS-owned `SYSTEM`;
- app-layer frontend, text UI, scanner, RetroArch, full libretro catalog,
  PicoArch, OpenBOR, DraStic, PPSSPP, Pyxel runtime, manifests, and strict
  checksum gate;
- ADB maintenance path;
- minimal USB Wi-Fi runtime path;
- Pixel2 input map contract and global hardware-key daemon;
- FE reboot path, RK817 shutdown helper, and RK817-aware audio routing.

Not yet complete:

- final System A/B and transactional updater;
- PortMaster, File Manager, and Music Player;
- production ADB authentication or explicit opt-in;
- final transactional update/rollback path.
