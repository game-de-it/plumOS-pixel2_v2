# Basic operation

[日本語](operation.ja.md)

## Frontend controls

| Physical control | Frontend action |
| --- | --- |
| D-pad | Move through systems, games, and menus |
| A | Confirm, open a system, or start a game |
| B | Go back or cancel |
| X | Switch between the game list and Gallery view |
| Y | Add or remove the selected game from Favorites |
| START | Open the six-entry START menu |
| SELECT | Open core/emulator selection for the highlighted system or game |
| FUNCTION | Save a frontend screenshot |
| Volume - / + | Change global volume |
| SELECT + Volume - / + | Change screen brightness |
| Power | Open the global power menu |

The START menu contains `UI Settings`, `System Settings`, `Network Settings`,
`Apps`, `HELP`, and `POWER`. `POWER` opens Sleep, Reboot, Shutdown, and Cancel.

## Games and emulator menus

- A starts the highlighted game.
- FUNCTION opens the menu in RetroArch, PicoArch, and supported standalone
  emulators.
- RetroArch uses START + SELECT to exit to the frontend.
- The global Power button works in the frontend, games, and supported apps. It
  pauses the foreground program before showing Sleep, Reboot, or Shutdown.

Button mapping is normalized for Pixel2. Physical A is confirm and physical B
is back even when an emulator internally reports different SDL button numbers.

## Sleep and power

Choose Sleep from the power menu and press Power once to wake. In a game, the
same game process is resumed. If the stock kernel cannot enter hardware sleep,
plumOS uses software standby and still turns the panel fully off.

Use Shutdown before removing the SD card. If a charger is attached during
Shutdown, Pixel2 enters the stock charging screen. Without a charger it powers
off completely.
