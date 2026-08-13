#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
RA_CFG="$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg"
RA_AUTOCONFIG="$ROOT_DIR/package/retroarch-pixel2/pixel2-joypad-udev.cfg"
RA_LAUNCHER="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
INPUT_ENV="$ROOT_DIR/package/app-layer-pixel2/config/system/input-map.env"
PICO_PATCH="$ROOT_DIR/docker/pixel2-tools/picoarch/picoarch-pixel2-input-aspect.patch"
PCSX_PATCH="$ROOT_DIR/patches/pcsx_rearmed/pcsx-rearmed-r26l-pixel2-evdev-menu.patch"
PCSX_PICOFE_PATCH="$ROOT_DIR/patches/pcsx_rearmed/libpicofe-r26l-pixel2-input.patch"
OPENBOR_PATCH="$ROOT_DIR/patches/openbor/openbor-v6391-pixel2-sdl.patch"
SA_BUILD="$ROOT_DIR/scripts/build-standalone-pixel2.sh"
SA_LAUNCHER="$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
PPSSPP_CONTROLS="$ROOT_DIR/package/standalone-pixel2/plumos/factory-defaults/standalone/ppsspp/PSP/SYSTEM/controls.ini"
PPSSPP_CONTROLLER_PATCH="$ROOT_DIR/patches/ppsspp/ppsspp-1.20.4-pixel2-controller.patch"
PPSSPP_DISPLAY_PATCH="$ROOT_DIR/patches/ppsspp/ppsspp-1.20.4-pixel2-display-rotation.patch"

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    exit 1
}

grep -Fqx 'PLUMOS_INPUT_FUNCTION_CODE=704' "$INPUT_ENV" ||
    fail 'raw Pixel2 FUNCTION code is not documented'
grep -Fqx 'PLUMOS_INPUT_FUNCTION_UDEV_BTN=14' "$INPUT_ENV" ||
    fail 'RetroArch compact FUNCTION index is not documented'

grep -Fqx 'input_menu_toggle_btn = "14"' "$RA_CFG" ||
    fail 'RetroArch persistent FUNCTION menu binding is missing'
grep -Fqx 'input_menu_toggle_btn = "14"' "$RA_AUTOCONFIG" ||
    fail 'RetroArch controller FUNCTION menu binding is missing'
if grep -Eq '^input_(enable_hotkey|exit_emulator)_btn[[:space:]]*=' "$RA_AUTOCONFIG"; then
    fail 'RetroArch controller menu must not be gated by Select'
fi
grep -Fq 'PLUMOS_INPUT_FUNCTION_UDEV_BTN:-14' "$RA_LAUNCHER" ||
    fail 'RetroArch launch append config does not enforce FUNCTION menu'

for binding in \
    'BTN_TRIGGER_HAPPY1, IN_BINDTYPE_EMU, EACTION_MENU' \
    'BTN_TRIGGER_HAPPY1, PBTN_MENU'; do
    grep -Fq "$binding" "$PICO_PATCH" ||
        fail "PicoArch FUNCTION menu binding missing: $binding"
done
if grep -Fq 'BTN_MODE' "$PICO_PATCH"; then
    fail 'PicoArch still treats Pixel2 FUNCTION as BTN_MODE'
fi

for binding in \
    'BTN_TRIGGER_HAPPY1, IN_BINDTYPE_EMU, SACTION_ENTER_MENU' \
    'BTN_TRIGGER_HAPPY1, PBTN_MENU' \
    'in_evdev_init(&in_pixel2_evdev_platform_data)'; do
    grep -Fq "$binding" "$PCSX_PATCH" ||
        fail "PCSX-ReARMed raw FUNCTION contract missing: $binding"
