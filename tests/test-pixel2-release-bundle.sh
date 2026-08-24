#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
work="$(mktemp -d /tmp/plumos-pixel2-release-bundle.XXXXXX)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/app/licenses" "$work/image" "$ROOT_DIR/dist/test-release-parent"
trap 'rm -rf "$work" "$ROOT_DIR/dist/test-release-parent"' EXIT

printf '{"device":"pixel2","complete":true}\n' >"$work/app/manifest.json"
: >"$work/app/checksums.sha256"
truncate -s 1048576 "$work/image/plumOS-Pixel2-0.0.0-test.img"
image_sha="$(shasum -a 256 "$work/image/plumOS-Pixel2-0.0.0-test.img" | awk '{print $1}')"
cat >"$work/image/image.manifest" <<EOF
file=plumOS-Pixel2-0.0.0-test.img
image_size=1048576
image_sha256=$image_sha
user_filesystem=created-on-first-boot
EOF

PLUMOS_PIXEL2_RELEASE_TESTING=1 \
python3 "$ROOT_DIR/scripts/build-pixel2-release-bundle.py" \
    --version 0.0.0-test \
    --image "$work/image/plumOS-Pixel2-0.0.0-test.img" \
    --app-root "$work/app" \
    --output-dir "$ROOT_DIR/dist/test-release-parent/bundle" \
    --allow-dirty --skip-image-verifier >"$work/build.log"
grep -q 'release_bundle=result-ok' "$work/build.log"
grep -q 'image_7z=plumOS-Pixel2-v0.0.0-test-sd-image.7z' "$work/build.log"
archive="$ROOT_DIR/dist/test-release-parent/bundle/plumOS-Pixel2-v0.0.0-test-sd-image.7z"
test -f "$archive"
if command -v 7zz >/dev/null 2>&1; then
    7zz t "$archive" >/dev/null
else
    7z t "$archive" >/dev/null
fi
python3 "$ROOT_DIR/scripts/verify-pixel2-release-bundle.py" \
    "$ROOT_DIR/dist/test-release-parent/bundle" --allow-dirty >"$work/verify.log"
grep -q 'release_bundle_verify=result-ok' "$work/verify.log"

mkdir -p "$work/download-source" "$work/download-target"
printf 'redownload fixture\n' >"$work/download-source/asset.bin"
python3 - "$ROOT_DIR" "$work/download-source" "$work/download-target" <<'PY'
import importlib.util
from pathlib import Path
import sys

root = Path(sys.argv[1])
source = Path(sys.argv[2]).resolve()
target = Path(sys.argv[3]).resolve()
spec = importlib.util.spec_from_file_location(
    "pixel2_release_verifier",
    root / "scripts/verify-pixel2-release-bundle.py",
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.download_assets(source.as_uri() + "/", {"asset.bin"}, target)
if (target / "asset.bin").read_text() != "redownload fixture\n":
    raise SystemExit("release redownload existing-directory fixture mismatch")
PY

printf 'corrupt' >>"$ROOT_DIR/dist/test-release-parent/bundle/RELEASE_NOTES.md"
if python3 "$ROOT_DIR/scripts/verify-pixel2-release-bundle.py" \
    "$ROOT_DIR/dist/test-release-parent/bundle" --allow-dirty >"$work/reject.log" 2>&1; then
    echo 'release verifier accepted a corrupted asset' >&2
    exit 1
fi
grep -q 'release checksum mismatch: RELEASE_NOTES.md' "$work/reject.log"

prepare="$ROOT_DIR/scripts/prepare-pixel2-release.sh"
test "$(grep -c 'docker-build.sh.*sd-image' "$prepare")" -eq 2
test "$(grep -c 'run-pixel2-strict-release-gate.sh' "$prepare")" -eq 2
grep -q 'Validate the exact second image' "$prepare"

printf 'pixel2_release_bundle_test=result-ok\n'
