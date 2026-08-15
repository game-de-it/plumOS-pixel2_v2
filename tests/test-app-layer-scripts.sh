#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
for script in \
    scripts/docker-build.sh \
    scripts/build-frontend-component.sh \
    scripts/build-retroarch.sh \
    scripts/build-libretro-cores.sh \
    scripts/build-standalone-pixel2.sh \
    scripts/build-app-layer.sh \
    scripts/verify-app-layer.sh \
    package/app-layer-pixel2/bin/plumos-retroarch-config-merge \
    package/app-layer-pixel2/bin/plumos-retroarch-launch \
    package/app-layer-pixel2/bin/plumos-retroarch-menu-launch \
    package/app-layer-pixel2/bin/plumos-ensure-udev-input-db \
    package/app-layer-pixel2/bin/plumos-safe-shutdown \
    package/app-layer-pixel2/bin/plumos-power-menu-overlay \
    package/app-layer-pixel2/bin/plumos-run-with-input-map \
    package/app-layer-pixel2/bin/plumos-hardware-keys-service \
    package/app-layer-pixel2/bin/plumos-display-control \
    package/app-layer-pixel2/bin/plumos-volume-control \
    package/app-layer-pixel2/bin/plumos-network-control \
    package/app-layer-pixel2/bin/plumos-network-services \
    package/app-layer-pixel2/bin/plumos-time-sync \
    package/app-layer-pixel2/bin/plumos-storage-health \
    package/app-layer-pixel2/bin/plumos-factory-reset \
    package/app-layer-pixel2/bin/plumos-thumbnail-scraper; do
    bash -n "$ROOT_DIR/$script"
done
test -x "$ROOT_DIR/tests/test-retroarch-config-merge.sh"
test -x "$ROOT_DIR/tests/test-pixel2-power-menu-sleep.sh"
sh -n "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
sh -n "$ROOT_DIR/package/picoarch-pixel2/plumos/bin/plumos-picoarch-launch"
sh -n "$ROOT_DIR/package/picoarch-pixel2/plumos/bin/plumos-picoarch-stop"
sh -n "$ROOT_DIR/scripts/pixel2-device-launch-smoke.sh"
grep -q 'PROFILE_SAMPLE_NAMES' \
    "$ROOT_DIR/scripts/smoke-test-pixel2-romset.py"
grep -q '"retroarch:mba_mini": "varthj.zip"' \
    "$ROOT_DIR/scripts/smoke-test-pixel2-romset.py"
grep -q '"retroarch:frodo": "inbread.d64"' \
    "$ROOT_DIR/scripts/smoke-test-pixel2-romset.py"
grep -q '"retroarch:neocd": "Fatal Fury WAV.cue"' \
    "$ROOT_DIR/scripts/smoke-test-pixel2-romset.py"
grep -q 'SYSTEM_PROFILE_SAMPLE_NAMES' \
    "$ROOT_DIR/scripts/smoke-test-pixel2-romset.py"
grep -q 'PARENT_TREE_SYSTEMS.*"easyrpg".*"scummvm".*"cannonball".*"cavestory".*"dinothawr"' \
    "$ROOT_DIR/scripts/smoke-test-pixel2-romset.py"
grep -q '"cannonball": "cannonball.game"' \
    "$ROOT_DIR/scripts/smoke-test-pixel2-romset.py"
grep -q '"scummvm": "sky.scummvm"' \
    "$ROOT_DIR/scripts/smoke-test-pixel2-romset.py"
grep -q 'retroarch-launch.log' \
    "$ROOT_DIR/scripts/pixel2-device-launch-smoke.sh"
grep -q 'root/lib/libretro:\$root/emulator/lib:\$root/frontend/lib:\$root/lib' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'f"_etc/{name}"' \
    "$ROOT_DIR/scripts/validate-romset-routes.py"
grep -q '"lynx": \["ATARI/Lynx"\]' \
    "$ROOT_DIR/scripts/validate-romset-routes.py"
grep -q 'SONAME_MAP=' \
    "$ROOT_DIR/package/picoarch-pixel2/plumos/bin/plumos-picoarch-launch"
grep -q 'RUNTIME_LIB_DIR/libSDL2-2.0.so.0' \
    "$ROOT_DIR/package/picoarch-pixel2/plumos/bin/plumos-picoarch-launch"
