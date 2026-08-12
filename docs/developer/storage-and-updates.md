# Storage and Updates

## Current Partition Contract

Pixel2 currently uses the stock-compatible Rockchip prefix plus:

| Mount | Label | Role |
| --- | --- | --- |
| `/boot` | `PLUMOS_BOOT` | stock-compatible boot files and `SYSTEM` |
| `/mnt/plumos` | `PLUMOS_SYS` | app-layer runtime and Linux state |
| `/mnt/plumos-user` | `PLUMOS_USER` | ROMs, BIOS, media, screenshots, updates |

`/boot` is normally read-only. `/mnt/plumos` is ext4 and preserves POSIX
permissions for the runtime. `/mnt/plumos-user` is the host-readable content
volume.

## Managed and Device-Owned Data

Managed files are covered by `/mnt/plumos/checksums.sha256`. Device-owned
mutable paths include active settings, ROMs, BIOS files, saves, states, logs,
credentials, SSH state, and future app state.

Live deploys must update managed files and metadata together. They must not
overwrite active user data.

## Update Status

The final MF/V90S-style transactional update system is not implemented yet for
Pixel2. The target design remains:

- fixed System A/B slots on the boot volume;
- signed Runtime updates on `PLUMOS_SYS`;
- health promotion after FE renderer readiness;
- rollback on failed pending boot or interrupted Runtime transaction.

The Pixel2 stock initramfs always opens `/SYSTEM`. The final boot contract
therefore keeps that filename as a small immutable dispatcher and stores the
case-insensitively distinct slot images below `/system-slots/`. A `/System/`
directory is invalid on FAT32 because it collides with `/SYSTEM`.

Until that lands, generated SD images and careful metadata-complete live deploys
are the supported development paths.
