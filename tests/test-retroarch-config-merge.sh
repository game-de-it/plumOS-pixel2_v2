#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
docker run --rm --platform linux/arm64 \
    -v "$ROOT_DIR:/work:ro" busybox:latest sh -euc '
root=/tmp/plumos
factory=$root/factory-defaults/retroarch
active=$root/config/retroarch
mkdir -p "$factory/remaps/ParaLLEl N64"
cp /work/package/retroarch-pixel2/retroarch.cfg "$factory/retroarch.cfg"
cp /work/package/retroarch-pixel2/retroarch-core-options.cfg \
    "$factory/retroarch-core-options.cfg"
cp "/work/package/retroarch-pixel2/remaps/ParaLLEl N64/ParaLLEl N64.rmp" \
    "$factory/remaps/ParaLLEl N64/ParaLLEl N64.rmp"

PLUMOS_ROOT=$root PLUMOS_BUSYBOX=/bin/busybox \
    /work/package/app-layer-pixel2/bin/plumos-retroarch-config-merge \
    > /tmp/install.log
grep -q "target=core-options" /tmp/install.log
grep -q "target=parallel-n64-remap" /tmp/install.log
cmp "$factory/retroarch.cfg" "$active/retroarch.cfg"
cmp "$factory/retroarch-core-options.cfg" "$active/retroarch-core-options.cfg"
cmp "$factory/remaps/ParaLLEl N64/ParaLLEl N64.rmp" \
    "$active/remaps/ParaLLEl N64/ParaLLEl N64.rmp"

# Existing user values must survive; only absent factory keys may be appended.
printf "%s\n" "reicast_cpu_mode = \"interpreter\"" \
    > "$active/retroarch-core-options.cfg"
printf "%s\n" "input_player1_btn_up = \"99\"" \
    > "$active/remaps/ParaLLEl N64/ParaLLEl N64.rmp"
PLUMOS_ROOT=$root PLUMOS_BUSYBOX=/bin/busybox \
    /work/package/app-layer-pixel2/bin/plumos-retroarch-config-merge \
    > /tmp/merge.log
grep -q "^reicast_cpu_mode = \"interpreter\"$" \
    "$active/retroarch-core-options.cfg"
grep -q "^parallel-n64-gfxplugin = \"gliden64\"$" \
    "$active/retroarch-core-options.cfg"
grep -q "^input_player1_btn_up = \"99\"$" \
    "$active/remaps/ParaLLEl N64/ParaLLEl N64.rmp"
grep -q "^input_player1_btn_down = \"18\"$" \
    "$active/remaps/ParaLLEl N64/ParaLLEl N64.rmp"

# A previous factory cfg becomes byte-different when RetroArch saves it. Use
# its generation marker to migrate only values that still equal the old
# Pixel2 defaults, while retaining an unrelated explicit user value.
sed -i \
    -e "s/input_toggle_fast_forward_axis = \"nul\"/input_toggle_fast_forward_axis = \"+5\"/" \
    -e "s/input_toggle_fast_forward_btn = \"7\"/input_toggle_fast_forward_btn = \"nul\"/" \
    -e "s/input_toggle_slowmotion_axis = \"nul\"/input_toggle_slowmotion_axis = \"+4\"/" \
    -e "s/input_toggle_slowmotion_btn = \"6\"/input_toggle_slowmotion_btn = \"nul\"/" \
    -e "s/savefiles_in_content_dir = \"true\"/savefiles_in_content_dir = \"false\"/" \
    -e "s/savestates_in_content_dir = \"true\"/savestates_in_content_dir = \"false\"/" \
    -e "s/sort_savefiles_by_content_enable = \"true\"/sort_savefiles_by_content_enable = \"false\"/" \
    -e "s/sort_savefiles_enable = \"true\"/sort_savefiles_enable = \"false\"/" \
    -e "s/sort_savestates_by_content_enable = \"true\"/sort_savestates_by_content_enable = \"false\"/" \
    -e "s/sort_savestates_enable = \"true\"/sort_savestates_enable = \"false\"/" \
    -e "s/input_save_state_btn = \"5\"/input_save_state_btn = \"42\"/" \
    "$active/retroarch.cfg"
printf "%s\n" "9f4aaebdab3cc3a9be24b203161e63f63f1887e4eb0b79c82618086d3cbc4b24" \
    > "$root/state/retroarch/factory-config.sha256"
PLUMOS_ROOT=$root PLUMOS_BUSYBOX=/bin/busybox \
    /work/package/app-layer-pixel2/bin/plumos-retroarch-config-merge \
    > /tmp/migrate.log
