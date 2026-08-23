# Pixel2 Frontend Feature Contract

[日本語](frontend-feature-audit.ja.md)

## Scope

Pixel2 follows the shared plumOS frontend surface while mapping it to real
Pixel2 hardware. Capabilities that do not exist—such as a lid switch, multiple
hardware audio outputs, or built-in Wi-Fi—are not copied as empty UI. Shared
Apps and file-transfer services must not be hidden merely to bypass a release
blocker.

## Historical Root Cause

The feature loss found during the 2026-08-14 audit was caused by provisional
bring-up code entering release evaluation, not by RK3326 performance or a stock
kernel limitation:

- UI and boot services used different network configuration paths;
- FTP, SFTP, and Samba were hidden and their backends reported
  `not_installed`;
- only Scraping and Pyxel Setup were exposed while missing shared Apps could
  still pass the release gate;
- the earlier ADB experiment could leave a fresh image with no maintenance
  path. ADB was later retired in favor of the Wi-Fi-priority product policy.

## Required Surface

The machine-readable source of truth is
`package/frontend-pixel2/feature-contract.json`.

- START: UI Settings, System Settings, Network Settings, Apps, HELP, POWER.
- POWER: Sleep, Reboot, Shutdown, and Cancel. START does not duplicate Reboot
  and Shutdown.
- Apps: Scraping, File Manager, Music Player, RetroArch, Pyxel Setup,
  PortMaster, and Update PortMaster.
- Ports: `roms/ports/*.sh` routes through the Pixel2 PortMaster launcher.
- Network Services: SSH, FTP, SFTP, and Samba.
- Network Settings: USB Wi-Fi connection, service controls, and connection
  information.
- Settings: the shared surface filtered by actual Pixel2 capabilities.

`scripts/audit-pixel2-implementation.py --release-gate` checks catalogs,
settings, handlers, four network services, and mandatory components.
`scripts/verify-app-layer.sh` verifies that every visible App launcher exists
and is executable and that component checksums match. Hiding an unimplemented
required item is not an accepted release strategy.

## Persistence and Boot

Service choices live at `/mnt/plumos/config/network/services.conf` and
`35-network-services` restores enabled services at boot. SSH defaults to ON on
a fresh image. The single port prioritizes USB Wi-Fi only while a downstream
adapter exists and returns to stock OTG for charging when it does not. The FE
does not expose ADB or a USB Mode selector.

Migration removes only retired `usb_mode`, `adb_enabled`, and FAT recovery
markers. It preserves SSID/PSK, service settings, ROMs, BIOS files, and saves.

## Device Acceptance

LCD, physical controls, USB Wi-Fi, SSH/SFTP/FTP/Samba over LAN, App exit, and
FE reacquisition have been exercised on physical Pixel2 hardware. Host gates
continue to catch surface, route, payload, and persistence regressions, but do
not replace physical display, input, and audio checks for each release
candidate.
