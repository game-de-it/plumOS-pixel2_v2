# Pixel2 P0 settings backends

## Scope

- `plumos-time-sync`: bounded RFC868 sync and RK817 RTC UTC storage;
- `plumos-storage-health`: bounded read-only FAT32 check for `/mnt/plumos-user`;
- `plumos-factory-reset`: per-emulator backup and atomic restore;
- Pixel2 frontend capability policy for audio output, lid, and network services.

## Host evidence

- `tests/test-app-layer-scripts.sh`: pass;
- `tests/test-implementation-audit.sh`: pass;
- `docker-build.sh frontend`: pass;
- `docker-build.sh app-layer --strict`: pass;
- implementation audit: release blockers reduced from 22 to 14;
- frontend component checksum contains all three helpers and `bin/fsck.fat`;
- app-layer root checksum contains normalized `factory-defaults/{ra,pico,sa}`.

## Pixel2 contract

| Function | Contract |
| --- | --- |
| Time | `/dev/rtc0` (`rk808-rtc`/RK817), RTC stored in UTC, network call bounded to 8 seconds |
| Storage | `/mnt/plumos-user` (`/dev/mmcblk0p3`, FAT32), `fsck.fat -n`, maximum 45 seconds; a live RW mount is reported as inconclusive because Linux clears the FAT clean-shutdown bit while mounted |
| Factory Reset | defaults contain paths relative to `/mnt/plumos`; existing settings are backed up before atomic replacement |
| Audio Output | RK817 speaker route only; no false Speaker/Headphone selector |
| Lid | Pixel2 has no lid switch; no Lid Suspend selector |
| Network services | only image-owned SSH and ADB are selectable; absent FTP/SFTP/Samba daemons are not advertised |

## Device gates still required

- RTC store/read and persistence across reboot;
- automatic time with working USB network and bounded failure without network;
- RO-mounted clean and deliberately dirty FAT32 status behavior; RW-mounted media must remain inconclusive rather than produce a false repair warning;
- RA, PicoArch, PPSSPP, and DraStic reset backup/restore;
- START menu rendering and navigation after live deployment.
