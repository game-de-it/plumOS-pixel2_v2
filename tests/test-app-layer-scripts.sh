#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
for script in \
    scripts/docker-build.sh \
    scripts/build-frontend-component.sh \
    scripts/build-retroarch.sh \
    scripts/build-libretro-cores.sh \
    scripts/build-app-layer.sh \
    scripts/verify-app-layer.sh \
    package/app-layer-pixel2/bin/plumos-retroarch-launch \
    package/app-layer-pixel2/bin/plumos-safe-shutdown \
    package/app-layer-pixel2/bin/plumos-run-with-input-map; do
    bash -n "$ROOT_DIR/$script"
done
grep -q 'retroarch:quicknes' "$ROOT_DIR/package/frontend-pixel2/systems.json"
grep -q 'internal:system-information' \
    "$ROOT_DIR/package/frontend-pixel2/menus.json"
grep -q 'internal:network-information' \
    "$ROOT_DIR/package/frontend-pixel2/menus.json"
grep -q '"apps": \[\]' "$ROOT_DIR/package/frontend-pixel2/apps.json"
grep -q 'video_rotation = "3"' \
    "$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg"
grep -q '^input_a_btn = "1"$' \
    "$ROOT_DIR/package/retroarch-pixel2/gkd-pixel2-joypad.cfg"
grep -q '^input_b_btn = "0"$' \
    "$ROOT_DIR/package/retroarch-pixel2/gkd-pixel2-joypad.cfg"
grep -q 'SOURCE_COMMIT=69a4f0ea' "$ROOT_DIR/scripts/build-retroarch.sh"
grep -q 'required_library in libpthread.so.0' \
    "$ROOT_DIR/scripts/build-retroarch.sh"
grep -q 'emulator/lib/libpthread.so.0' \
    "$ROOT_DIR/scripts/verify-app-layer.sh"
grep -q '^PLUMOS_INPUT_A_CODE=305$' \
    "$ROOT_DIR/package/app-layer-pixel2/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_B_CODE=304$' \
    "$ROOT_DIR/package/app-layer-pixel2/config/system/input-map.env"
grep -q 'input-map.env' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'input-map.env' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-run-with-input-map"
grep -q 'east-confirm' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
grep -q 'strcmp(ab_layout, "east-confirm")' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'case BTN_TRIGGER_HAPPY1:' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'value=%d action=%s' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'open_power_menu_for_action(ui, "reboot")' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'open_power_menu_for_action(ui, "shutdown")' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'sysrq-trigger' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
grep -q 'rk817-dev-off' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
grep -q 'i2cset -f -y 0 0x20 0xf4' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
grep -q 'SOURCE_COMMIT=058d6651' "$ROOT_DIR/scripts/build-libretro-cores.sh"
grep -q '^complete=true$' "$ROOT_DIR/scripts/build-app-layer.sh"
if grep -R -E -i '(rocknix|emuelec|batocera|knulli|stockos)' \
    "$ROOT_DIR/package/frontend-pixel2" \
    "$ROOT_DIR/package/retroarch-pixel2" \
    "$ROOT_DIR/package/app-layer-pixel2" >/dev/null; then
    printf 'error: foreign distribution identity in Pixel2 app sources\n' >&2
    exit 1
fi
foreign_models='M[F]|V9[0]S|M[M]F|A3[0]|Miy[o]o'
if grep -R -I -E -i "(^|[^[:alnum:]_])($foreign_models)([^[:alnum:]_]|$)" \
    "$ROOT_DIR/package/frontend-pixel2" \
    "$ROOT_DIR/package/app-layer-pixel2" \
    "$ROOT_DIR/vendor/plumos-frontend"; then
    printf 'error: foreign device identity in Pixel2 frontend sources\n' >&2
    exit 1
fi
printf 'app_layer_scripts=result-ok\n'
