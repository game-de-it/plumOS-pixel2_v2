#!/usr/bin/env bash
set -euo pipefail

ROOT="${1:-}"
[ -d "$ROOT" ] || { printf 'usage: %s APP_LAYER_ROOT\n' "$0" >&2; exit 2; }
for path in \
    manifest.json checksums.sha256 VERSION COMPAT_VENDOR RUNTIME_ABI \
    bin/plumos-frontend-pixel2 bin/plumos-library-scan bin/plumos-text-ui \
    bin/plumos-retroarch-launch bin/plumos-ensure-udev-input-db \
    bin/retroarch cores/quicknes_libretro.so \
    bin/plumos-safe-shutdown bin/plumos-run-with-input-map \
    bin/plumos-hardware-keys bin/plumos-hardware-keys-service \
    bin/plumos-display-control bin/plumos-volume-control \
    bin/plumos-network-control bin/plumos-network-services \
    emulator/lib/libpthread.so.0 \
    factory-defaults/alsa/alsa.conf \
    config/frontend/systems.json factory-defaults/retroarch/retroarch.cfg \
    factory-defaults/retroarch/autoconfig/udev/pixel2_joypad.cfg \
    config/system/input-map.env config/system/input-map.json \
    components/frontend/manifest.json components/retroarch/manifest.json \
    components/libretro-cores/manifest.json; do
    [ -f "$ROOT/$path" ] || { printf 'error: app-layer file missing: %s\n' "$path" >&2; exit 1; }
done
(cd "$ROOT" && sha256sum -c checksums.sha256 >/dev/null)
(cd "$ROOT" && sha256sum -c components/frontend/checksums.sha256 >/dev/null)
(cd "$ROOT" && sha256sum -c components/retroarch/checksums.sha256 >/dev/null)
(cd "$ROOT" && sha256sum -c components/libretro-cores/checksums.sha256 >/dev/null)
grep -q '"device": "pixel2"' "$ROOT/manifest.json"
grep -q '"complete": true' "$ROOT/manifest.json"
grep -q '"retroarch:quicknes"' "$ROOT/config/frontend/systems.json"
grep -q '^input_device = "pixel2_joypad"$' \
    "$ROOT/factory-defaults/retroarch/autoconfig/udev/pixel2_joypad.cfg"
grep -q '^input_joypad_driver = "udev"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^video_rotation = "3"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^video_force_aspect = "true"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^aspect_ratio_index = "0"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^config_save_on_exit = "false"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '"device": "pixel2"' "$ROOT/config/frontend/menus.json"
grep -q 'ID_INPUT_JOYSTICK=1' "$ROOT/bin/plumos-ensure-udev-input-db"
grep -q '^PLUMOS_INPUT_AB_LAYOUT=east-confirm$' "$ROOT/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_A_CODE=305$' "$ROOT/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_B_CODE=304$' "$ROOT/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_A_UDEV_BTN=1$' "$ROOT/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_B_UDEV_BTN=0$' "$ROOT/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_DOWN_UDEV_AXIS=+1$' "$ROOT/config/system/input-map.env"
grep -q '"ab_layout": "east-confirm"' "$ROOT/config/system/input-map.json"
grep -q '"udev": {' "$ROOT/config/system/input-map.json"
grep -q '"action": "internal:system-settings"' \
    "$ROOT/config/frontend/menus.json"
grep -q '"action": "internal:network-settings"' \
    "$ROOT/config/frontend/menus.json"
grep -q '"action": "menu:apps"' \
    "$ROOT/config/frontend/menus.json"
if grep -Eq 'plumos-(nextcommander|music-player|retroarch-menu|pyxel|portmaster)' \
    "$ROOT/config/frontend/apps.json"; then
    printf 'error: unavailable application exposed by Pixel2 frontend\n' >&2
    exit 1
