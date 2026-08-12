#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if [ "${1:-}" != --inside ]; then
    exec "$ROOT_DIR/scripts/docker-build.sh" standalone "$@"
fi
shift

ROOT_DIR=/work
OUT_ROOT="${PLUMOS_PIXEL2_STANDALONE_OUT:-$ROOT_DIR/output/standalone/pixel2}"
PLUMOS_DIR="$OUT_ROOT/plumos"
COMPONENT_DIR="$PLUMOS_DIR/components/standalone"
PACKAGE_ROOT="$ROOT_DIR/package/standalone-pixel2/plumos"

rm -rf "$OUT_ROOT"
mkdir -p "$PLUMOS_DIR" "$COMPONENT_DIR" "$PLUMOS_DIR/licenses"
rsync -a "$PACKAGE_ROOT/" "$PLUMOS_DIR/"
chmod 0755 "$PLUMOS_DIR/bin/plumos-standalone-launch" \
  "$PLUMOS_DIR/bin/plumos-standalone-stop"

cat > "$COMPONENT_DIR/manifest.json" <<'EOF'
{
  "name": "plumOS Pixel2 standalone launcher",
  "component": "standalone",
  "device": "pixel2",
  "architecture": "aarch64",
  "status": "launcher-only",
  "runtime_contract": "Pixel2 app-layer launcher, ALSA plumos_output, SDL/KMSDRM defaults",
  "emulators": [
    {"id": "pcsx_rearmed", "status": "pending-binary"},
    {"id": "ppsspp", "status": "pending-binary"},
    {"id": "drastic", "status": "pending-binary"},
    {"id": "yabasanshiro", "status": "pending-binary"},
    {"id": "openbor", "status": "pending-binary"},
    {"id": "scummvm", "status": "pending-binary"},
    {"id": "easyrpg", "status": "pending-binary"},
    {"id": "flycast", "status": "pending-binary"},
    {"id": "mupen64plus", "status": "pending-binary"},
    {"id": "nxengine-evo", "status": "pending-binary"}
  ]
}
EOF

(
  cd "$PLUMOS_DIR"
  find bin config/standalone components/standalone -type f \
    ! -path 'components/standalone/checksums.sha256' \
    -print |
    sort |
    while IFS= read -r path; do sha256sum "$path"; done
) > "$COMPONENT_DIR/checksums.sha256"
(
  cd "$PLUMOS_DIR"
  sha256sum -c components/standalone/checksums.sha256 >/dev/null
)
printf 'created: %s\n' "$PLUMOS_DIR"
