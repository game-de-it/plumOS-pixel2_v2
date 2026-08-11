#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"

usage() {
    printf '%s\n' \
        'Usage: scripts/docker-build.sh TARGET [ARGS...]' \
        '' \
        'Targets: image frontend retroarch cores app-layer system-rootfs sd-image release-image'
}

if [ "${1:-}" = --inside ]; then
    shift
    target=${1:-}
    [ -n "$target" ] || { usage >&2; exit 2; }
    shift
    case "$target" in
        frontend) exec ./scripts/build-frontend-component.sh --inside "$@" ;;
        retroarch) exec ./scripts/build-retroarch.sh --inside "$@" ;;
        cores) exec ./scripts/build-libretro-cores.sh --inside "$@" ;;
        app-layer) exec ./scripts/build-app-layer.sh --inside "$@" ;;
        system-rootfs) exec ./scripts/build-system-rootfs.sh --inside "$@" ;;
        sd-image) exec ./scripts/build-sd-image.sh --inside "$@" ;;
        release-image)
            ./scripts/build-frontend-component.sh --inside
            ./scripts/build-retroarch.sh --inside
            ./scripts/build-libretro-cores.sh --inside --filter quicknes
            ./scripts/build-app-layer.sh --inside --strict
            ./scripts/build-system-rootfs.sh --inside
            exec ./scripts/build-sd-image.sh --inside
            ;;
        *) printf 'error: unknown target: %s\n' "$target" >&2; exit 2 ;;
    esac
fi

target=${1:-}
[ -n "$target" ] || { usage >&2; exit 2; }
if [ "$target" = image ]; then
    exec docker build --platform linux/arm64 -t "$IMAGE" "$ROOT_DIR/docker/pixel2-tools"
fi
docker image inspect "$IMAGE" >/dev/null 2>&1 || "$ROOT_DIR/scripts/build-tools-image.sh"
exec docker run --rm --platform linux/arm64 \
    -e SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-}" \
    -e PLUMOS_PIXEL2_VERSION="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}" \
    -e PLUMOS_PIXEL2_BOOT_PREFIX="/work/artifacts/vendor/pixel2-stock-source/rockchip-boot-prefix.bin" \
    -v "$ROOT_DIR:/work" -w /work "$IMAGE" \
    ./scripts/docker-build.sh --inside "$@"
