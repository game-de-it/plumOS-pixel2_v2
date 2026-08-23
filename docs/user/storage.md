# SD card and folders

[日本語](storage.ja.md)

## Partition layout

The distributed image starts compact. On first boot it creates the final
layout automatically:

| Volume | Format | Purpose |
| --- | --- | --- |
| `PLUMOS_BOOT` | FAT32 | Boot files and A/B System images; do not edit |
| `PLUMOS_SYS` | ext4 | plumOS Runtime and Linux state; not a content volume |
| `PLUMOS_USER` | FAT32 | ROMs, BIOS files, media, saves, and update packages |

macOS and Windows should use only `PLUMOS_USER` for normal file management.

## User folders

| Folder | Purpose |
| --- | --- |
| `roms/` | System-specific game folders |
| `bios/` | User-supplied emulator BIOS and firmware |
| `Images/` | Scraped and manually supplied artwork |
| `Themes/` | User themes and related assets |
| `Screenshots/` | Frontend and emulator screenshots |
| `Music/` | Music Player content |
| `updates/` | Signed plumOS update packages |
| `imports/`, `exports/` | File Manager import/export staging |
| `plumos-logs/` | Persistent diagnostic logs exported for support |

System folders under `roms/` use the names created by plumOS, such as `FC`,
`SFC`, `GB`, `GBA`, `PS`, `NEOGEO`, or `PSP`. Keep archive-based arcade ROMs
zipped unless the emulator documentation specifically requires extraction.

## Safe handling and backups

- Shut down before removing the SD card.
- Eject the volume from the computer before removing the reader.
- Do not rename or format `PLUMOS_BOOT` or `PLUMOS_SYS`.
- Back up ROM-independent saves, states, screenshots, and configuration before
  replacing a card or applying a recovery image.
- Never copy a host-side factory tree over the complete card; it can overwrite
  saves, Wi-Fi credentials, PortMaster games, and user settings.
