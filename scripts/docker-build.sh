#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"

usage() {
    printf '%s\n' \
        'Usage: scripts/docker-build.sh TARGET [ARGS...]' \
        '' \
        'Targets: image frontend retroarch cores picoarch standalone audio-router app-layer system-rootfs sd-image release-image'
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
        picoarch) exec ./scripts/build-picoarch-pixel2.sh --inside "$@" ;;
        standalone) exec ./scripts/build-standalone-pixel2.sh --inside "$@" ;;
        audio-router) exec ./scripts/build-audio-router-pixel2.sh "$@" ;;
        app-layer) exec ./scripts/build-app-layer.sh --inside "$@" ;;
        system-rootfs) exec ./scripts/build-system-rootfs.sh --inside "$@" ;;
        sd-image) exec ./scripts/build-sd-image.sh --inside "$@" ;;
        release-image)
            ./scripts/build-frontend-component.sh --inside
            ./scripts/build-retroarch.sh --inside
            ./scripts/build-libretro-cores.sh --inside --filter all
            ./scripts/build-picoarch-pixel2.sh --inside
            ./scripts/build-standalone-pixel2.sh --inside
            ./scripts/build-audio-router-pixel2.sh
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
host_boot_prefix="${PLUMOS_PIXEL2_BOOT_PREFIX:-$ROOT_DIR/artifacts/vendor/pixel2-stock-source/rockchip-boot-prefix.bin}"
host_boot_prefix="$(CDPATH= cd -- "$(dirname -- "$host_boot_prefix")" && pwd)/$(basename -- "$host_boot_prefix")"
case "$host_boot_prefix" in
    "$ROOT_DIR"/*) container_boot_prefix="/work/${host_boot_prefix#"$ROOT_DIR"/}" ;;
    *) printf 'error: boot prefix must be under the repository\n' >&2; exit 2 ;;
esac
docker_env=(
    -e "PLUMOS_PIXEL2_VERSION=${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}"
    -e "PLUMOS_PIXEL2_BOOT_PREFIX=$container_boot_prefix"
)
if [ -n "${SOURCE_DATE_EPOCH:-}" ]; then
    docker_env+=(-e "SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH")
fi
container="$(
    docker create --platform linux/arm64 \
    "${docker_env[@]}" \
    -v "$ROOT_DIR:/work" -w /work "$IMAGE" \
    ./scripts/docker-build.sh --inside "$@"
)"
cleanup_container() {
    [ -n "${container:-}" ] || return 0
    docker rm -f "$container" >/dev/null 2>&1 || true
}
trap cleanup_container INT TERM HUP
docker start "$container" >/dev/null
docker logs -f "$container"
rc="$(docker wait "$container")"
docker rm "$container" >/dev/null 2>&1 || true
trap - INT TERM HUP
exit "$rc"
