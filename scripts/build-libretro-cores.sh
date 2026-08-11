#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if [ "${1:-}" != --inside ]; then
    exec "$ROOT_DIR/scripts/docker-build.sh" cores "$@"
fi
shift
FILTER=quicknes
while [ "$#" -gt 0 ]; do
    case "$1" in --filter) FILTER=$2; shift 2 ;; *) exit 2 ;; esac
done
[ "$FILTER" = quicknes ] || { printf 'error: only quicknes is implemented\n' >&2; exit 2; }

ROOT_DIR=/work
SOURCE_URL=https://github.com/libretro/QuickNES_Core.git
SOURCE_COMMIT=058d66516ed3f1260b69e5b71cd454eb7e9234a3
INFO_URL=https://github.com/libretro/libretro-core-info.git
INFO_COMMIT=beb3b8bb8175f27a295bcbce922dc846f5c6362f
WORK="$ROOT_DIR/output/build/quicknes-pixel2"
INFO_WORK="$ROOT_DIR/output/build/libretro-core-info-pixel2"
OUT_ROOT="$ROOT_DIR/output/libretro-cores/pixel2"
PLUMOS_DIR="$OUT_ROOT/plumos"
COMPONENT_DIR="$PLUMOS_DIR/components/libretro-cores"
SOURCE_REF="$(git -C "$ROOT_DIR" rev-parse --short HEAD)"

for spec in "$WORK|$SOURCE_URL|$SOURCE_COMMIT" "$INFO_WORK|$INFO_URL|$INFO_COMMIT"; do
    dir=${spec%%|*}; rest=${spec#*|}; url=${rest%%|*}; commit=${rest##*|}
    if [ ! -d "$dir/.git" ]; then rm -rf "$dir"; git clone "$url" "$dir"; fi
    git -C "$dir" fetch --quiet origin "$commit"
    git -C "$dir" checkout --quiet --detach "$commit"
    git -C "$dir" reset --hard --quiet "$commit"
    git -C "$dir" clean -fdx --quiet
done
make -C "$WORK" -j"${JOBS:-$(nproc)}" platform=unix

rm -rf "$OUT_ROOT"
mkdir -p "$PLUMOS_DIR/cores" "$PLUMOS_DIR/info" "$PLUMOS_DIR/licenses" "$COMPONENT_DIR"
install -m 0755 "$WORK/quicknes_libretro.so" "$PLUMOS_DIR/cores/quicknes_libretro.so"
strip "$PLUMOS_DIR/cores/quicknes_libretro.so" 2>/dev/null || true
install -m 0644 "$INFO_WORK/quicknes_libretro.info" "$PLUMOS_DIR/info/quicknes_libretro.info"
license=$(find "$WORK" -maxdepth 2 -type f \( -iname 'license*' -o -iname 'copying*' \) | head -1)
[ -n "$license" ] || { printf 'error: QuickNES license missing\n' >&2; exit 1; }
install -m 0644 "$license" "$PLUMOS_DIR/licenses/QuickNES-LICENSE"
cat >"$COMPONENT_DIR/manifest.json" <<EOF
{
  "name": "plumOS Pixel2 libretro cores",
  "component": "libretro-cores",
  "device": "pixel2",
  "architecture": "aarch64",
  "source_ref": "$SOURCE_REF",
  "cores": [{"id": "quicknes", "class": "bringup", "rendering": "software", "upstream": "$SOURCE_URL", "upstream_commit": "$SOURCE_COMMIT", "binary": "cores/quicknes_libretro.so"}]
}
EOF
(
    cd "$PLUMOS_DIR"
    sha256sum cores/quicknes_libretro.so info/quicknes_libretro.info \
        licenses/QuickNES-LICENSE components/libretro-cores/manifest.json
) >"$COMPONENT_DIR/checksums.sha256"
file "$PLUMOS_DIR/cores/quicknes_libretro.so"
printf 'libretro_cores=result-ok count=1 output=%s\n' "$PLUMOS_DIR"
