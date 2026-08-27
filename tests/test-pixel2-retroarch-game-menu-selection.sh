#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

docker run --rm --platform linux/arm64 \
    -v "$ROOT_DIR:/work:ro" busybox:latest sh -euc '
root=/tmp/plumos
user=/tmp/plumos-user
mkdir -p "$root/bin" "$root/cores" "$root/config/retroarch" \
    "$root/factory-defaults/retroarch" "$user/roms/nes"
touch "$root/cores/quicknes_libretro.so" \
    "$root/cores/flycast_libretro.so" "$user/roms/nes/game.nes"

cat >"$root/bin/retroarch" <<"EOF"
#!/bin/sh
append=
while [ "$#" -gt 0 ]; do
    case "$1" in
        --appendconfig) append=$2; shift 2 ;;
        *) shift ;;
    esac
done
cp "$append" "$PLUMOS_TEST_APPEND"
printf "%s\n" "${PLUMOS_GL_MENU_ROTATION:-unset}" >"$PLUMOS_TEST_ROTATION"
EOF
chmod 0755 "$root/bin/retroarch"

run_case() {
    name=$1
    menu=$2
    core=$3
    expected_video=$4
    expected_rotation=$5
    printf "menu_driver = \"%s\"\n" "$menu" >"$root/config/retroarch/retroarch.cfg"
    cp "$root/config/retroarch/retroarch.cfg" \
        "$root/factory-defaults/retroarch/retroarch.cfg"
    PLUMOS_ROOT=$root \
    PLUMOS_ROM_ROOT=$user/roms \
    PLUMOS_SDCARD_ROOT=$user \
    PLUMOS_RUNTIME_ROOT=/tmp/run-$name \
    PLUMOS_TEST_APPEND=/tmp/$name-append.cfg \
    PLUMOS_TEST_ROTATION=/tmp/$name-rotation \
        /work/package/app-layer-pixel2/bin/plumos-retroarch-launch \
            --system nes --core "$root/cores/$core" \
            --rom "$user/roms/nes/game.nes"
    grep -q "^video_driver = \"$expected_video\"$" /tmp/$name-append.cfg
    [ "$(cat /tmp/$name-rotation)" = "$expected_rotation" ]
}

run_case rgui rgui quicknes_libretro.so drm unset
run_case ozone ozone quicknes_libretro.so gl display
run_case xmb xmb quicknes_libretro.so gl display
run_case glui glui quicknes_libretro.so gl display
run_case hw-rgui rgui flycast_libretro.so gl content
run_case hw-ozone ozone flycast_libretro.so gl display
'

printf 'test-pixel2-retroarch-game-menu-selection: PASS\n'
