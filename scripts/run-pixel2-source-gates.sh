#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$ROOT_DIR"

git diff --check

shell_tests=(
    tests/test-app-layer-scripts.sh
    tests/test-system-rootfs-scripts.sh
    tests/test-sd-image-scripts.sh
    tests/test-stock-capture-scripts.sh
    tests/test-implementation-audit.sh
    tests/test-pixel2-emulator-menu-contract.sh
    tests/test-pixel2-pico8-wget.sh
    tests/test-pixel2-gw-core-geometry.sh
    tests/test-pixel2-license-bundle.sh
    tests/test-pixel2-release-content.sh
    tests/test-pixel2-release-bundle.sh
)

for test_script in "${shell_tests[@]}"; do
    printf 'source-gate: RUN %s\n' "$test_script"
    "$ROOT_DIR/$test_script"
done

python3 "$ROOT_DIR/tests/test_pyxel_pixel2_geometry.py"
python3 -m py_compile \
    "$ROOT_DIR/scripts/audit-pixel2-implementation.py" \
    "$ROOT_DIR/scripts/audit-pixel2-release-content.py" \
    "$ROOT_DIR/scripts/build-pixel2-audit-fixture.py" \
    "$ROOT_DIR/scripts/build-pixel2-release-bundle.py" \
    "$ROOT_DIR/scripts/verify-pixel2-release-bundle.py"

printf 'pixel2_source_gates=result-ok tests=%s\n' "$(( ${#shell_tests[@]} + 1 ))"
