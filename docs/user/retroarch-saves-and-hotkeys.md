# RetroArch Saves and Hotkeys

## Hotkeys

Gameplay hotkeys use SELECT as the modifier unless noted otherwise.

| Physical buttons | Action |
| --- | --- |
| FUNCTION | Open the RetroArch menu |
| START + SELECT | Open the RetroArch menu (fallback) |
| SELECT + L | Load state |
| SELECT + R | Save state |
| SELECT + D-pad Left/Right | Previous/next state slot |
| SELECT + X | Screenshot |
| SELECT + Y | Toggle FPS display |
| SELECT + L2 | Toggle slow motion |
| SELECT + R2 | Toggle fast forward |

Use the menu to close content or quit RetroArch cleanly. A clean exit allows
normal save data and the automatic exit state to finish writing.

## Save locations

By default RetroArch stores saves on the same filesystem as the active ROM and
sorts them by ROM folder and core. For a ROM at:

```text
roms/FC/Akumajou Densetsu.nes
```

QuickNES uses a layout similar to:

```text
roms/FC/FC/QuickNES/
  Akumajou Densetsu.srm
  Akumajou Densetsu.state.auto
  Akumajou Densetsu.state.auto.png
```

Normal save RAM is flushed every 10 seconds. Save-state slots are automatically
indexed, keep up to 20 generations, and include thumbnails. An automatic state
is saved on exit but is not loaded automatically at the next start.

If content-local saving is disabled in RetroArch, the fallback locations are:

```text
/mnt/plumos/saves/<system>/
/mnt/plumos/states/<system>/
```

Updates and factory-setting migrations do not delete existing fallback saves or
states. Exit the game before copying or replacing save data.
