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

## PCSX menu input de-duplication

The first physical PCSX menu test exposed a second issue after Function could
open the menu: the PCSX process held `/dev/input/event2` twice. One descriptor
came from the Pixel2 raw evdev driver and the other from SDL joystick probing.
libpicofe merges menu state across registered devices, so the same physical
press and release could enter its menu path twice.

Commit `002e250` retains SDL video and keyboard event handling but disables SDL
joystick probing for the Pixel2 build. The controller, D-pad, and Function menu
now share one raw evdev source. The menu contract remains physical A/`BTN_EAST`
for confirm, physical B/`BTN_SOUTH` for back, and the D-pad for navigation.
`tests/test-pixel2-emulator-menu-contract.sh`, the app-layer script gate, both
PCSX/libpicofe patch applicability checks, and a full four-standalone parallel
build passed.

The strict app-layer and signed delta reported:

```text
version=0.1.0-dev-002e250
source_ref=002e250
package=plumos-pixel2-runtime-0.1.0-dev-002e250.tar.gz
package_sha256=2f72fd2774f65956b661d263dcae59e35dd36b9b4aa9adac7831b0d802ee1f93
payload_files=12
deleted_files=0
```

The device verified the signed package, base version, Pixel2 compatibility,
ABI, and package readback hash before reboot. The transaction reached
`healthy` after FE first render, removed `runtime-pending.json`, and matched the
host PCSX binary SHA-256
`143c9d97e627a622cb61bf925f94fd4e398a4343193fc2225ca0ad0356fcc44b`.
All 3470 root app-layer checksum entries passed. Physical PCSX menu navigation,
confirm, back, resume, and exit remain an operator gate.

## PCSX physical D-pad correction

An on-device capture taken while the PCSX menu owned the foreground recorded
physical D-pad Up/Down as Linux key codes 544/545 (`BTN_DPAD_UP` and
`BTN_DPAD_DOWN`), not `KEY_UP`/`KEY_DOWN` or `ABS_X`/`ABS_Y`. Physical A and B
were correctly reported as 305/304. The PCSX evdev defaults and menu key map now
include all four `BTN_DPAD_*` codes while retaining the prior `KEY_*` path for
compatibility.

Commit `3234b0d` was built as the complete four-emulator standalone component,
assembled into strict app-layer `0.1.0-dev-3234b0d`, and applied as a signed
12-file Runtime delta with no deletions. The update reached `healthy`; the
device PCSX SHA-256 was
`bf9fb6e898c4d45895b975b53a04650ec5b2f80d41e6e86aa34fc1f2dd7249b9`,
and all 3470 app-layer checksums passed. The operator then confirmed that the
PCSX Function menu can be navigated and controlled. Menu exit, clean FE return,
second launch, audio, and save persistence remain separate acceptance checks.