grep -q 'libSDL2-2\.0\.so\.0' "$ROOT_DIR/scripts/build-picoarch-pixel2.sh"
grep -q 'physical=%ux%u logical=%ux%u ccw' \
    "$ROOT_DIR/docker/pixel2-tools/picoarch/picoarch_pixel2_fbdev.h"
PYTHONPYCACHEPREFIX="${TMPDIR:-/tmp}/plumos-pixel2-test-pycache" \
    python3 -m py_compile \
        "$ROOT_DIR/scripts/generate-pixel2-system-logos.py" \
        "$ROOT_DIR/scripts/prepare-pixel2-bios.py" \
        "$ROOT_DIR/scripts/validate-romset-routes.py" \
        "$ROOT_DIR/scripts/smoke-test-pixel2-romset.py"
grep -q 'retroarch:quicknes' "$ROOT_DIR/package/frontend-pixel2/systems.json"
grep -q 'retroarch:gambatte' "$ROOT_DIR/package/frontend-pixel2/systems.json"
grep -q 'retroarch:pcsx_rearmed' "$ROOT_DIR/package/frontend-pixel2/systems.json"
grep -q 'picoarch:quicknes' "$ROOT_DIR/package/frontend-pixel2/systems.json"
python3 - "$ROOT_DIR" <<'PY'
import csv
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
theme = json.loads(
    (root / "vendor/plumos-frontend/seed/themes/default/theme.json").read_text()
)
assert theme["id"] == "default"
assert theme["layout_preset"] == "grid_preview"
assert theme["graphic_mode"]["top_layout"] == "tile_grid"
assert theme["graphic_mode"]["transition_axis"] == "vertical"
systems = json.loads((root / "package/frontend-pixel2/systems.json").read_text())["systems"]
saturn = next(item for item in systems if item["id"] == "saturn")
assert saturn["enabled"] is False
assert saturn["launch_profiles"] == []
assert saturn["default_launch_profile"] == ""
assert saturn["scraper"]["reason"] == "unsupported_performance_rk3326"
recipes = (root / "docker/pixel2-tools/libretro-core-recipes.tsv").read_text().splitlines()
ids = {line.split("|", 1)[0] for line in recipes if line and not line.startswith("#")}
assert "beetle_saturn" not in ids
assert "yabasanshiro" not in ids
assert "mupen64plus_next" not in ids
n64 = next(item for item in systems if item["id"] == "n64")
assert n64["launch_profiles"] == ["retroarch:parallel_n64"]
standalone_build = (root / "scripts/build-standalone-pixel2.sh").read_text()
standalone_launch = (root / "package/standalone-pixel2/plumos/bin/plumos-standalone-launch").read_text()
assert '"id": "mupen64plus"' not in standalone_build
assert "mupen64plus)" not in standalone_launch
PY
grep -q 'strcmp(core_id, "easyrpg") == 0 && directory_exists(plan->rom_path)' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_text_ui.c"
grep -q '"RPG_RT.ldb"' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_text_ui.c"
grep -q 'internal:system-settings' \
    "$ROOT_DIR/package/frontend-pixel2/menus.json"
grep -q 'internal:network-settings' \
    "$ROOT_DIR/package/frontend-pixel2/menus.json"
grep -q 'menu:apps' \
    "$ROOT_DIR/package/frontend-pixel2/menus.json"
grep -q '"id": "pyxel_setup"' "$ROOT_DIR/package/frontend-pixel2/apps.json"
grep -q '"id": "scraping"' "$ROOT_DIR/package/frontend-pixel2/apps.json"
grep -q 'video_rotation = "3"' \
    "$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg"
python3 - "$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg" <<'PY'
from pathlib import Path
import hashlib
import sys

