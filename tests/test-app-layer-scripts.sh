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
    package/app-layer-pixel2/bin/plumos-ensure-udev-input-db \
    package/app-layer-pixel2/bin/plumos-safe-shutdown \
    package/app-layer-pixel2/bin/plumos-run-with-input-map \
    package/app-layer-pixel2/bin/plumos-hardware-keys-service \
    package/app-layer-pixel2/bin/plumos-display-control \
    package/app-layer-pixel2/bin/plumos-volume-control \
    package/app-layer-pixel2/bin/plumos-network-control \
    package/app-layer-pixel2/bin/plumos-network-services \
    package/app-layer-pixel2/bin/plumos-time-sync \
    package/app-layer-pixel2/bin/plumos-storage-health \
    package/app-layer-pixel2/bin/plumos-factory-reset; do
    bash -n "$ROOT_DIR/$script"
done
PYTHONPYCACHEPREFIX="${TMPDIR:-/tmp}/plumos-pixel2-test-pycache" \
    python3 -m py_compile "$ROOT_DIR/scripts/generate-pixel2-system-logos.py"
grep -q 'retroarch:quicknes' "$ROOT_DIR/package/frontend-pixel2/systems.json"
grep -q 'retroarch:gambatte' "$ROOT_DIR/package/frontend-pixel2/systems.json"
grep -q 'retroarch:pcsx_rearmed' "$ROOT_DIR/package/frontend-pixel2/systems.json"
grep -q 'picoarch:quicknes' "$ROOT_DIR/package/frontend-pixel2/systems.json"
grep -q 'internal:system-settings' \
    "$ROOT_DIR/package/frontend-pixel2/menus.json"
grep -q 'internal:network-settings' \
    "$ROOT_DIR/package/frontend-pixel2/menus.json"
grep -q 'menu:apps' \
    "$ROOT_DIR/package/frontend-pixel2/menus.json"
grep -q '"id": "pyxel_setup"' "$ROOT_DIR/package/frontend-pixel2/apps.json"
grep -q 'video_rotation = "3"' \
    "$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg"
grep -q 'video_force_aspect = "true"' \
    "$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg"
grep -q 'aspect_ratio_index = "0"' \
    "$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg"
grep -q 'input_joypad_driver = "udev"' \
    "$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg"
grep -q 'input_player1_analog_dpad_mode = "0"' \
    "$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg"
grep -q 'input_player1_down_btn = "11"' \
    "$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg"
grep -q '^input_a_btn = "1"$' \
    "$ROOT_DIR/package/retroarch-pixel2/pixel2-joypad-udev.cfg"
grep -q '^input_b_btn = "0"$' \
    "$ROOT_DIR/package/retroarch-pixel2/pixel2-joypad-udev.cfg"
grep -q '^input_start_btn = "9"$' \
    "$ROOT_DIR/package/retroarch-pixel2/pixel2-joypad-udev.cfg"
grep -q '^input_down_axis = "+1"$' \
    "$ROOT_DIR/package/retroarch-pixel2/pixel2-joypad-udev.cfg"
grep -q '^input_down_btn = "11"$' \
    "$ROOT_DIR/package/retroarch-pixel2/pixel2-joypad-udev.cfg"
grep -q '^input_l_y_plus_axis = "+1"$' \
    "$ROOT_DIR/package/retroarch-pixel2/pixel2-joypad-udev.cfg"
grep -q '^input_device = "pixel2_joypad"$' \
    "$ROOT_DIR/package/retroarch-pixel2/pixel2-joypad-udev.cfg"
grep -q 'SOURCE_COMMIT=69a4f0ea' "$ROOT_DIR/scripts/build-retroarch.sh"
grep -q 'drm_set_rotation' \
    "$ROOT_DIR/patches/retroarch/014-pixel2-drm-software-rotation.patch"
grep -q 'logical_width' \
    "$ROOT_DIR/patches/retroarch/014-pixel2-drm-software-rotation.patch"
grep -q 'required_library in libpthread.so.0' \
    "$ROOT_DIR/scripts/build-retroarch.sh"
grep -q 'emulator/lib/libpthread.so.0' \
    "$ROOT_DIR/scripts/verify-app-layer.sh"
grep -q '^PLUMOS_INPUT_A_CODE=305$' \
    "$ROOT_DIR/package/app-layer-pixel2/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_B_CODE=304$' \
    "$ROOT_DIR/package/app-layer-pixel2/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_A_UDEV_BTN=1$' \
    "$ROOT_DIR/package/app-layer-pixel2/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_B_UDEV_BTN=0$' \
    "$ROOT_DIR/package/app-layer-pixel2/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_DOWN_UDEV_AXIS=+1$' \
    "$ROOT_DIR/package/app-layer-pixel2/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_DOWN_UDEV_BTN=11$' \
    "$ROOT_DIR/package/app-layer-pixel2/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_ANALOG_DPAD_MODE=none$' \
    "$ROOT_DIR/package/app-layer-pixel2/config/system/input-map.env"
grep -q 'input-map.env' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'ID_INPUT_JOYSTICK=1' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-ensure-udev-input-db"
grep -q 'plumos-ensure-udev-input-db' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'plumos-audio-output' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'ALSA_PLUGIN_DIR' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'PLUMOS_AUDIO_FAST_FORWARD_DROP=1' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'PLUMOS_AUDIO_FAST_FORWARD_STATE' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'audio_device = "%s"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'pcm.plumos_output' \
    "$ROOT_DIR/package/audio-router-pixel2/plumos/bin/plumos-audio-output"
