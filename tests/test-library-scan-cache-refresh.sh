#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/plumos-library-cache-test.XXXXXX")"
trap 'rm -rf "$TEST_ROOT"' EXIT

mkdir -p \
    "$TEST_ROOT/card/Roms/nes" \
    "$TEST_ROOT/card/Roms/VMU" \
    "$TEST_ROOT/plumos/config/frontend" \
    "$TEST_ROOT/plumos/state/frontend/systems"
cp "$ROOT_DIR/package/frontend-pixel2/systems.json" \
    "$TEST_ROOT/plumos/config/frontend/systems.json"
printf 'test-rom\n' >"$TEST_ROOT/card/Roms/nes/cache-refresh.nes"
printf 'metadata-only\n' >"$TEST_ROOT/card/Roms/VMU/ANIMTEST.VMI"
printf '%s\n' \
    '{"systems":[{"id":"vmu","rom_count":1,"roms":[{"file_name":"ANIMTEST.VMI"}]}]}' \
    >"$TEST_ROOT/plumos/state/frontend/systems/vmu.json"

cc -std=gnu99 -O0 -Wall -Wextra \
    "$ROOT_DIR/vendor/plumos-frontend/src/plumos_library_scan.c" \
    -o "$TEST_ROOT/plumos-library-scan"

PLUMOS_ROOT="$TEST_ROOT/plumos" \
PLUMOS_SDCARD_ROOT="$TEST_ROOT/card" \
PLUMOS_SYSTEMS_JSON="$TEST_ROOT/plumos/config/frontend/systems.json" \
    "$TEST_ROOT/plumos-library-scan" --defer-thumbnails >"$TEST_ROOT/scan.log"

enabled_count="$(jq '[.systems[] | select(.enabled == true)] | length' \
    "$ROOT_DIR/package/frontend-pixel2/systems.json")"
grep -q "^wrote_system_caches=$enabled_count$" "$TEST_ROOT/scan.log"
jq -e '(.systems | length) == 1 and .systems[0].id == "nes" and
       .systems[0].rom_count == 1 and
       .systems[0].roms[0].file_name == "cache-refresh.nes"' \
    "$TEST_ROOT/plumos/state/frontend/systems/nes.json" >/dev/null
jq -e '(.systems | length) == 1 and .systems[0].id == "vmu" and
       .systems[0].rom_count == 0 and
       (.systems[0].roms | length) == 0' \
    "$TEST_ROOT/plumos/state/frontend/systems/vmu.json" >/dev/null
test ! -e "$TEST_ROOT/plumos/state/frontend/systems/saturn.json"

printf 'library_scan_cache_refresh=result-ok enabled=%s\n' "$enabled_count"