grep -q "retroarch_config=result-migrated-legacy added=10" /tmp/migrate.log
grep -q "^input_exit_emulator_btn = \"9\"$" "$active/retroarch.cfg"
grep -q "^input_menu_toggle_gamepad_combo = \"0\"$" "$active/retroarch.cfg"
grep -q "^input_toggle_fast_forward_btn = \"7\"$" "$active/retroarch.cfg"
grep -q "^input_toggle_slowmotion_btn = \"6\"$" "$active/retroarch.cfg"
grep -q "^savefiles_in_content_dir = \"true\"$" "$active/retroarch.cfg"
grep -q "^savestates_in_content_dir = \"true\"$" "$active/retroarch.cfg"
grep -q "^input_save_state_btn = \"42\"$" "$active/retroarch.cfg"

# Repair the short-lived Pixel2 factory that changed START+SELECT from direct
# exit to a menu combo. A cfg saved by RetroArch is identified by its factory
# marker, and only the two matching regression values are restored.
sed -i \
    -e "s/input_exit_emulator_btn = \"9\"/input_exit_emulator_btn = \"nul\"/" \
    -e "s/input_menu_toggle_gamepad_combo = \"0\"/input_menu_toggle_gamepad_combo = \"4\"/" \
    "$active/retroarch.cfg"
printf "%s\n" "8d9a8e71cc38e63d1c6084d6bcf99701507cb632ff2dd1f4ebf9963b82beae77" \
    > "$root/state/retroarch/factory-config.sha256"
PLUMOS_ROOT=$root PLUMOS_BUSYBOX=/bin/busybox \
    /work/package/app-layer-pixel2/bin/plumos-retroarch-config-merge \
    > /tmp/regression.log
grep -q "retroarch_config=result-migrated-regression added=2" /tmp/regression.log
grep -q "^input_exit_emulator_btn = \"9\"$" "$active/retroarch.cfg"
grep -q "^input_menu_toggle_gamepad_combo = \"0\"$" "$active/retroarch.cfg"
grep -q "^input_save_state_btn = \"42\"$" "$active/retroarch.cfg"

# Correct the Pixel2 factory generation that disabled plain-DRM OSD. Only the
# two matching factory values are changed; an unrelated hotkey survives.
cp "$factory/retroarch.cfg" "$active/retroarch.cfg"
sed -i \
    -e "s|video_font_enable = \"true\"|video_font_enable = \"false\"|" \
    -e "s|video_font_path = \"/mnt/plumos/fonts/default.otf\"|video_font_path = \"\"|" \
    -e "s|input_save_state_btn = \"5\"|input_save_state_btn = \"42\"|" \
    "$active/retroarch.cfg"
printf "%s\n" "2db551beda3cd62e4f87d15d77a56ae2df1905fa3bc3925dbf5aaee803a4dcc6" \
    > "$root/state/retroarch/factory-config.sha256"
PLUMOS_ROOT=$root PLUMOS_BUSYBOX=/bin/busybox \
    /work/package/app-layer-pixel2/bin/plumos-retroarch-config-merge \
    > /tmp/osd.log
grep -q "retroarch_config=result-migrated-osd added=2" /tmp/osd.log
grep -q "^video_font_enable = \"true\"$" "$active/retroarch.cfg"
grep -q "^video_font_path = \"/mnt/plumos/fonts/default.otf\"$" \
    "$active/retroarch.cfg"
grep -q "^input_save_state_btn = \"42\"$" "$active/retroarch.cfg"

# The validated live-default generation accidentally saved black as its OSD
# message colour. Replace an active cfg only when it is byte-identical to that
# exact factory generation.
cp "$factory/retroarch.cfg" "$active/retroarch.cfg"
sed -i \
    -e "s|assets_directory = \"/mnt/plumos/retroarch/assets\"|assets_directory = \"/mnt/plumos/config/retroarch/assets\"|" \
    -e "s/video_message_color = \"ffff00\"/video_message_color = \"0\"/" \
    "$active/retroarch.cfg"
test "$(sha256sum "$active/retroarch.cfg" | cut -d " " -f 1)" = \
    "231ee2585779c098d9512a64cc8b17322c3b86e07d3e84889aaac815893d7280"
printf "%s\n" "231ee2585779c098d9512a64cc8b17322c3b86e07d3e84889aaac815893d7280" \
    > "$root/state/retroarch/factory-config.sha256"
PLUMOS_ROOT=$root PLUMOS_BUSYBOX=/bin/busybox \
    /work/package/app-layer-pixel2/bin/plumos-retroarch-config-merge \
    > /tmp/black-osd.log
grep -q "retroarch_config=result-replaced-black-osd added=0" \
    /tmp/black-osd.log
cmp "$factory/retroarch.cfg" "$active/retroarch.cfg"
grep -q "^video_message_color = \"ffff00\"$" "$active/retroarch.cfg"

