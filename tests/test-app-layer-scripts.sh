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
    package/app-layer-pixel2/bin/plumos-safe-shutdown; do
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
grep -q 'SOURCE_COMMIT=69a4f0ea' "$ROOT_DIR/scripts/build-retroarch.sh"
grep -q 'required_library in libpthread.so.0' \
    "$ROOT_DIR/scripts/build-retroarch.sh"
grep -q 'emulator/lib/libpthread.so.0' \
    "$ROOT_DIR/scripts/verify-app-layer.sh"
grep -q 'SOURCE_COMMIT=058d6651' "$ROOT_DIR/scripts/build-libretro-cores.sh"
if grep -R -E -i '(rocknix|emuelec|batocera|knulli|stockos)' \
    "$ROOT_DIR/package/frontend-pixel2" \
    "$ROOT_DIR/package/retroarch-pixel2" \
    "$ROOT_DIR/package/app-layer-pixel2" >/dev/null; then
    printf 'error: foreign distribution identity in Pixel2 app sources\n' >&2
    exit 1
fi
if grep -R -I -E -i '(^|[^[:alnum:]_])(MF|V90S)([^[:alnum:]_]|$)' \
    "$ROOT_DIR/package/frontend-pixel2" \
    "$ROOT_DIR/package/app-layer-pixel2" \
    "$ROOT_DIR/vendor/plumos-frontend"; then
    printf 'error: foreign device identity in Pixel2 frontend sources\n' >&2
    exit 1
fi
printf 'app_layer_scripts=result-ok\n'
