# Pixel2 forced-power-loss health and PicoArch PC-98 validation

Date: 2026-08-31

## Forced power loss

After the battery was exhausted, the device booted normally and the managed
runtime remained intact. The frontend and hardware-key service were running,
the ext4 app/state partition reported no error, and the only storage warning
was the expected FAT dirty bit on `/dev/mmcblk0p3`.

The mounted read-only check found no directory or allocation damage. The user
partition was then taken offline and checked with the bundled `fsck.fat`:

```text
Dirty bit is set. Fs was not properly unmounted and some data may be corrupt.
 Automatically removing dirty bit.
/dev/mmcblk0p3: 3818 files, 170347/1628977 clusters
```

The repair returned `1` (metadata corrected), and the immediate no-write
verification returned `0`. After a clean plumOS reboot, the kernel log no
longer contained `Volume was not properly unmounted` or any ext4, MMC, I/O, or
corruption error.

## PicoArch NP2kai input

NP2kai declares `np2kai_joymode=OFF` upstream. That default leaves PicoArch's
RetroPad input connected to the core but does not translate the D-pad and face
buttons into PC-98 keyboard input. The Pixel2 core build now changes the
English and Japanese core-option defaults to `Arrows 3button`, matching the
working RetroArch factory policy.

PicoArch still loads a user's saved game/global configuration after core
defaults, so a user-selected `Mouse`, `OFF`, or another mapping remains in
effect. The PC-98 frontend catalog now exposes `picoarch:np2kai` and
`picoarch:nekop2` in addition to the two RetroArch profiles; RetroArch NP2kai
remains the default route.

## Neko Project II font

Neko Project II historically searches only `system/np2`. Pixel2 stores the
shared PC-98 firmware in `system/np2kai`, so the core started without its font.
The MF-proven fallback was ported with this lookup order:

1. `np2/font.bmp`
2. `np2/FONT.ROM`
3. `np2kai/font.bmp`
4. `np2kai/FONT.ROM`

No BIOS was copied or renamed. The existing files under the mutable user BIOS
directory remain the single source of truth.

## Build and live-device proof

- Commit: `b1c1de3`
- Runtime: `0.1.4-dev-b1c1de3`
- AArch64 core SHA-256:
  - NP2kai: `757fd02b9301bac9a05a20e5f1591fcf83d86da587f6eedafc68f92f03308526`
  - Neko Project II: `781c0c8a71657ed3b62193f369b004adc8b49446e25cac85ed733603b941008d`
- Frontend component: 201 / 201 checksums passed
- Libretro core component: 360 / 360 checksums passed
- Complete app layer: 11,334 / 11,334 checksums passed

The three new/non-default PC-98 routes were launched with the existing
`Can Can Bunny 5 and half Limited.hdi` test content:

| Profile | Startup | Visible frame | ALSA playback |
|---|---:|---:|---:|
| `retroarch:nekop2` | pass | pass | pass |
| `picoarch:np2kai` | pass | pass | pass |
| `picoarch:nekop2` | pass | pass | pass |

Both Neko Project II captures visibly rendered Japanese PC-9800 boot text,
which proves that the shared font is loaded. Machine tests cannot press the
physical Pixel2 controls; final D-pad/ABXY behavior remains an operator check.

The deployment did not modify mutable settings. The active RetroArch config
remained SHA-256 `6c077932...9258`, and `bios/np2kai/np2kai.cfg` remained
`0fd644e7...dabb` before and after deployment and reboot.