# A cfg changed by the user is not byte-identical to the defective factory.
# Preserve both its explicit black colour and its unrelated hotkey.
cp "$factory/retroarch.cfg" "$active/retroarch.cfg"
sed -i \
    -e "s|assets_directory = \"/mnt/plumos/retroarch/assets\"|assets_directory = \"/mnt/plumos/config/retroarch/assets\"|" \
    -e "s/video_message_color = \"ffff00\"/video_message_color = \"0\"/" \
    -e "s/input_save_state_btn = \"5\"/input_save_state_btn = \"42\"/" \
    "$active/retroarch.cfg"
printf "%s\n" "231ee2585779c098d9512a64cc8b17322c3b86e07d3e84889aaac815893d7280" \
    > "$root/state/retroarch/factory-config.sha256"
PLUMOS_ROOT=$root PLUMOS_BUSYBOX=/bin/busybox \
    /work/package/app-layer-pixel2/bin/plumos-retroarch-config-merge \
    > /tmp/custom-black-osd.log
grep -q "retroarch_config=result-migrated-menu-assets added=1" \
    /tmp/custom-black-osd.log
grep -q "^assets_directory = \"/mnt/plumos/retroarch/assets\"$" \
    "$active/retroarch.cfg"
grep -q "^video_message_color = \"0\"$" "$active/retroarch.cfg"
grep -q "^input_save_state_btn = \"42\"$" "$active/retroarch.cfg"

# The legacy menu-assets path was empty on Pixel2. Move only that exact path
# to the app-layer-managed assets while preserving the selected menu,
# language and an unrelated hotkey.
sed -i \
    -e "s|assets_directory = \"/mnt/plumos/retroarch/assets\"|assets_directory = \"/mnt/plumos/config/retroarch/assets\"|" \
    -e "s|menu_driver = \"rgui\"|menu_driver = \"ozone\"|" \
    -e "s|user_language = \"0\"|user_language = \"1\"|" \
    "$active/retroarch.cfg"
PLUMOS_ROOT=$root PLUMOS_BUSYBOX=/bin/busybox \
    /work/package/app-layer-pixel2/bin/plumos-retroarch-config-merge \
    > /tmp/menu-assets.log
grep -q "retroarch_config=result-migrated-menu-assets added=1" \
    /tmp/menu-assets.log
grep -q "^assets_directory = \"/mnt/plumos/retroarch/assets\"$" \
    "$active/retroarch.cfg"
grep -q "^menu_driver = \"ozone\"$" "$active/retroarch.cfg"
grep -q "^user_language = \"1\"$" "$active/retroarch.cfg"
grep -q "^input_save_state_btn = \"42\"$" "$active/retroarch.cfg"

# The first graphical-menu factory was usable but too small on the physical
# 3.5-inch panel. Migrate only values that still match that generation while
# preserving the selected driver, language and unrelated user settings.
cp "$factory/retroarch.cfg" "$active/retroarch.cfg"
sed -i \
    -e "s/menu_scale_factor = \"1.500000\"/menu_scale_factor = \"1.000000\"/" \
    -e "s/ozone_font_scale = \"1\"/ozone_font_scale = \"0\"/" \
    -e "s/ozone_font_scale_factor_global = \"1.350000\"/ozone_font_scale_factor_global = \"1.000000\"/" \
    -e "s/menu_driver = \"rgui\"/menu_driver = \"ozone\"/" \
    -e "s/user_language = \"0\"/user_language = \"1\"/" \
    -e "s/input_save_state_btn = \"5\"/input_save_state_btn = \"42\"/" \
    "$active/retroarch.cfg"
printf "%s\n" "0fe41cce8ee36e4ca0b346ad2aee17563f08afb9f402fe1a3f8ea5e46a1744ef" \
    > "$root/state/retroarch/factory-config.sha256"
PLUMOS_ROOT=$root PLUMOS_BUSYBOX=/bin/busybox \
    /work/package/app-layer-pixel2/bin/plumos-retroarch-config-merge \
    > /tmp/menu-scale.log
grep -q "retroarch_config=result-migrated-menu-scale added=3" \
    /tmp/menu-scale.log
grep -q "^menu_scale_factor = \"1.500000\"$" "$active/retroarch.cfg"
grep -q "^ozone_font_scale = \"1\"$" "$active/retroarch.cfg"
grep -q "^ozone_font_scale_factor_global = \"1.350000\"$" \
    "$active/retroarch.cfg"
grep -q "^menu_driver = \"ozone\"$" "$active/retroarch.cfg"
grep -q "^user_language = \"1\"$" "$active/retroarch.cfg"
grep -q "^input_save_state_btn = \"42\"$" "$active/retroarch.cfg"
'