path = Path(sys.argv[1])
data = path.read_bytes()
assert hashlib.sha256(data).hexdigest() == "231ee2585779c098d9512a64cc8b17322c3b86e07d3e84889aaac815893d7280"
lines = data.decode().splitlines()
pairs = [line.split(" = ", 1) for line in lines if " = " in line]
values = dict(pairs)
assert len(pairs) == 3376
assert len(values) == 3376
required = {
    "auto_overrides_enable": "\"true\"",
    "auto_remaps_enable": "\"true\"",
    "config_save_on_exit": "\"true\"",
    "content_video_history_path": "\"/roms/content_video_history.lpl\"",
    "input_driver": "\"udev\"",
    "input_joypad_driver": "\"udev\"",
    "input_menu_toggle_btn": "\"14\"",
    "input_menu_toggle_gamepad_combo": "\"0\"",
    "menu_show_core_updater": "\"false\"",
    "menu_show_online_updater": "\"false\"",
    "input_enable_hotkey_btn": "\"8\"",
    "input_exit_emulator_btn": "\"9\"",
    "input_load_state_btn": "\"4\"",
    "input_save_state_btn": "\"5\"",
    "input_toggle_fast_forward_axis": "\"nul\"",
    "input_toggle_fast_forward_btn": "\"7\"",
    "input_toggle_slowmotion_axis": "\"nul\"",
    "input_toggle_slowmotion_btn": "\"6\"",
    "joypad_autoconfig_dir": "\"/mnt/plumos/factory-defaults/retroarch/autoconfig\"",
    "fps_show": "\"false\"",
    "menu_show_dump_disc": "\"true\"",
    "menu_show_load_disc": "\"true\"",
    "quick_menu_show_save_load_state": "\"true\"",
    "savefile_directory": "\"/mnt/plumos/saves/gb\"",
    "savefiles_in_content_dir": "\"true\"",
    "savestate_auto_index": "\"true\"",
    "savestate_auto_save": "\"true\"",
    "savestate_directory": "\"/mnt/plumos/states/gb\"",
    "savestate_max_keep": "\"20\"",
    "savestate_thumbnail_enable": "\"true\"",
    "savestates_in_content_dir": "\"true\"",
    "settings_show_saving": "\"true\"",
    "screenshot_directory": "\"/mnt/plumos-user/Images\"",
    "sort_savefiles_by_content_enable": "\"true\"",
    "sort_savefiles_enable": "\"true\"",
    "sort_savestates_by_content_enable": "\"true\"",
    "sort_savestates_enable": "\"true\"",
    "system_directory": "\"/mnt/plumos-user/bios\"",
    "video_driver": "\"drm\"",
    "video_font_enable": "\"true\"",
    "video_font_path": "\"/mnt/plumos/fonts/default.otf\"",
    "video_font_size": "\"26.000000\"",
    "video_message_color": "\"0\"",
    "video_refresh_rate": "\"60.000000\"",
    "video_rotation": "\"3\"",
    "vrr_runloop_enable": "\"false\"",
}
for key, expected in required.items():
    assert values[key] == expected, (key, values[key], expected)
text = path.read_text()
for forbidden in ("mali_fbdev", "~/", "/root/", "V90S", "v90s", "Miyoo", "miyoo"):
    assert forbidden not in text, forbidden
PY
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
grep -q 'legacy_pixel2_sha256=b97c897b' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'legacy_incomplete_full_sha256=9f4aaebd' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'regressed_pixel2_sha256=8d9a8e71' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'osd_disabled_factory_sha256=2db551be' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'main_result=migrated-legacy' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'main_result=migrated-regression' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'main_result=migrated-osd' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'main_result=replaced-legacy' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'retroarch_aux=result-%s target=core-options' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'retroarch_aux=result-%s target=parallel-n64-remap' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q '^reicast_cpu_mode = "dynamic_recompiler"$' \
    "$ROOT_DIR/package/retroarch-pixel2/retroarch-core-options.cfg"
grep -q '^parallel-n64-gfxplugin = "gliden64"$' \
    "$ROOT_DIR/package/retroarch-pixel2/retroarch-core-options.cfg"
grep -q '^input_player1_btn_up = "19"$' \
    "$ROOT_DIR/package/retroarch-pixel2/remaps/ParaLLEl N64/ParaLLEl N64.rmp"
grep -q 'retroarch-core-options.cfg' "$ROOT_DIR/scripts/build-retroarch.sh"
grep -q 'remaps/ParaLLEl N64/ParaLLEl N64.rmp' "$ROOT_DIR/scripts/build-retroarch.sh"
if grep -q 'config_save_on_exit = "false"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-menu-launch"; then
    exit 1
