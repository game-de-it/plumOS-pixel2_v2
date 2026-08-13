# Pixel2 persistent visual-validation ROM restore

Date: 2026-08-14

## Correction

The all-route smoke runner intentionally staged one system at a time and
removed its hidden directory after launch. That was safe for the original
1.5 GiB user volume, but it broke the handoff to operator-led visual checks:
content that passed automated startup was no longer available in the frontend.

The restore workflow now treats automated startup and physical acceptance as
two consecutive gates. `smoke-test-pixel2-romset.py` explicitly records that
persistent content was not retained, and `restore-pixel2-smoke-roms.py` restores
every passing sample into the normal frontend directory aliases. Existing
different files are never overwritten; each new file is transferred through a
temporary name and renamed only after its device SHA-256 matches the source.

## User-volume preparation

The physical SD is 62,534,975,488 bytes, but the development image ended at the
4 GiB seed boundary. The complete smoke-tested representative set requires
2,256,405,428 bytes, while the old p3 had only 919,336 KiB free.

Before changing the layout, all 651 files from the old `PLUMOS_USER` were
pulled to
`output/live/2026-08-14-user-volume-expansion/plumos-user-backup`. Host and
device inventories matched with zero missing, extra, or mismatched SHA-256
entries. The current 16 MiB Rockchip/MBR prefix was also captured as
`mmc-prefix-before-user-expand.bin`, SHA-256
`e94d4d7f7986a9d613325ce3b143f2f2fe8e0dc667ae52ab6ce6baeb7eafd5b8`.

The tested card was then changed to the adopted final boundaries:

| partition | start sector | sectors | resulting size |
| --- | ---: | ---: | ---: |
| p1 `PLUMOS_BOOT` | 32768 | 1048576 | 512 MiB |
| p2 `PLUMOS_SYS` | 1081344 | 16777216 | 8192 MiB |
| p3 `PLUMOS_USER` | 17858560 | 104280064 | 49.7 GiB |

p1, its boot flag, and the p2 start sector were unchanged. p2 was grown online
with `resize2fs`; p3 was created as FAT32 with label `PLUMOS_USER` and serial
`504C0003`. The 651-file backup was restored with 651/651 SHA-256 matches.
This proves the target layout on this card; the idempotent first-boot
provisioner for release images remains a separate implementation gate.

## Persistent content restore

The restore plan was derived only from `status=pass` rows in the canonical
device-smoke reports:

```text
systems=73
launch_profiles=164
representative_samples=78
source_files=1447
synthetic_markers=2
bytes=2256405428
```

The first application transferred all 1,447 source files. A second complete
application rehashed every target and reported:

```text
restore_result transferred=0 skipped=1447 result=ok
```

The device contains 1,450 ROM-tree files: the 1,449 planned restored files plus
the pre-existing lower-case `psx/chroQW.img` copy. No ROM is added to the
repository, app-layer, or release image; this is user-volume validation content
from the operator-provided set.

## Cold-boot result

After safe reboot:

- p2 mounted as 7.8 GiB with 6.3 GiB available;
- p3 mounted as 49.7 GiB with 47.0 GiB available;
- `/roms` bound to the persistent p3 ROM tree;
- frontend renderer-ready and ADB returned;
- frontend scan completed in 526 ms;
- 73 systems exposed 83 visible content entries.

The 14 enabled systems without visible content are the 13 systems for which the
supplied set has no compatible sample plus Channel F, whose three mandatory
BIOS files are absent. The restored 73-system set is now ready for operator-led
screen, input, audio, menu, exit, and second-launch checks.
