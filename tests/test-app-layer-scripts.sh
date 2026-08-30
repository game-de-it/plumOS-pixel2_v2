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
    package/app-layer-pixel2/bin/plumos-np2kai-config-repair \
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
    package/app-layer-pixel2/bin/plumos-udhcpc-script \
    package/app-layer-pixel2/bin/plumos-wifi-recovery \
    package/app-layer-pixel2/bin/plumos-wifi-uevent \
    package/app-layer-pixel2/bin/plumos-network-services \
    package/app-layer-pixel2/bin/plumos-time-sync \
    package/app-layer-pixel2/bin/plumos-storage-health \
    package/app-layer-pixel2/bin/plumos-factory-reset \
    package/app-layer-pixel2/bin/plumos-thumbnail-scraper; do
    bash -n "$ROOT_DIR/$script"
done
test -x "$ROOT_DIR/tests/test-retroarch-config-merge.sh"
test -x "$ROOT_DIR/tests/test-np2kai-config-repair.sh"
"$ROOT_DIR/tests/test-np2kai-config-repair.sh"
test -x "$ROOT_DIR/tests/test-pixel2-retroarch-game-menu-selection.sh"
"$ROOT_DIR/tests/test-pixel2-retroarch-game-menu-selection.sh"
test -x "$ROOT_DIR/tests/test-pixel2-power-menu-sleep.sh"
test -x "$ROOT_DIR/tests/test-pixel2-volume-control.sh"
"$ROOT_DIR/tests/test-pixel2-volume-control.sh"
test -x "$ROOT_DIR/tests/test-pixel2-network-control.sh"
"$ROOT_DIR/tests/test-pixel2-network-control.sh"
test -x "$ROOT_DIR/tests/test-pixel2-wifi-recovery.sh"
"$ROOT_DIR/tests/test-pixel2-wifi-recovery.sh"
sh -n "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
sh -n "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-stop"
sh -n "$ROOT_DIR/package/standalone-pixel2/plumos/standalone/pico8/bin/wget"
sh -n "$ROOT_DIR/package/picoarch-pixel2/plumos/bin/plumos-picoarch-launch"
sh -n "$ROOT_DIR/package/picoarch-pixel2/plumos/bin/plumos-picoarch-stop"
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
grep -q 'picoarch-pixel2-host-input-poll\.patch' \
    "$ROOT_DIR/scripts/build-picoarch-pixel2.sh"
grep -q 'physical=%ux%u logical=%ux%u ccw' \
    "$ROOT_DIR/docker/pixel2-tools/picoarch/picoarch_pixel2_fbdev.h"
grep -q 'FBIOBLANK, FB_BLANK_UNBLANK' \
    "$ROOT_DIR/docker/pixel2-tools/picoarch/picoarch_pixel2_fbdev.h"
grep -q 'FBIOPAN_DISPLAY' \
    "$ROOT_DIR/docker/pixel2-tools/picoarch/picoarch_pixel2_fbdev.h"
grep -q 'sigaction(SIGCONT' \
    "$ROOT_DIR/docker/pixel2-tools/picoarch/picoarch_pixel2_fbdev.h"
grep -q 'getenv("PLUMOS_PICOARCH_RGB565_BYTESWAP")' \
    "$ROOT_DIR/docker/pixel2-tools/picoarch/picoarch-pixel2-pixel-format.patch"
grep -q '(pixel << 8) | (pixel >> 8)' \
    "$ROOT_DIR/docker/pixel2-tools/picoarch/picoarch-pixel2-pixel-format.patch"
grep -q 'gambatte|gambatte_libretro.so) rgb565_byteswap_default=1' \
    "$ROOT_DIR/package/picoarch-pixel2/plumos/bin/plumos-picoarch-launch"
grep -q 'rgb565_byteswap=%s' \
    "$ROOT_DIR/package/picoarch-pixel2/plumos/bin/plumos-picoarch-launch"