fi
foreign_models='M[F]|V9[0]S|M[M]F|A3[0]|Miy[o]o'
if find "$ROOT/config" "$ROOT/share" "$ROOT/licenses" -type f \
    \( -name '*.json' -o -name '*.lang' -o -name '*.manifest' \
       -o -name '*.txt' -o -name 'LICENSE' \) -exec \
    grep -EIil "(^|[^[:alnum:]_])($foreign_models)([^[:alnum:]_]|$)" {} + | \
    grep -q .; then
    printf 'error: foreign device identity in Pixel2 user-facing files\n' >&2
    exit 1
fi
if strings "$ROOT/bin/plumos-frontend-pixel2" "$ROOT/bin/plumos-text-ui" | \
    grep -Eiq "(^|[^[:alnum:]_])($foreign_models)([^[:alnum:]_]|$)"; then
    printf 'error: foreign device identity in Pixel2 frontend binaries\n' >&2
    exit 1
fi
if strings "$ROOT/bin/plumos-hardware-keys" | \
    grep -Eiq "(^|[^[:alnum:]_])($foreign_models)([^[:alnum:]_]|$)"; then
    printf 'error: foreign device identity in Pixel2 hardware key daemon\n' >&2
    exit 1
fi
if find "$ROOT" -type f \( -iname '*.nes' -o -iname '*.gb' -o -iname '*.gba' \
    -o -iname '*.sfc' -o -iname '*.smc' -o -iname '*.bin' -o -iname '*.cue' \) |
    grep -q .; then
    printf 'error: ROM or BIOS-like content in app layer\n' >&2
    exit 1
fi
if grep -R -a -E -i -n 'rocknix|emuelec|batocera|knulli|stockos' \
    "$ROOT" >/dev/null; then
    printf 'error: foreign distribution identity in app layer\n' >&2
    exit 1
fi
file "$ROOT/bin/retroarch" "$ROOT/cores/quicknes_libretro.so" |
    grep -q 'ARM aarch64'
if [ "$(uname -m)" = aarch64 ]; then
    LD_LIBRARY_PATH="$ROOT/emulator/lib:$ROOT/frontend/lib" \
        "$ROOT/bin/retroarch" --version | grep -q '^Version: 1\.22\.2'
    "$ROOT/bin/plumos-text-ui" --help >/dev/null

    runtime_tmp=$(mktemp -d /tmp/plumos-pixel2-app-verify.XXXXXX)
    trap 'rm -rf "$runtime_tmp"' EXIT
    mkdir -p "$runtime_tmp/app/state" "$runtime_tmp/roms/nes"
    ln -s "$ROOT/bin" "$runtime_tmp/app/bin"
    ln -s "$ROOT/cores" "$runtime_tmp/app/cores"
    ln -s "$ROOT/config" "$runtime_tmp/app/config"
    PLUMOS_FRONTEND_MODE=manual \
        PLUMOS_ROOT="$runtime_tmp/app" \
        PLUMOS_SDCARD_ROOT="$runtime_tmp" \
        "$ROOT/bin/plumos-frontend-diagnostics" >/dev/null
    printf 'NES-test-fixture' >"$runtime_tmp/roms/nes/test.nes"
    PLUMOS_ROOT="$runtime_tmp/app" \
        PLUMOS_SDCARD_ROOT="$runtime_tmp" \
        PLUMOS_ROM_ROOT="$runtime_tmp/roms" \
        "$ROOT/bin/plumos-library-scan" >/dev/null
    PLUMOS_ROOT="$runtime_tmp/app" \
        PLUMOS_SDCARD_ROOT="$runtime_tmp" \
        PLUMOS_ROM_ROOT="$runtime_tmp/roms" \
        "$ROOT/bin/plumos-text-ui" launch nes nes/test.nes --no-scan \
        >"$runtime_tmp/launch-plan.txt"
    grep -q '^launch_profile: retroarch:quicknes$' "$runtime_tmp/launch-plan.txt"
    grep -q '^can_execute: yes$' "$runtime_tmp/launch-plan.txt"
    rm -rf "$runtime_tmp"
    trap - EXIT
fi
printf 'app_layer_verify=result-ok root=%s\n' "$ROOT"
