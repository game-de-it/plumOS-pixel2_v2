#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
VERSION=""

usage() {
    printf '%s\n' \
        'Usage: scripts/prepare-pixel2-release.sh --version VERSION' \
        '' \
        'Builds, verifies, compresses, and packages a release locally.' \
        'It does not create a Git tag, GitHub Release, or upload anything.'
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --version) VERSION="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'error: unknown argument: %s\n' "$1" >&2; exit 2 ;;
    esac
done
[ -n "$VERSION" ] || { usage >&2; exit 2; }

cd "$ROOT_DIR"
dirty="$(git status --short)"
[ -z "$dirty" ] || {
    printf 'release-prepare: FAIL: commit all changes first\n%s\n' "$dirty" >&2
    exit 1
}

PLUMOS_PIXEL2_VERSION="$VERSION" \
    "$ROOT_DIR/scripts/docker-build.sh" release-image
PLUMOS_PIXEL2_VERSION="$VERSION" \
    "$ROOT_DIR/scripts/run-pixel2-strict-release-gate.sh" --version "$VERSION"

IMAGE="$ROOT_DIR/output/image/pixel2/plumOS-Pixel2-$VERSION.img"
FIRST_SHA="$(shasum -a 256 "$IMAGE" | awk '{print $1}')"
PLUMOS_PIXEL2_VERSION="$VERSION" \
    "$ROOT_DIR/scripts/docker-build.sh" sd-image
SECOND_SHA="$(shasum -a 256 "$IMAGE" | awk '{print $1}')"
[ "$FIRST_SHA" = "$SECOND_SHA" ] || {
    printf 'release-prepare: FAIL: SD image is not reproducible\nfirst=%s\nsecond=%s\n' \
        "$FIRST_SHA" "$SECOND_SHA" >&2
    exit 1
}
printf 'release_image_reproducibility=result-ok sha256=%s\n' "$SECOND_SHA"

python3 "$ROOT_DIR/scripts/build-pixel2-release-bundle.py" --version "$VERSION"

printf 'release_prepare=result-ok version=%s publish_action=not-performed\n' "$VERSION"