fi
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
grep -q '^PLUMOS_INPUT_FUNCTION_CODE=704$' \
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
grep -q 'wonderswan|wonderswancolor)' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'video_allow_rotate=false' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'PLUMOS_DRM_PANEL_ROTATION=3' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'video_rotation=0' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'aspect_ratio_index=22' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'video_allow_rotate = "%s"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'aspect_ratio_index = "%s"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'PLUMOS_DRM_PANEL_ROTATION' \
    "$ROOT_DIR/patches/retroarch/015-pixel2-drm-panel-rotation.patch"
grep -q 'Fixed panel rotation remains' \
    "$ROOT_DIR/patches/retroarch/015-pixel2-drm-panel-rotation.patch"
grep -q 'core_requested_rotation = 0' \
    "$ROOT_DIR/patches/retroarch/015-pixel2-drm-panel-rotation.patch"
grep -q 'video_driver_set_aspect_ratio();' \
    "$ROOT_DIR/patches/retroarch/015-pixel2-drm-panel-rotation.patch"
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
grep -q 'core_options_path = "%s"' \
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
grep -q 'rk805 pwrkey' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q 'plumos-power-menu-overlay' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_pixel2_hardware_keys.c"
grep -q 'POWER_MENU_DEBOUNCE_MS' \
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
grep -q 'printf.*sleep_backend.*power_state' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
grep -q 'sleep=adb-restart-ok' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
grep -q 'run_rk817_resume rearm' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
grep -q 'pause_display_owners' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-power-menu-overlay"
grep -q 'PLUMOS_POWER_MENU_SELECTION' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-power-menu-overlay"
! grep -q 'plumos-hardware-keys-service.*stop' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-frontend-stop"
grep -q 'docker/pixel2-tools/libretro-core-recipes.tsv' "$ROOT_DIR/scripts/build-libretro-cores.sh"
! grep -q '^mupen64plus_next|' \
    "$ROOT_DIR/docker/pixel2-tools/libretro-core-recipes.tsv"
grep -q 'quicknes|A|https://github.com/libretro/QuickNES_Core.git|058d6651' \
    "$ROOT_DIR/docker/pixel2-tools/libretro-core-recipes.tsv"
grep -q 'pcsx_rearmed|A|https://github.com/libretro/pcsx_rearmed.git|d26eaee5' \
    "$ROOT_DIR/docker/pixel2-tools/libretro-core-recipes.tsv"
grep -q 'PCSX_REF=.*9f8b6f248e073f03c530efda7c4cc60a7e2ecafc' \
    "$ROOT_DIR/scripts/build-standalone-pixel2.sh"
grep -q 'PCSX_SDL12_REF=.*fc2ec0c128197f1f5050e48359bc41e618f3abfb' \
    "$ROOT_DIR/scripts/build-standalone-pixel2.sh"
grep -q 'renderer.*builtin-neon-threaded-pixel2-fbdev-ccw' \
    "$ROOT_DIR/scripts/build-standalone-pixel2.sh"
grep -q 'SDL_VIDEODRIVER=dummy' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
grep -q 'PLUMOS_PCSX_REQUIRE_ALSA=1' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
grep -q 'emulator/lib/libpthread.so.0' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
grep -q 'PCSX-ReARMed SDL2 pthread compatibility runtime missing' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
grep -q 'mkdir -p /dev/shm' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
grep -q 'bin/runner' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
grep -q 'PLUMOS_PIXEL2_PYTHON_EXTRA_LIBRARY_PATH' \
    "$ROOT_DIR/package/pyxel-pixel2/plumos/bin/plumos-pyxel-pixel2-launch"
grep -q '^Gpu3 = builtin_gpu$' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/factory-defaults/standalone/pcsx_rearmed/pcsx.cfg"
grep -q 'logical=640x480 ccw' \
    "$ROOT_DIR/package/standalone-pixel2/src/pcsx-pixel2-fbdev.h"
if grep -ERiq 'V9[0]S|M[F]|Miy[o]o|ROCKNIX' \
    "$ROOT_DIR/patches/pcsx_rearmed" \
    "$ROOT_DIR/package/standalone-pixel2/src/pcsx-pixel2-fbdev.h"; then
    printf 'error: foreign identity in Pixel2 PCSX-ReARMed implementation\n' >&2
    exit 1
fi
grep -q 'component: "libretro-cores"' "$ROOT_DIR/scripts/build-libretro-cores.sh"
grep -q 'updater-incompatible app-layer symlink' "$ROOT_DIR/scripts/verify-app-layer.sh"
! grep -q 'ln -s ../network/bin/busybox' \
    "$ROOT_DIR/scripts/build-network-services-pixel2.sh"
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
for service in ssh ftp sftp samba adb; do
    grep -q "network_${service}_enabled" \
        "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
    grep -Eq "(^|[[:space:]|])${service}([[:space:]|)]|$)" \
        "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services"