PYTHONPYCACHEPREFIX="${TMPDIR:-/tmp}/plumos-pixel2-test-pycache" \
    python3 -m py_compile \
        "$ROOT_DIR/scripts/generate-pixel2-system-logos.py" \
        "$ROOT_DIR/scripts/prepare-pixel2-bios.py" \
        "$ROOT_DIR/scripts/validate-romset-routes.py"
grep -q 'retroarch:quicknes' "$ROOT_DIR/package/frontend-pixel2/systems.json"
grep -q 'retroarch:gambatte' "$ROOT_DIR/package/frontend-pixel2/systems.json"
grep -q 'retroarch:mgba_modern' "$ROOT_DIR/package/frontend-pixel2/systems.json"
grep -q 'retroarch:pcsx_rearmed' "$ROOT_DIR/package/frontend-pixel2/systems.json"
grep -q 'picoarch:quicknes' "$ROOT_DIR/package/frontend-pixel2/systems.json"
jq -e '.systems[] | select(.id == "gameandwatch") |
       .extensions == ["mgw", "zip"]' \
    "$ROOT_DIR/package/frontend-pixel2/systems.json" >/dev/null
jq -e '.systems[] | select(.id == "pcfx") |
       .extensions == ["cue", "ccd", "toc", "chd"]' \
    "$ROOT_DIR/package/frontend-pixel2/systems.json" >/dev/null
grep -q 'retroarch_archive=result-extracted system=gameandwatch format=mgw' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'mgba_modern_libretro.so' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'state_root="$state_root/mgba-modern"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'savestates_in_content_dir = "false"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'strcmp(core_id, "lutro") == 0 && directory_exists(plan->rom_path)' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_text_ui.c"
grep -q 'first_file_with_extension(plan->rom_path, "svm"' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_text_ui.c"
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
assert "mgba" in ids
assert "mgba_modern" in ids
for system_id in ("gb", "gbc", "gba"):
    system = next(item for item in systems if item["id"] == system_id)
    assert "retroarch:mgba" in system["launch_profiles"]
    assert "retroarch:mgba_modern" in system["launch_profiles"]
n64 = next(item for item in systems if item["id"] == "n64")
assert n64["launch_profiles"] == [
    "retroarch:parallel_n64",
    "standalone:mupen64plus",
]
assert n64["default_launch_profile"] == "retroarch:parallel_n64"
pico8 = next(item for item in systems if item["id"] == "pico8")
assert pico8["launch_profiles"][0] == "standalone:pico8"
assert pico8["default_launch_profile"] == "standalone:pico8"
assert "pico8" in {entry["name"] for entry in pico8["directory_aliases"]}
standalone_build = (root / "scripts/build-standalone-pixel2.sh").read_text()
standalone_launch = (root / "package/standalone-pixel2/plumos/bin/plumos-standalone-launch").read_text()
standalone_mupen_sram_patch = (
    root / "patches/mupen64plus/mupen64plus-core-2.6.0-sram-save-range.patch"
).read_text()
standalone_mupen_input_patch = (
    root / "patches/mupen64plus/mupen64plus-input-sdl-2.6.0-pixel2-dpad-mode.patch"
).read_text()
assert '"id": "mupen64plus"' in standalone_build
assert "mupen64plus)" in standalone_launch
assert "MUPEN64PLUS_SRAM_PATCH" in standalone_build
assert "save_start = cart_addr & ~(size_t)3" in standalone_mupen_sram_patch
assert "save_end - save_start" in standalone_mupen_sram_patch
assert "pixel2_dpad_mode" in standalone_mupen_input_patch
assert "pixel2_dpad_mode_requested" in standalone_mupen_input_patch
assert 'mupen64plus-dpad-mode.%ld' in standalone_mupen_input_patch
assert "access(pixel2_dpad_mode_path, F_OK)" in standalone_mupen_input_patch
assert "controller[Control].joystick, 14" not in standalone_mupen_input_patch
assert 'MUPEN64PLUS_INPUT_PATCH' in standalone_build
assert "DPad R =\n" in standalone_build
assert "X Axis = button(12,13)" in standalone_build
assert "Y Axis = button(10,11)" in standalone_build
assert "PLUMOS_MUPEN64PLUS_GL_ROTATION=270" in standalone_launch
assert "mupen64plus-video-rice.so" in standalone_launch
assert 'mupen_data_dir="${XDG_CACHE_HOME}/data.$$"' in standalone_launch
assert '--datadir "$mupen_data_dir"' in standalone_launch
assert '--datadir "${workdir}/share/mupen64plus"' not in standalone_launch
assert 'rm -rf "$mupen_data_dir"' in standalone_launch
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
python3 - "$ROOT_DIR/package/frontend-pixel2/menus.json" \
    "$ROOT_DIR/package/frontend-pixel2/feature-contract.json" <<'PY'
