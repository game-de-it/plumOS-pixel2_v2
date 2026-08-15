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
'
