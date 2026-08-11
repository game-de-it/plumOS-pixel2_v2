# Pixel2 frontend input repair

Date: 2026-08-12
Scope: Pixel2 face-button contract, frontend action mapping, live deployment and
release-image validation

## Root cause

The stock DTB and the generated plumOS DTB used different Linux event codes for
the two lower face buttons. The physical A switch is GPIO3 PD1 and must emit
`BTN_EAST` (`0x131`); the physical B switch is GPIO3 PD2 and must emit
`BTN_SOUTH` (`0x130`). The generated DTB had only these two codes reversed.

The frontend already uses the Pixel2 Nintendo-style layout, where East is
confirm/A and South is back/B. The incorrect DTB therefore inverted the two
actions. X and Y have no TOP-screen navigation action, so testing all four face
buttons on TOP made the controller appear non-functional even though evdev was
delivering events.

The stock and corrected plumOS contracts are:

| Physical control | GPIO | Linux event code | Frontend action |
| --- | --- | --- | --- |
| A | GPIO3 PD1 | `BTN_EAST` (`0x131`) | A / confirm |
| B | GPIO3 PD2 | `BTN_SOUTH` (`0x130`) | B / back |
| X | GPIO3 PA5 | `BTN_NORTH` (`0x133`) | X |
| Y | GPIO3 PA6 | `BTN_WEST` (`0x134`) | Y |
| Function | GPIO3 PC5 | `BTN_TRIGGER_HAPPY1` (`0x2c0`) | Function / screenshot |

## Implementation

- `4b9f66d` restores the stock A/B event-code contract during Pixel2 DTB
  generation and makes the transform fail if the pinned input DTS changes
  unexpectedly.
- `330b6ca` adds the Pixel2 Function-key code to the frontend and records the
  mapped action name in both the runtime input trace and `--dump-events` output.
- `3cf4f90` fixes the app-layer assembler so a successful assembly containing
  all mandatory components is always emitted as `complete: true`.
- `672f70e` keeps legacy device names out of Pixel2 build/test source while
  preserving the app-layer identity rejection gate.

## Generated-artifact validation

The corrected DTB was decompiled after the kernel build. Its live nodes match
the table above. Kernel checksums passed for `Image`, DTB, kernel config and
source manifest.

- corrected DTB SHA-256:
  `56f0aa6fee9a7e5924716eb73c849834e143d6cd2b2141179e2d060112e0a268`
- frontend SHA-256:
  `028de93ed338f4a2d84e02e302990295ed236c85c7b3462c16ecf779005ce6da`
- release image:
  `output/image/pixel2/plumOS-Pixel2-0.1.0-dev.img`
- release image SHA-256:
  `574943ac8acbb6a6a2acc7c63c25a9f1422b528645bfd902f8c7eed05fb2fb13`
- image source ref: `3cf4f90`

The canonical `release-image` target rebuilt the frontend, RetroArch, QuickNES,
complete app layer, SYSTEM and SD image. The image verifier re-extracted and
verified the SYSTEM and app-layer payloads successfully.

## Live Pixel2 deployment

The corrected DTB was staged on STATE, copied to BOOT only after its hash was
verified, and BOOT was returned to read-only mode. The previous DTB remains at:

`/boot/rk3326s-gkd-pixel2.dtb.bak-4b9f66d`

After reboot, `/proc/device-tree/gkd-pixel2-joypad` reported the corrected event
codes and GPIOs. ADB reconnected automatically, the frontend started from the
new binary, and the app-layer root plus all three component checksum files
passed. The live app-layer manifest is `complete: true` at source ref
`3cf4f90`.

Runtime input evidence is written to:

`/run/plumos/frontend-input.log`

Each event now includes `action=A`, `action=B`, `action=X`, `action=Y` or
`action=FUNCTION`, so subsequent physical validation no longer requires
inferring the frontend mapping from raw numeric codes.

## Remaining physical gate

The following remains intentionally unchecked in `TODO.md` until observed on
the physical unit:

1. A opens the selected TOP, ROM or menu entry.
2. B returns to the previous screen.
3. START opens the Start menu and A confirms its entries.
4. Function produces the expected frontend action.
5. A/B/X/Y and the exit hotkey work inside RetroArch.
