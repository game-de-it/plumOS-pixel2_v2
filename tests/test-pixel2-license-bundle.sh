#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
AUDIT="$ROOT_DIR/scripts/audit-pixel2-license-bundle.sh"

for path in \
    LICENSE NOTICE.md THIRD_PARTY_NOTICES.md THIRD_PARTY_NOTICES.ja.md \
    package/licenses-pixel2/runtime-license-index.tsv \
    package/licenses-pixel2/pixel2-stock-vendor-runtime-NOTICE.txt \
    package/licenses-pixel2/drastic-upstream-NOTICE.txt \
    package/licenses-pixel2/nextcommander-upstream-NOTICE.txt; do
    test -s "$ROOT_DIR/$path"
done

grep -q 'executable is separately authored closed software' \
    "$ROOT_DIR/THIRD_PARTY_NOTICES.md"
grep -q 'NOASSERTION' "$ROOT_DIR/THIRD_PARTY_NOTICES.md"
grep -q 'does not relicense the NextCommander' \
    "$ROOT_DIR/package/licenses-pixel2/nextcommander-upstream-NOTICE.txt"
grep -q 'audit-pixel2-license-bundle.sh' \
    "$ROOT_DIR/scripts/build-app-layer.sh"
grep -q 'pixel2-stock-vendor-runtime-NOTICE.txt' \
    "$ROOT_DIR/scripts/build-system-rootfs.sh"

test -x "$AUDIT"
printf 'pixel2_license_bundle_test=result-ok\n'