done
! grep -q 'ships SSH and ADB only' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'default-on-no-explicit-setting' \
    "$ROOT_DIR/rootfs/pixel2/usr/lib/plumos/init.d/10-adbd"
grep -q 'generate-pixel2-system-logos.py' \
    "$ROOT_DIR/scripts/build-frontend-component.sh"
grep -Fq 'apps/*/bin/*.bin|ssh/libexec/*.bin' \
    "$ROOT_DIR/scripts/verify-app-layer.sh"
! grep -q 'head -n 1' "$ROOT_DIR/scripts/verify-app-layer.sh"
grep -q 'xargs -0 grep -I' "$ROOT_DIR/scripts/verify-app-layer.sh"
jq -e '.systems[] | select(.id == "ports" and .enabled == true and .default_launch_profile == "external:port")' \
    "$ROOT_DIR/package/frontend-pixel2/systems.json" >/dev/null
grep -q 'install_scraper_runtime' "$ROOT_DIR/scripts/build-frontend-component.sh"
grep -q 'PLUMOS_SDCARD_ROOT:-/mnt/plumos-user' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-thumbnail-scraper"
grep -q 'PLUMOS_BUSYBOX:-/bin/busybox' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-thumbnail-scraper"
grep -q 'scraper-sources.tsv' "$ROOT_DIR/scripts/build-frontend-component.sh"

feature_tmp="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-feature-test.XXXXXX")"
trap 'rm -rf "$feature_tmp"' EXIT
mkdir -p "$feature_tmp/card/roms/nes" "$feature_tmp/plumos"
python3 - "$ROOT_DIR" "$feature_tmp" <<'PY'
import importlib.util
import sys
from pathlib import Path

repo = Path(sys.argv[1])
temp = Path(sys.argv[2]) / "portmaster-stage"
module_path = repo / "package/portmaster-pixel2/plumos/apps/portmaster/adapter/plumos_portmaster_update.py"
spec = importlib.util.spec_from_file_location("plumos_portmaster_update", module_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
(temp / "PortMaster/batocera").mkdir(parents=True)
(temp / "PortMaster/mod_ROCKNIX.txt").write_text("foreign adapter\n")
(temp / "PortMaster/funcs.txt").write_text("shared runtime\n")
module.prune_foreign_adapters(temp)
assert not (temp / "PortMaster/batocera").exists()
assert not (temp / "PortMaster/mod_ROCKNIX.txt").exists()
assert (temp / "PortMaster/funcs.txt").is_file()
PY

mkdir -p "$feature_tmp/network/plumos/bin" "$feature_tmp/network/card"
cat >"$feature_tmp/network/adbd-control" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >>"$PLUMOS_TEST_ADBD_CALLS"
case "${PLUMOS_TEST_ADBD_STATE:-stopped}" in
    running) printf '%s\n' 'state=running' ;;
    *) printf '%s\n' 'state=stopped' 'summary=ADB applies at reboot' ;;
esac
EOF
chmod 0755 "$feature_tmp/network/adbd-control"
network_env=(
    PLUMOS_ROOT="$feature_tmp/network/plumos"
    PLUMOS_SDCARD_ROOT="$feature_tmp/network/card"
    PLUMOS_RUNTIME_ROOT="$feature_tmp/network/run"
    PLUMOS_ADBD_CONTROL="$feature_tmp/network/adbd-control"
    PLUMOS_TEST_ADBD_CALLS="$feature_tmp/network/adbd-calls"
)
env "${network_env[@]}" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services" \
    status adb >"$feature_tmp/network/adb-default.status" || true
grep -q '^enabled=1$' "$feature_tmp/network/adb-default.status"
PLUMOS_TEST_ADBD_STATE=running env "${network_env[@]}" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services" \
    status adb >"$feature_tmp/network/adb-running.status"
grep -q '^state=running$' "$feature_tmp/network/adb-running.status"
grep -q '^summary=ADB connected over USB$' \
    "$feature_tmp/network/adb-running.status"