import json
import sys
from pathlib import Path

menus = json.loads(Path(sys.argv[1]).read_text())
contract = json.loads(Path(sys.argv[2]).read_text())
start = next(menu for menu in menus["menus"] if menu["id"] == "start")
entries = start["entries"]
ids = [entry["id"] for entry in entries]
actions = [entry["action"] for entry in entries]
assert ids == [
    "ui-settings", "system-settings", "network-settings", "apps", "help", "power"
]
assert actions[-1] == "system:power"
assert "reboot" not in ids and "shutdown" not in ids
assert "system:reboot" not in actions and "system:shutdown" not in actions
assert contract["start_menu_ids"] == ids
PY
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
assert hashlib.sha256(data).hexdigest() == "c88ea45f9d341d31dfacc272d1b2cb930702c399b8c992d5953784d7aac44ac8"
lines = data.decode().splitlines()
pairs = [line.split(" = ", 1) for line in lines if " = " in line]
values = dict(pairs)
assert len(pairs) == 3376
assert len(values) == 3376
required = {
    "assets_directory": "\"/mnt/plumos/retroarch/assets\"",
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
    "video_message_color": "\"ffff00\"",
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
grep -q 'black_osd_factory_sha256=231ee258' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'main_result=migrated-legacy' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'main_result=migrated-regression' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'main_result=migrated-osd' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-config-merge"
grep -q 'main_result=replaced-black-osd' \
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
grep -q 'flycast_xtreme_libretro.so|' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'video_driver=gl' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'video_rotation=1' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'aspect_ratio_index=24' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'dreamcast)' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'PLUMOS_GL_MENU_ROTATION=content' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'parallel_n64_libretro.so)' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'km_duckswanstation_xtreme_amped_libretro.so|' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'n64|psx)' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -q 'rotate_menu ? &gl->mvp : &gl->mvp_no_rot' \
    "$ROOT_DIR/patches/retroarch/017-pixel2-gl-menu-rotation.patch"
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
grep -q '^aspect_ratio_index=$' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
grep -F -q '[ -z "$aspect_ratio_index" ] ||' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"
if grep -q "printf 'video_aspect_ratio" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-launch"; then
    exit 1
fi
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
grep -q 'video_aspect_ratio_idx == ASPECT_RATIO_CORE' \
    "$ROOT_DIR/patches/retroarch/022-pixel2-drm-core-aspect-rotation.patch"
grep -q 'retroarch_get_core_requested_rotation' \
    "$ROOT_DIR/patches/retroarch/022-pixel2-drm-core-aspect-rotation.patch"
grep -q 'new_aspect = 1.0f / new_aspect' \
    "$ROOT_DIR/patches/retroarch/022-pixel2-drm-core-aspect-rotation.patch"
grep -q 'Core aspect corrected for Pixel2 rotation' \
    "$ROOT_DIR/patches/retroarch/022-pixel2-drm-core-aspect-rotation.patch"
grep -q 'surface->aspect, keep_aspect' \
    "$ROOT_DIR/patches/retroarch/023-pixel2-drm-surface-aspect.patch"
grep -q 'video_viewport_get_scaled_aspect2' \
    "$ROOT_DIR/patches/retroarch/023-pixel2-drm-surface-aspect.patch"
grep -q 'Surfaces invalidated for runtime aspect change' \
    "$ROOT_DIR/patches/retroarch/026-pixel2-drm-dynamic-aspect-recreate.patch"
grep -q 'drm_surface_free(_drmvars, &_drmvars->main_surface)' \
    "$ROOT_DIR/patches/retroarch/026-pixel2-drm-dynamic-aspect-recreate.patch"
grep -q 'drm_surface_free(_drmvars, &_drmvars->menu_surface)' \
    "$ROOT_DIR/patches/retroarch/026-pixel2-drm-dynamic-aspect-recreate.patch"
grep -q '_drmvars->core_width  = 0' \
    "$ROOT_DIR/patches/retroarch/026-pixel2-drm-dynamic-aspect-recreate.patch"
if grep -q '_drmvars->menu_active = false' \
    "$ROOT_DIR/patches/retroarch/026-pixel2-drm-dynamic-aspect-recreate.patch"; then
    exit 1
fi
grep -q '^-static void drm_surface_set_aspect' \
    "$ROOT_DIR/patches/retroarch/026-pixel2-drm-dynamic-aspect-recreate.patch"
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
grep -A2 'pcm.!default' \
    "$ROOT_DIR/package/audio-router-pixel2/plumos/bin/plumos-audio-output" |
    grep -q 'type plumos_hotplug'
! grep -A3 'pcm.!default' \
    "$ROOT_DIR/package/audio-router-pixel2/plumos/bin/plumos-audio-output" |
    grep -q 'slave.pcm'
grep -q 'export PLUMOS_AUDIO_POLL_PROXY=0' \
    "$ROOT_DIR/package/portmaster-pixel2/plumos/bin/plumos-portmaster-port-launch"
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
grep -q 'strcmp(entry->action, "system:power") == 0' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'wifi_back_to_network_settings(ui, connected_status)' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'wifi_connect_page && wifi_keyboard_row >= 0' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h"
grep -q 'password_scale = wifi_keyboard_row >= 0 ? 3 : 2' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h"
font_source="$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
sed -n '/static int choose_mali_font_path/,/static int choose_mali_fallback_font_path/p' \
    "$font_source" | grep -q 'ui->plumos_root,'
sed -n '/static int choose_mali_font_path/,/static int choose_mali_fallback_font_path/p' \
    "$font_source" | grep -q '"fonts/default.otf"'
sed -n '/static int choose_mali_fallback_font_path/,/static void add_setting_entry/p' \
    "$font_source" | grep -q 'ui->plumos_root,'
sed -n '/static int choose_mali_fallback_font_path/,/static void add_setting_entry/p' \
    "$font_source" | grep -q '"fonts/cjk-fallback.ttc"'
grep -q 'sysrq-trigger' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
grep -q 'rk817-dev-off' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
grep -q 'i2cset -f -y 0 0x20 0xf4' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
grep -q 'plumos-reboot-mode' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
grep -q 'LINUX_REBOOT_CMD_RESTART2' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_reboot_mode.c"
grep -q 'printf.*sleep_backend.*power_state' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
! grep -qi 'adbd\|functionfs' \
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
grep -q 'pico8_machine.*b700' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
grep -q '_pico8_64' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
grep -q 'PLUMOS_PICO8_SDL_ROTATION=270' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
grep -q 'SDL_RENDER_DRIVER=opengles2' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
grep -q 'PLUMOS_PICO8_PIXEL_PERFECT:-0' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
grep -q 'binary_policy.*external-proprietary' \
    "$ROOT_DIR/scripts/build-standalone-pixel2.sh"
grep -q 'MUPEN64PLUS_UI_REF=.*1a68327fddda71f1acbad8a63ef04288b1887d19' \
    "$ROOT_DIR/scripts/build-standalone-pixel2.sh"
grep -q 'MUPEN64PLUS_CORE_REF=.*b0d68c20f49b8f833afa21450e0e8874c87c13c4' \
    "$ROOT_DIR/scripts/build-standalone-pixel2.sh"
grep -q 'MUPEN64PLUS_VIDEO_REF=.*fcf00779f08a9503ef30d26422f6b0350684820d' \
    "$ROOT_DIR/scripts/build-standalone-pixel2.sh"
grep -q 'pixel2_joypad' \
    "$ROOT_DIR/scripts/build-standalone-pixel2.sh"
grep -q 'BTN_TRIGGER_HAPPY1' \
    "$ROOT_DIR/package/standalone-pixel2/src/plumos-mupen64plus-hotkey.c"
grep -q 'FUNCTION_EXIT_HOLD_MS 1500L' \
    "$ROOT_DIR/package/standalone-pixel2/src/plumos-mupen64plus-hotkey.c"
grep -q 'FUNCTION_RELEASE_SETTLE_MS 200L' \
    "$ROOT_DIR/package/standalone-pixel2/src/plumos-mupen64plus-hotkey.c"
grep -q 'publish_dpad_mode' \
    "$ROOT_DIR/package/standalone-pixel2/src/plumos-mupen64plus-hotkey.c"
grep -q 'mupen64plus-dpad-mode.%ld' \
    "$ROOT_DIR/package/standalone-pixel2/src/plumos-mupen64plus-hotkey.c"
grep -q 'Function held; stopping' \
    "$ROOT_DIR/package/standalone-pixel2/src/plumos-mupen64plus-hotkey.c"
grep -q 'PLUMOS_MUPEN64PLUS_LOGICAL_SIZE=640x480' \
    "$ROOT_DIR/package/standalone-pixel2/plumos/bin/plumos-standalone-launch"
grep -q 'PLUMOS_PIXEL2_PYTHON_EXTRA_LIBRARY_PATH' \
    "$ROOT_DIR/package/pyxel-pixel2/plumos/bin/plumos-pyxel-pixel2-launch"
grep -q 'PLUMOS_PYXEL_GL_ROTATION.*270' \
    "$ROOT_DIR/package/pyxel-pixel2/plumos/bin/plumos-pyxel-pixel2-launch"
grep -q 'PLUMOS_PYXEL_FIT=0' \
    "$ROOT_DIR/package/pyxel-pixel2/plumos/bin/plumos-pyxel-pixel2-launch"
grep -q 'PLUMOS_PYXEL_HIDE_CURSOR.*1' \
    "$ROOT_DIR/package/pyxel-pixel2/plumos/bin/plumos-pyxel-pixel2-launch"
grep -q 'plumos-pyxel-gl-rotate.so' \
    "$ROOT_DIR/scripts/build-pyxel-runtime-pixel2.sh"
grep -q 'PLUMOS_PYXEL_LOGICAL_SIZE.*640x480' \
    "$ROOT_DIR/package/pyxel-pixel2/plumos/bin/plumos-pyxel-pixel2-launch"
grep -q 'plumos_pyxel_pixel2_shim' \
    "$ROOT_DIR/package/pyxel-pixel2/plumos/bin/plumos-pyxel-pixel2-launch"
grep -q 'SDL_GAMECONTROLLERCONFIG' \
    "$ROOT_DIR/package/pyxel-pixel2/plumos/bin/plumos-pyxel-pixel2-launch"
grep -q 'a:b1,b:b0,x:b2,y:b3' \
    "$ROOT_DIR/package/pyxel-pixel2/plumos/bin/plumos-pyxel-pixel2-launch"
grep -q 'PLUMOS_PYXEL_TEMP_ROOT.*CACHE_ROOT/tmp' \
    "$ROOT_DIR/package/pyxel-pixel2/plumos/bin/plumos-pyxel-pixel2-launch"
grep -q 'PLUMOS_GL_LOGICAL_SIZE_ENV' \
    "$ROOT_DIR/package/portmaster-pixel2/src/plumos_portmaster_gl_rotate.c"
grep -q 'PLUMOS_GL_ROTATION_ENV' \
    "$ROOT_DIR/package/portmaster-pixel2/src/plumos_portmaster_gl_rotate.c"
grep -q 'LOAD_NEXT(sdl_gl_get_proc_address, "SDL_GL_GetProcAddress")' \
    "$ROOT_DIR/package/portmaster-pixel2/src/plumos_portmaster_gl_rotate.c"
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
for service in ssh ftp sftp samba; do
    grep -q "network_${service}_enabled" \
        "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
    grep -Eq "(^|[[:space:]|])${service}([[:space:]|)]|$)" \
        "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services"
done
! grep -qi 'adb\|functionfs\|network_usb_mode' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
! grep -qi 'adb\|functionfs\|usb_mode' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services"
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
grep -q -- '--retry-all-errors' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-thumbnail-scraper"
grep -q 'acquire_index_lock' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-thumbnail-scraper"
grep -q 'preflight_lookup_indexes "$fetch_system_id"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-thumbnail-scraper"
"$ROOT_DIR/tests/test-thumbnail-scraper-concurrency.sh"
"$ROOT_DIR/tests/test-library-scan-cache-refresh.sh"

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
network_env=(
    PLUMOS_ROOT="$feature_tmp/network/plumos"
    PLUMOS_SDCARD_ROOT="$feature_tmp/network/card"
    PLUMOS_RUNTIME_ROOT="$feature_tmp/network/run"
)
env "${network_env[@]}" \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services" \
    status ssh >"$feature_tmp/network/ssh-default.status" || true
grep -q '^enabled=1$' "$feature_tmp/network/ssh-default.status"
! grep -q 'waiting for authorized_keys' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services"
grep -q 'INITIAL_PASSWORD="${PLUMOS_SSH_INITIAL_PASSWORD:-plumos}"' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-ssh-password"
grep -q 'auth=password,pubkey' \
    "$ROOT_DIR/package/app-layer-pixel2/ssh/start-ssh.sh"
grep -q 'ip link set lo up' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-network-services"
grep -q 'ip addr add 127.0.0.1/8 dev lo' \
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
"$ROOT_DIR/tests/test-portmaster-pgrep.sh"
"$ROOT_DIR/tests/test-portmaster-df.sh"
"$ROOT_DIR/tests/test-portmaster-pixel2-runtime.sh"
"$ROOT_DIR/tests/test-portmaster-pixel2-moonlight-gui.sh"
"$ROOT_DIR/tests/test-portmaster-pixel2-audit.sh"
"$ROOT_DIR/tests/test-portmaster-pixel2-exec-guard.sh"
"$ROOT_DIR/tests/test-portmaster-pixel2-rockbox.sh"
"$ROOT_DIR/tests/test-portmaster-pixel2-session-cleanup.sh"
grep -q 'retroarch" --menu -v' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-menu-launch"
grep -q 'plumos-ensure-udev-input-db' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-retroarch-menu-launch"
grep -q '/proc/\$pid/stat' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-frontend-stop"
grep -q '40-frontend' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-frontend-launch"
test -x "$ROOT_DIR/tests/test-pixel2-frontend-clean-environment.sh"
"$ROOT_DIR/tests/test-pixel2-frontend-clean-environment.sh"
grep -q 'process_is_live' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-hardware-keys-service"
grep -q 'plumos-frontend-stop" stop' \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-safe-shutdown"
grep -q 'if (is_start_menu_return_screen(ui->screen))' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
grep -q 'screen == SCREEN_TOP || screen == SCREEN_ROMS' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_controller_ui.c"
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
grep -q 'SND_PCM_NONBLOCK' \
    "$ROOT_DIR/docker/pixel2-tools/patches/music-player-pixel2.patch"
grep -q 'rc == -EPIPE || rc == -ESTRPIPE' \
    "$ROOT_DIR/docker/pixel2-tools/patches/music-player-pixel2.patch"
grep -q '/mnt/plumos-user/Music' \
    "$ROOT_DIR/docker/pixel2-tools/patches/music-player-pixel2.patch"
test -x "$ROOT_DIR/scripts/pixel2-sleep-auto-select.sh"
test -x "$ROOT_DIR/scripts/pixel2-sleep-cycle-device.sh"
test -x "$ROOT_DIR/scripts/validate-pixel2-sleep-matrix.py"
python3 -m py_compile "$ROOT_DIR/scripts/validate-pixel2-sleep-matrix.py"
grep -q 'logical_width = (r->rotation == 1 || r->rotation == 3)' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h"
grep -q '? (int)r->physical_yres' \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_fbdev_renderer.h"
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
