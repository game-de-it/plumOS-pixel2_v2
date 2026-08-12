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

## Implemented Update Contract

Pixel2 has a transactional update path with the same ownership goals as the
other plumOS handhelds, adapted to the stock boot substrate:

- `/SYSTEM` is a small immutable dispatcher because the stock initramfs always
  opens that exact filename;
- System generations are stored as `/system-slots/system-{a,b}.squashfs` on
  `PLUMOS_BOOT` and only the inactive slot is written;
- a complete readback SHA-256 is required before pending-slot metadata is
  committed;
- signed Runtime updates modify only managed app-layer paths on `PLUMOS_SYS`,
  keep one rollback generation, and use a write-ahead transaction journal;
- Ed25519 signature, device ID, vendor runtime, System ABI, Runtime ABI, source
  version, payload path, and package hash are checked before installation;
- System and Runtime generations become healthy only after the FE creates
  `/tmp/plumos-fe-ready` following its first successful render;
- an unpromoted System boot returns to the active slot, while an unpromoted or
  interrupted Runtime transaction restores the previous managed generation.

`/System/` cannot be used for slot storage because FAT32 treats it as colliding
with `/SYSTEM`. Update packages are staged in
`/mnt/plumos-user/updates`; the request journal itself is committed on ext4
under `/mnt/plumos/update-state` before safe reboot.

The host fixtures cover signature rejection, compatibility gates, interrupted
Runtime recovery, inactive-slot readback, and rollback state transitions. The
successful signed Runtime and System paths have also been validated on a
physical Pixel2. Remaining release acceptance is the FE menu interaction and
deliberate failure injection on a physical device.
