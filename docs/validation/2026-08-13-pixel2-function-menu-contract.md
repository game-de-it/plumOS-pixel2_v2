# Pixel2 emulator Function menu contract

Date: 2026-08-13

## Failure

PCSX-ReARMed launched a PlayStation title but the physical Function button did
not open its menu. The process also ignored the normal termination request, so
it was force-stopped over ADB and the frontend recovered.

The cross-runtime audit found that the problem was not isolated to PCSX:

- RetroArch had no Function menu binding;
- PicoArch used `BTN_MODE`, although Pixel2 reports `BTN_TRIGGER_HAPPY1`;
- PCSX depended on an assumed SDL button number;
- DraStic still carried the known stale menu value 1154;
- PPSSPP did not explicitly include its SDL Guide pause code;
- OpenBOR treated axis-derived D-pad inputs as buttons and assigned Function to
  screenshot instead of its Escape/menu action.

## Repair

The authoritative Pixel2 input contract remains the two-unit physical capture:
Function is Linux code 704 (`BTN_TRIGGER_HAPPY1`). The implementations now
derive their framework-specific mapping from that contract:

| Runtime | Function menu mapping |
| --- | --- |
| RetroArch | udev button 14 -> `input_menu_toggle_btn` |
| PicoArch | raw `BTN_TRIGGER_HAPPY1` -> `EACTION_MENU` |
| PCSX-ReARMed | raw `BTN_TRIGGER_HAPPY1` -> `SACTION_ENTER_MENU` |
| DraStic | SDL button 8 -> value 1032 |
| PPSSPP | SDL Guide -> Pause code `10-4` |
| OpenBOR | SDL button 10 -> Escape/menu input |

RetroArch keeps SELECT+START as an explicit exit chord, but Function does not
require SELECT. PPSSPP and DraStic launcher migrations only add the missing
Pause code or replace the single known stale value, preserving all other user
configuration.

## Host gate

The focused contract gate passes:

```text
tests/test-pixel2-emulator-menu-contract.sh
PASS: Pixel2 emulator FUNCTION menu contract
```

`tests/test-app-layer-scripts.sh` also passes. Physical confirmation remains a
separate gate after rebuilding and deploying the affected managed components.
