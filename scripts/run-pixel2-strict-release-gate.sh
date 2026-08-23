#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
VERSION="${PLUMOS_PIXEL2_VERSION:-}"
IMAGE=""

usage() {
    printf '%s\n' \
        'Usage: scripts/run-pixel2-strict-release-gate.sh --version VERSION [--image PATH]' \
        '' \
        'Requires a clean tree and fully assembled app-layer, System, and SD image.'
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --version) VERSION="$2"; shift 2 ;;
        --image) IMAGE="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'error: unknown argument: %s\n' "$1" >&2; exit 2 ;;
    esac
done
[ -n "$VERSION" ] || { usage >&2; exit 2; }

cd "$ROOT_DIR"
dirty="$(git status --short)"
[ -z "$dirty" ] || {
    printf 'strict-release: FAIL: git worktree is dirty\n%s\n' "$dirty" >&2
    exit 1
}

APP_ROOT="$ROOT_DIR/output/app-layer/pixel2/plumos"
SYSTEM_ROOT="$ROOT_DIR/output/system-rootfs/pixel2/rootfs"
SYSTEM_IMAGE="$ROOT_DIR/output/system-rootfs/pixel2/payload/system-slots/system-a.squashfs"
BOOT_PREFIX="${PLUMOS_PIXEL2_BOOT_PREFIX:-$ROOT_DIR/artifacts/vendor/pixel2-stock-source/rockchip-boot-prefix.bin}"
if [ -z "$IMAGE" ]; then
    IMAGE="$ROOT_DIR/output/image/pixel2/plumOS-Pixel2-$VERSION.img"
fi

"$ROOT_DIR/scripts/run-pixel2-source-gates.sh"
"$ROOT_DIR/scripts/verify-app-layer.sh" "$APP_ROOT"
"$ROOT_DIR/scripts/audit-pixel2-license-bundle.sh" "$APP_ROOT" "$SYSTEM_ROOT"
python3 "$ROOT_DIR/scripts/audit-pixel2-implementation.py" \
    --app-root "$APP_ROOT" --release-gate
"$ROOT_DIR/scripts/verify-system-rootfs.sh" "$SYSTEM_IMAGE"
"$ROOT_DIR/scripts/verify-sd-image.sh" "$IMAGE" "$BOOT_PREFIX"
python3 "$ROOT_DIR/scripts/audit-pixel2-release-content.py" \
    --app-root "$APP_ROOT" --image "$IMAGE"

python3 - "$VERSION" "$APP_ROOT" "$SYSTEM_ROOT" "$IMAGE" <<'PY'
import json
import subprocess
import sys
from pathlib import Path

version, app_root, system_root, image = sys.argv[1:]
app = json.loads((Path(app_root) / "manifest.json").read_text())
system = json.loads((Path(system_root) / "usr/lib/plumos/system-manifest.json").read_text())
source_ref = subprocess.check_output(
    ["git", "rev-parse", "--short", "HEAD"], text=True
).strip()
if app.get("version") != version:
    raise SystemExit(f"app-layer version mismatch: {app.get('version')} != {version}")
if system.get("version") != version:
    raise SystemExit(f"System version mismatch: {system.get('version')} != {version}")
if app.get("source_ref") != source_ref:
    raise SystemExit(f"app-layer source mismatch: {app.get('source_ref')} != {source_ref}")
if system.get("source_ref") != source_ref:
    raise SystemExit(f"System source mismatch: {system.get('source_ref')} != {source_ref}")
values = {}
for line in Path(image).with_name("image.manifest").read_text().splitlines():
    if "=" in line:
        key, value = line.split("=", 1)
        values[key] = value
if Path(values.get("file", "")).name != Path(image).name:
    raise SystemExit("image manifest filename mismatch")
if values.get("source_ref") != source_ref:
    raise SystemExit(f"image source mismatch: {values.get('source_ref')} != {source_ref}")
PY

printf 'strict_release=result-ok version=%s image=%s\n' "$VERSION" "$IMAGE"
