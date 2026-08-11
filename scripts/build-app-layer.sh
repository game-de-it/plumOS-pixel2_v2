#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if [ "${1:-}" != --inside ]; then
    exec "$ROOT_DIR/scripts/docker-build.sh" app-layer "$@"
fi
shift
STRICT=0
while [ "$#" -gt 0 ]; do
    case "$1" in --strict) STRICT=1; shift ;; *) exit 2 ;; esac
done

ROOT_DIR=/work
OUT_ROOT="$ROOT_DIR/output/app-layer/pixel2"
PLUMOS_DIR="$OUT_ROOT/plumos"
FRONTEND="$ROOT_DIR/output/frontend/pixel2/plumos"
RETROARCH="$ROOT_DIR/output/retroarch/pixel2/plumos"
CORES="$ROOT_DIR/output/libretro-cores/pixel2/plumos"
VERSION="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}"
SOURCE_REF="$(git -C "$ROOT_DIR" rev-parse --short HEAD)"
SOURCE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[ -n "$SOURCE_EPOCH" ] || SOURCE_EPOCH="$(git -C "$ROOT_DIR" show -s --format=%ct HEAD)"

for component in "$FRONTEND" "$RETROARCH" "$CORES"; do
    [ -d "$component" ] || { printf 'error: missing component: %s\n' "$component" >&2; exit 1; }
done
(cd "$FRONTEND" && sha256sum -c components/frontend/checksums.sha256)
(cd "$RETROARCH" && sha256sum -c components/retroarch/checksums.sha256)
(cd "$CORES" && sha256sum -c components/libretro-cores/checksums.sha256)

rm -rf "$OUT_ROOT"
mkdir -p "$PLUMOS_DIR"
rsync -a "$FRONTEND/" "$PLUMOS_DIR/"
rsync -a "$RETROARCH/" "$PLUMOS_DIR/"
rsync -a "$CORES/" "$PLUMOS_DIR/"
rsync -a "$ROOT_DIR/package/app-layer-pixel2/" "$PLUMOS_DIR/"
chmod 0755 "$PLUMOS_DIR/bin/"*
mkdir -p "$PLUMOS_DIR/config" "$PLUMOS_DIR/state" "$PLUMOS_DIR/saves" \
    "$PLUMOS_DIR/states" "$PLUMOS_DIR/logs" "$PLUMOS_DIR/updates"
printf '%s\n' "$VERSION" >"$PLUMOS_DIR/VERSION"
printf 'pixel2-rockchip-r1\n' >"$PLUMOS_DIR/COMPAT_VENDOR"
printf 'plumos-pixel2-app-layer-v1\n' >"$PLUMOS_DIR/RUNTIME_ABI"

complete=false
[ "$STRICT" -eq 0 ] || complete=true
cat >"$PLUMOS_DIR/manifest.json" <<EOF
{
  "name": "plumOS Pixel2 app layer",
  "device": "pixel2",
  "architecture": "aarch64",
  "version": "$VERSION",
  "runtime_abi": "plumos-pixel2-app-layer-v1",
  "compat_vendor": "pixel2-rockchip-r1",
  "source_ref": "$SOURCE_REF",
  "source_date_epoch": $SOURCE_EPOCH,
  "complete": $complete,
  "components": ["frontend", "retroarch", "libretro-cores"],
  "launch_profiles": ["retroarch:quicknes"],
  "missing_components": []
}
EOF
(
    cd "$PLUMOS_DIR"
    find . -type f \
        ! -path './checksums.sha256' \
        ! -path './state/*' \
        ! -path './saves/*' \
        ! -path './states/*' \
        ! -path './logs/*' \
        ! -path './updates/*' \
        -print | sed 's#^./##' | sort |
        while IFS= read -r file; do sha256sum "$file"; done
) >"$PLUMOS_DIR/checksums.sha256"
"$ROOT_DIR/scripts/verify-app-layer.sh" "$PLUMOS_DIR"
printf 'app_layer=result-ok strict=%s output=%s\n' "$STRICT" "$PLUMOS_DIR"
