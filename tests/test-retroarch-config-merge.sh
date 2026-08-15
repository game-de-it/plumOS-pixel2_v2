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
'
