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

## Build and signed Runtime deployment

The corrected frontend, RetroArch, PicoArch, and standalone components were
rebuilt from clean commit `e9c8f38`. Component checksum verification passed for
all four outputs. The versioned strict app-layer reported:

```text
version=0.1.0-dev-e9c8f38
source_ref=e9c8f38
app_layer_verify=result-ok
```

A signed delta from the installed `0.1.0-dev-b33b877` generation contained 34
managed files and no deletions:

```text
package=plumos-pixel2-runtime-0.1.0-dev-e9c8f38.tar.gz
sha256=6fca0cca0174338ed7faf11aa3b048ab4dda3b951c014e39dfabbcd309c96783
payload_uncompressed_bytes=39698056
```

The device verified the signature, Pixel2 IDs, ABI, current version and
package hash before accepting the request. After safe reboot, the Runtime
transaction remained `pending_health` until the frontend's first render, then
became `healthy` and removed the pending marker. The frontend returned as one
process on `/dev/input/event2`.

Twenty-two targeted host/device hashes matched, including every changed
launcher, emulator binary, Function mapping, component manifest/checksum, and
root metadata file. Full on-device verification passed all 3470 root checksum
entries. Physical Function-menu interaction in each runtime remains unchecked
until observed by the operator.
