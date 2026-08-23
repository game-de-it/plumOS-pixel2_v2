# Storage and Updates

[日本語](storage-and-updates.ja.md)

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

## Compact Seed and First Boot

The distributed image is 2,701,131,776 bytes and contains two MBR partitions:

| Partition | Seed geometry | First-boot geometry |
| --- | --- | --- |
| p1 `PLUMOS_BOOT` | sector 32768, 512 MiB FAT32 | unchanged |
| p2 `PLUMOS_SYS` | sector 1081344, 2048 MiB ext4 | expanded to exactly 8192 MiB |
| p3 `PLUMOS_USER` | absent | sector 17858560 through the physical-card end, FAT32 |

The minimum supported physical card is 16 GB (15,000,000,000 readable bytes
or larger). The provisioner runs after the retained stock initramfs mounts p2.
It validates exact p1/p2 boundaries, records a durable journal under
`/storage/provision`, replaces the seed MBR entries, updates the kernel view,
runs online `resize2fs`, formats only the newly owned p3 as `PLUMOS_USER`, and
creates `roms`, `bios`, `Images`, `Themes`, `Screenshots`, `Music`, `updates`,
`imports`, `exports`, and `plumos-logs`.

If the mounted p2 cannot be resized in the running kernel view, init performs
one synchronized early reboot and resumes from the observed table and journal.
After `/storage/provision/complete` and the final geometry agree, ordinary boot
does not rerun resize, format, seeding, or marker writes. An existing p3 is
never formatted: incompatible legacy geometry is logged and preserved for a
manual migration.

Before first boot a host sees only `PLUMOS_BOOT`. After provisioning completes
and the card is returned to macOS or Windows, `PLUMOS_USER` is the volume for
ROMs, BIOS files, images, themes, media, and update archives.

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
signed Runtime and System paths, including FE update interaction and
post-update health promotion, have been validated on a physical Pixel2.
Deliberate physical failure injection remains a separate destructive test when
explicitly scheduled; host fixtures continue to gate the rollback state
machine on every build.
