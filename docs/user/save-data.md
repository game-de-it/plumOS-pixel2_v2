# Save data, states, and screenshots

[日本語](save-data.ja.md)

## RetroArch

RetroArch normally stores saves beside the active ROM, organized by ROM folder
and core. Save RAM is flushed regularly, and the normal START + SELECT exit path
allows the final save and automatic exit state to finish writing.

| Physical buttons | Action |
| --- | --- |
| FUNCTION | Open the RetroArch menu |
| START + SELECT | Exit to the frontend |
| SELECT + L | Load state |
| SELECT + R | Save state |
| SELECT + D-pad Left/Right | Previous/next state slot |
| SELECT + X | Screenshot |
| SELECT + Y | Toggle FPS display |
| SELECT + L2 | Toggle slow motion |
| SELECT + R2 | Toggle fast forward |

Save-state slots keep multiple generations and thumbnails. An automatic state
is written on exit but is not automatically loaded at the next launch.

See [RetroArch saves and hotkeys](retroarch-saves-and-hotkeys.md) for the exact
fallback paths and a concrete directory example.

## Other runtimes

Standalone emulators, PICO-8, Pyxel, and PortMaster may use their own folders
under `PLUMOS_USER` or next to their content. Exit through the emulator menu or
the documented hotkey before copying files; copying an active save can produce
an incomplete backup.

Frontend and emulator screenshots are stored under `Screenshots/` or the
runtime's content-local screenshot folder.

## Back up and restore

1. Shut down Pixel2 from the POWER menu.
2. Connect the SD card to a computer.
3. Copy save, state, screenshot, and important app-data folders to another
   disk without changing their names.
4. To restore, use the same folder and core/profile layout.

Do not restore an entire old Runtime tree over a newer image. Restore only
device-owned data such as saves, states, screenshots, and content.
