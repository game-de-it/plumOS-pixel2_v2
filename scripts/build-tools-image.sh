#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
IMAGE="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"

docker build \
    --platform "${PLUMOS_PIXEL2_TOOLS_PLATFORM:-linux/arm64}" \
    -t "$IMAGE" \
    -f "$ROOT_DIR/docker/pixel2-tools/Dockerfile" \
    "$ROOT_DIR"

