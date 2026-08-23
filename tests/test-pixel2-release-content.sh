#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
AUDIT="$ROOT_DIR/scripts/audit-pixel2-release-content.py"
work="$(mktemp -d /tmp/plumos-pixel2-release-content.XXXXXX)"
trap 'rm -rf "$work"' EXIT

python3 "$AUDIT" --tracked-only --json "$work/tracked.json"
jq -e '.result == "ok" and (.findings | length) == 0' "$work/tracked.json" >/dev/null

mkdir -p "$work/app/state" "$work/app/licenses"
printf '{"device":"pixel2","complete":true}\n' >"$work/app/manifest.json"
: >"$work/app/checksums.sha256"
python3 "$AUDIT" --app-root "$work/app" --json "$work/clean.json"
jq -e '.result == "ok"' "$work/clean.json" >/dev/null

printf 'private\n' >"$work/app/state/user-save.dat"
if python3 "$AUDIT" --app-root "$work/app" >"$work/rejected.log" 2>&1; then
    echo 'release content audit accepted mutable state' >&2
    exit 1
fi
grep -q 'mutable app-layer file: state/user-save.dat' "$work/rejected.log"
rm "$work/app/state/user-save.dat"

printf '%s%s\n' '-----BEGIN OPENSSH ' 'PRIVATE KEY-----' \
    >"$work/app/licenses/test.txt"
if python3 "$AUDIT" --app-root "$work/app" >"$work/key-rejected.log" 2>&1; then
    echo 'release content audit accepted a private key' >&2
    exit 1
fi
grep -q 'private key material in app layer' "$work/key-rejected.log"

printf 'pixel2_release_content_test=result-ok\n'
