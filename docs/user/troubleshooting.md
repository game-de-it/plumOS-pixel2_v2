# Troubleshooting

[日本語](troubleshooting.ja.md)

## The frontend does not start

- Wait for first-boot setup or an update screen to finish; one automatic restart
  can be normal during first setup.
- If Pixel2 shows `NO SD`, power off, remove and reinsert the card, then try a
  known-good branded SD written from the verified image.
- A repeated `NO SD` or kernel MMC tuning/I/O message points to the card or
  contact and is not repaired by changing emulator settings.

## Games are missing or do not start

- Confirm the ROM is under the system folder created inside `roms/`.
- In UI Settings, use `Refresh TOP`, or reboot cleanly to rebuild the ROM list.
- Confirm required BIOS files are in `bios/` with the expected name and hash.
- For arcade games, use a ROM-set revision compatible with the selected core.
- Press SELECT on the game and try another packaged core only when needed.

## Wi-Fi does not connect

- Use a validated adapter and wait up to about 30 seconds for a cold first
  connection.
- Confirm the SSID and password, then check Network Information for an IP.
- Turn Wi-Fi off and on once. If the LED remains off, remove and reinsert the
  adapter or perform a normal reboot with the adapter attached.
- Wi-Fi cannot work while the single USB port is connected as a charger.

## Controls, display, or sound are wrong

- Exit the emulator and start the game again from the normal frontend route.
- Use FUNCTION to open the emulator menu and confirm the active profile.
- Restore that runtime's factory settings from System Settings if a manual
  configuration change caused the problem. This does not replace ROM files.
- If a specific system is affected, record the system, game, selected core,
  and whether the issue affects the game screen, menu, controls, or audio.

## Storage and logs

`START -> System Settings -> Storage Check` performs a bounded read-only health
check. It does not repair a mounted FAT32 volume. Persistent support logs are
available under `PLUMOS_USER/plumos-logs` after a clean shutdown.

When reporting a problem, include the plumOS version, SD card identity, exact
steps, visible error text, and whether the issue remains after a normal reboot.
Do not post Wi-Fi passwords, private keys, ROMs, or copyrighted BIOS files.
