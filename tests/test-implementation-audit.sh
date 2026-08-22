#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
AUDIT="$ROOT_DIR/scripts/audit-pixel2-implementation.py"

work="$(mktemp -d /tmp/plumos-pixel2-audit-test.XXXXXX)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/app"
PYTHONPYCACHEPREFIX="$work/pycache" python3 -m py_compile "$AUDIT"

python3 "$AUDIT" --app-root "$ROOT_DIR/output/app-layer/pixel2/plumos" \
  --json "$work/audit.json" --markdown "$work/audit.md"
jq -e '.device == "pixel2"' "$work/audit.json" >/dev/null
jq -e '.metrics.systems_enabled > 0' "$work/audit.json" >/dev/null
jq -e '.metrics.release_blockers == 0' "$work/audit.json" >/dev/null
grep -q '^# Pixel2 implementation audit$' "$work/audit.md"

python3 "$AUDIT" --app-root "$ROOT_DIR/output/app-layer/pixel2/plumos" \
  --release-gate >"$work/release-gate.log" 2>&1
grep -q 'implementation_audit=result-ok release_blockers=0' "$work/release-gate.log"
grep -q './scripts/audit-pixel2-implementation.py --release-gate' \
  "$ROOT_DIR/scripts/docker-build.sh"
jq -e '.systems[] | select(.id == "vmu") | (.extensions | index("vmi") | not)' \
  "$ROOT_DIR/package/frontend-pixel2/systems.json" >/dev/null
grep -q 'NON_LAUNCHABLE_DESCRIPTOR_EXTENSIONS' "$AUDIT"

printf 'implementation_audit_test=result-ok\n'