done
for binding in \
    'BTN_DPAD_UP,        IN_BINDTYPE_PLAYER12, DKEY_UP' \
    'BTN_DPAD_DOWN,      IN_BINDTYPE_PLAYER12, DKEY_DOWN' \
    'BTN_DPAD_LEFT,      IN_BINDTYPE_PLAYER12, DKEY_LEFT' \
    'BTN_DPAD_RIGHT,     IN_BINDTYPE_PLAYER12, DKEY_RIGHT' \
    'BTN_DPAD_UP,        PBTN_UP' \
    'BTN_DPAD_DOWN,      PBTN_DOWN' \
    'BTN_DPAD_LEFT,      PBTN_LEFT' \
    'BTN_DPAD_RIGHT,     PBTN_RIGHT'; do
    grep -Fq "$binding" "$PCSX_PATCH" ||
        fail "PCSX-ReARMed physical D-pad contract missing: $binding"
done
for binding in \
    'Pixel2 controller input is owned by evdev' \
    'joycount = 0'; do
    grep -Fq "$binding" "$PCSX_PICOFE_PATCH" ||
        fail "PCSX-ReARMed single input path contract missing: $binding"
done

for binding in \
    'CONTROL_ESC                 (JOY_LIST_FIRST + 15)' \
    'CONTROL_DEFAULT1_UP         (JOY_LIST_FIRST + 1)' \
    'CONTROL_DEFAULT1_RIGHT      (JOY_LIST_FIRST + 2)' \
    'CONTROL_DEFAULT1_DOWN       (JOY_LIST_FIRST + 3)' \
    'CONTROL_DEFAULT1_LEFT       (JOY_LIST_FIRST + 4)' \
    'CONTROL_DEFAULT1_START      (JOY_LIST_FIRST + 14)' \
    'CONTROL_DEFAULT1_ESC        (JOY_LIST_FIRST + 15)'; do
    grep -Fq "$binding" "$OPENBOR_PATCH" ||
        fail "OpenBOR Pixel2 controller contract missing: $binding"
done

for contract in \
    'controls_b[CONTROL_INDEX_MENU] = 1154' \
    'controls_b[CONTROL_INDEX_MENU] = 1032'; do
    grep -Fq "$contract" "$SA_BUILD" ||
        fail "DraStic build menu migration missing: $contract"
    grep -Fq "$contract" "$SA_LAUNCHER" ||
        fail "DraStic live config migration missing: $contract"
done

grep -Eq '^Pause[[:space:]]*=.*(^|,)10-4(,|$)' "$PPSSPP_CONTROLS" ||
    fail 'PPSSPP factory FUNCTION pause binding is missing'
grep -Fq "s/$/,10-4/" "$SA_LAUNCHER" ||
    fail 'PPSSPP existing config migration is missing'
for contract in \
    'pixel2_joypad,a:b1,b:b0,x:b2,y:b3' \
    'back:b8,guide:b14,start:b9' \
    'dpup:b10,dpdown:b11,dpleft:b12,dpright:b13' \
    'leftshoulder:b4,rightshoulder:b5,lefttrigger:b6,righttrigger:b7'; do
    grep -Fq "$contract" "$PPSSPP_CONTROLLER_PATCH" ||
        fail "PPSSPP Pixel2 controller mapping missing: $contract"
done
grep -Fq 'PLUMOS_PIXEL2_DISPLAY_ROTATION' "$PPSSPP_DISPLAY_PATCH" ||
    fail 'PPSSPP Pixel2 display presenter patch is missing'
for contract in \
    'PLUMOS_PIXEL2_DISPLAY_ROTATION=ccw' \
    'PLUMOS_PIXEL2_DISPLAY_LOGICAL=640x480' \
    'PLUMOS_PIXEL2_DISPLAY_FORCE_LANDSCAPE=1'; do
    grep -Fq "$contract" "$SA_LAUNCHER" ||
        fail "PPSSPP Pixel2 display launch contract missing: $contract"
done
for patch_name in \
    ppsspp-1.20.4-pixel2-display-rotation.patch \
    ppsspp-1.20.4-pixel2-controller.patch; do
    grep -Fq "$patch_name" "$SA_BUILD" ||
        fail "PPSSPP build does not consume patch: $patch_name"
done

printf 'PASS: Pixel2 emulator FUNCTION menu contract\n'
