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
PICOARCH="$ROOT_DIR/output/picoarch/pixel2/plumos"
STANDALONE="$ROOT_DIR/output/standalone/pixel2/plumos"
AUDIO_ROUTER="$ROOT_DIR/output/audio-router/pixel2/plumos"
PYXEL="$ROOT_DIR/output/pyxel-runtime/pixel2/plumos"
VERSION="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}"
SOURCE_REF="$(git -C "$ROOT_DIR" rev-parse --short HEAD)"
SOURCE_EPOCH="${SOURCE_DATE_EPOCH:-}"
[ -n "$SOURCE_EPOCH" ] || SOURCE_EPOCH="$(git -C "$ROOT_DIR" show -s --format=%ct HEAD)"

for component in "$FRONTEND" "$RETROARCH" "$CORES" "$PICOARCH" "$STANDALONE" "$AUDIO_ROUTER" "$PYXEL"; do
    [ -d "$component" ] || { printf 'error: missing component: %s\n' "$component" >&2; exit 1; }
done
(cd "$FRONTEND" && sha256sum -c components/frontend/checksums.sha256)
(cd "$RETROARCH" && sha256sum -c components/retroarch/checksums.sha256)
(cd "$CORES" && sha256sum -c components/libretro-cores/checksums.sha256)
(cd "$PICOARCH" && sha256sum -c components/picoarch/checksums.sha256)
(cd "$STANDALONE" && sha256sum -c components/standalone/checksums.sha256)
(cd "$AUDIO_ROUTER" && sha256sum -c components/audio-router/checksums.sha256)
(cd "$PYXEL" && sha256sum -c components/pyxel/checksums.sha256)

rm -rf "$OUT_ROOT"
mkdir -p "$PLUMOS_DIR"
rsync -a "$FRONTEND/" "$PLUMOS_DIR/"
rsync -a "$RETROARCH/" "$PLUMOS_DIR/"
rsync -a "$CORES/" "$PLUMOS_DIR/"
rsync -a "$PICOARCH/" "$PLUMOS_DIR/"
rsync -a "$STANDALONE/" "$PLUMOS_DIR/"
rsync -a "$AUDIO_ROUTER/" "$PLUMOS_DIR/"
rsync -a "$PYXEL/" "$PLUMOS_DIR/"
rsync -a "$ROOT_DIR/package/app-layer-pixel2/" "$PLUMOS_DIR/"
chmod 0755 "$PLUMOS_DIR/bin/"*
mkdir -p "$PLUMOS_DIR/config" "$PLUMOS_DIR/state" "$PLUMOS_DIR/saves" \
    "$PLUMOS_DIR/states" "$PLUMOS_DIR/logs" "$PLUMOS_DIR/updates"
printf '%s\n' "$VERSION" >"$PLUMOS_DIR/VERSION"
printf 'pixel2-rockchip-r1\n' >"$PLUMOS_DIR/COMPAT_VENDOR"
printf 'plumos-pixel2-app-layer-v1\n' >"$PLUMOS_DIR/RUNTIME_ABI"

# All components above are mandatory and checksum-verified before the
# assembler reaches this point.  A successful Pixel2 app-layer assembly is
# therefore complete regardless of whether the compatibility --strict flag was
# supplied.  Emitting complete=false here made the default command create an
# unusable tree and then fail its own verifier without a useful error.
complete=true
launch_profiles_json="$(
    jq -c '[.systems[] | select(.enabled != false) | .launch_profiles[]?] | unique' \
        "$PLUMOS_DIR/config/frontend/systems.json"
)"
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
  "components": ["frontend", "retroarch", "libretro-cores", "picoarch", "standalone", "audio-router", "pyxel"],
  "launch_profiles": $launch_profiles_json,
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