grep -q 'waiting for authorized_keys' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services"
grep -q "awk '{ print \$3 }'" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services"
grep -q 'PLUMOS_ROOT/emulator/lib' \
    "$ROOT_DIR/scripts/build-nextcommander-pixel2.sh"
grep -q 'CXXFLAGS += -DCMDR_KEY_OPEN=1' \
    "$ROOT_DIR/docker/pixel2-tools/patches/nextcommander-pixel2.patch"
grep -q 'CXXFLAGS += -DCMDR_KEY_PARENT=0' \
    "$ROOT_DIR/docker/pixel2-tools/patches/nextcommander-pixel2.patch"
grep -q 'CXXFLAGS += -DCMDR_KEY_SELECT=8' \
    "$ROOT_DIR/docker/pixel2-tools/patches/nextcommander-pixel2.patch"
grep -q 'CXXFLAGS += -DCMDR_KEY_TRANSFER=9' \
    "$ROOT_DIR/docker/pixel2-tools/patches/nextcommander-pixel2.patch"
grep -q 'CXXFLAGS += -DCMDR_KEY_MENU=14' \
    "$ROOT_DIR/docker/pixel2-tools/patches/nextcommander-pixel2.patch"
grep -q 'Pixel2 virtual D-pad is SDL joystick buttons 10-13' \
    "$ROOT_DIR/docker/pixel2-tools/patches/nextcommander-pixel2.patch"
grep -q '"/bin/busybox", AsConstCStr(args)' \
    "$ROOT_DIR/docker/pixel2-tools/patches/nextcommander-pixel2.patch"
grep -q 'i == inputs.size() - 1' \
    "$ROOT_DIR/docker/pixel2-tools/patches/nextcommander-pixel2.patch"
grep -q '^path_default_right=/roms$' \
    "$ROOT_DIR/scripts/build-nextcommander-pixel2.sh"
grep -q 'lib/libretro/libvorbisfile.so.3' \
    "$ROOT_DIR/package/portmaster-pixel2/plumos/bin/plumos-portmaster-runtime"
grep -q 'lib/libretro/libopusfile.so.0' \
    "$ROOT_DIR/package/portmaster-pixel2/plumos/bin/plumos-portmaster-runtime"
grep -q 'retroarch" --menu -v' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-menu-launch"
grep -q 'plumos-ensure-udev-input-db' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-menu-launch"
grep -q '/proc/\$pid/stat' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-frontend-stop"
grep -q '40-frontend' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-frontend-launch"
grep -q 'process_is_live' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-hardware-keys-service"
grep -q 'plumos-frontend-stop" stop' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
"$ROOT_DIR/tests/test-pixel2-power-menu-sleep.sh"
grep -q 'renderer->var.xres = renderer->physical_yres' \
    "$ROOT_DIR/docker/pixel2-tools/patches/music-player-pixel2.patch"
grep -q 'case BTN_EAST:' \
    "$ROOT_DIR/docker/pixel2-tools/patches/music-player-pixel2.patch"
grep -q 'case BTN_SOUTH:' \
    "$ROOT_DIR/docker/pixel2-tools/patches/music-player-pixel2.patch"
grep -q 'case BTN_TRIGGER_HAPPY1:' \
    "$ROOT_DIR/docker/pixel2-tools/patches/music-player-pixel2.patch"
grep -q 'B/FUNCTION Exit' \
    "$ROOT_DIR/docker/pixel2-tools/patches/music-player-pixel2.patch"
grep -q 'logical_width = (r->rotation == 1 || r->rotation == 3)' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h"
grep -q '? (int)r->physical_yres' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h"
env "${network_env[@]}" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services" \
    start adb >"$feature_tmp/network/adb-start.status" || true
grep -q '^adb_enabled=1$' \
    "$feature_tmp/network/plumos/config/network/services.conf"
! grep -q '^start$' "$feature_tmp/network/adbd-calls"
env "${network_env[@]}" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services" \
    stop adb >"$feature_tmp/network/adb-stop.status" || true
grep -q '^adb_enabled=0$' \
    "$feature_tmp/network/plumos/config/network/services.conf"
touch "$feature_tmp/network/card/plumos-enable-adb"
env "${network_env[@]}" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services" \
    status adb >"$feature_tmp/network/adb-recovery.status" || true
grep -q '^enabled=1$' "$feature_tmp/network/adb-recovery.status"

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