grep -q 'factory-defaults/alsa/alsa.conf' \
    "$ROOT_DIR/package/audio-router-pixel2/plumos/bin/plumos-audio-output"
grep -q 'pcm.plumos_hw_card' \
    "$ROOT_DIR/package/audio-router-pixel2/plumos/bin/plumos-audio-output"
grep -q 'PLUMOS_PIXEL2_INTERNAL_CARD_ID' \
    "$ROOT_DIR/package/audio-router-pixel2/plumos/bin/plumos-audio-output"
grep -q 'plumos_%s_card%d' \
    "$ROOT_DIR/package/audio-router-pixel2/plumos_hotplug.c"
grep -q 'snd_pcm_avail_update' \
    "$ROOT_DIR/package/audio-router-pixel2/plumos_hotplug.c"
grep -q 'plumOS Pixel2 hotplug audio' \
    "$ROOT_DIR/package/audio-router-pixel2/plumos_hotplug.c"
grep -q 'PLUMOS_RETROARCH_CPU_GOVERNOR:-}' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'unsupported CPU governor' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'scaling_available_governors' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'audio_latency = "%s"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'video_threaded = "true"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'video_refresh_rate = "%s"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'vrr_runloop_enable = "%s"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q '#define VOLUME_PROBE_INTERVAL 0' \
    "$ROOT_DIR/package/audio-router-pixel2/plumos_hotplug.c"
grep -q 'pcm->volume_level < VOLUME_MAX' \
    "$ROOT_DIR/package/audio-router-pixel2/plumos_hotplug.c"
grep -q 'input-map.env' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-run-with-input-map"
grep -q 'east-confirm' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
grep -q 'plumos-hardware-keys-service' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/40-frontend"
grep -q 'plumos_pixel2_hardware_keys.c' \
    "$ROOT_DIR/scripts/build-frontend-component.sh"
grep -q 'pixel2_joypad' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q 'gpio-keys' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q 'KEY_VOLUMEUP' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q 'BTN_SELECT' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
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
grep -q 'docker/pixel2-tools/libretro-core-recipes.tsv' "$ROOT_DIR/scripts/build-libretro-cores.sh"
grep -q 'quicknes|A|https://github.com/libretro/QuickNES_Core.git|058d6651' \
    "$ROOT_DIR/docker/pixel2-tools/libretro-core-recipes.tsv"
grep -q 'pcsx_rearmed|A|https://github.com/libretro/pcsx_rearmed.git|d26eaee5' \
    "$ROOT_DIR/docker/pixel2-tools/libretro-core-recipes.tsv"
grep -q 'component: "libretro-cores"' "$ROOT_DIR/scripts/build-libretro-cores.sh"
grep -q '^complete=true$' "$ROOT_DIR/scripts/build-app-layer.sh"
grep -q 'PLUMOS_STORAGE_ROOT:-/mnt/plumos-user' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-storage-health"
grep -q '"$FSCK" -n "$device"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-storage-health"
grep -q 'PLUMOS_RTC_DEVICE:-/dev/rtc0' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-time-sync"
grep -q 'hwclock -u -f "$RTC_DEVICE" -w' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-time-sync"
grep -q 'factory-defaults/ra/config/retroarch' \
    "$ROOT_DIR/scripts/build-app-layer.sh"
grep -q 'factory-defaults/pico/config/standalone' \
    "$ROOT_DIR/scripts/build-app-layer.sh"
grep -q 'factory-defaults/sa/state/standalone' \
    "$ROOT_DIR/scripts/build-app-layer.sh"
grep -q 'if (!runtime_device_is_pixel2())' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'generate-pixel2-system-logos.py' \
    "$ROOT_DIR/scripts/build-frontend-component.sh"

feature_tmp="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-feature-test.XXXXXX")"
trap 'rm -rf "$feature_tmp"' EXIT
mkdir -p "$feature_tmp/card/roms/nes" "$feature_tmp/plumos"
touch "$feature_tmp/card/roms/nes/.DS_Store"
PLUMOS_ROOT="$feature_tmp/plumos" \
PLUMOS_SDCARD_ROOT="$feature_tmp/card" \
PLUMOS_RUNTIME_ROOT="$feature_tmp/run" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-sdcard-cleanup" \
    --force --dry-run >"$feature_tmp/cleanup-dry-run.log"
grep -q 'result=ok files=1 dirs=0 failed=0 dry_run=1' \
    "$feature_tmp/cleanup-dry-run.log"
test -f "$feature_tmp/card/roms/nes/.DS_Store"
PLUMOS_ROOT="$feature_tmp/plumos" \
PLUMOS_SDCARD_ROOT="$feature_tmp/card" \
PLUMOS_RUNTIME_ROOT="$feature_tmp/run" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-sdcard-cleanup" \
    --force >"$feature_tmp/cleanup.log"
test ! -e "$feature_tmp/card/roms/nes/.DS_Store"
"$ROOT_DIR/scripts/generate-pixel2-system-logos.py" "$feature_tmp/logos"
for logo in arduboy megaduck puzzlescript superbroswar; do
    file "$feature_tmp/logos/$logo.png" | grep -q 'PNG image data, 190 x 156'
done
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
