#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
REPAIR="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-mame2003-xtreme-config-repair"
TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

options="$TMP_ROOT/config/MAME 2003-Xtreme/MAME 2003-Xtreme.opt"
recovery="$TMP_ROOT/recovery"
mkdir -p "${options%/*}"
printf '%s\n' \
    'mame2003-xtreme-amped-turboboost = "X6"' \
    'mame2003-xtreme-amped-oc = "100"' >"$options"

"$REPAIR" "$options" "$recovery" disabled >"$TMP_ROOT/repair.log"
grep -q 'result-repaired field=turboboost reason=stale-x6 value=disabled' \
    "$TMP_ROOT/repair.log"
grep -q '^mame2003-xtreme-amped-turboboost = "disabled"$' "$options"
grep -q '^mame2003-xtreme-amped-oc = "100"$' "$options"
grep -q '^mame2003-xtreme-amped-turboboost = "X6"$' \
    "$recovery/MAME-2003-Xtreme.opt.before-turboboost-migration"
test -e "$recovery/mame2003-xtreme-turboboost-v1-migrated"

# After migration, an explicit user X6 selection must remain untouched.
printf '%s\n' \
    'mame2003-xtreme-amped-turboboost = "X6"' \
    'mame2003-xtreme-amped-oc = "100"' >"$options"
"$REPAIR" "$options" "$recovery" disabled >"$TMP_ROOT/after.log"
grep -q 'reason=migration-complete' "$TMP_ROOT/after.log"
grep -q '^mame2003-xtreme-amped-turboboost = "X6"$' "$options"

# A fresh installation receives the plumOS default before the first launch.
fresh="$TMP_ROOT/fresh/MAME 2003-Xtreme.opt"
"$REPAIR" "$fresh" "$TMP_ROOT/fresh-recovery" disabled >"$TMP_ROOT/fresh.log"
grep -q 'result-installed field=turboboost value=disabled' "$TMP_ROOT/fresh.log"
grep -q '^mame2003-xtreme-amped-turboboost = "disabled"$' "$fresh"

# Any pre-existing non-X6 choice is preserved and marked as migrated.
custom="$TMP_ROOT/custom/MAME 2003-Xtreme.opt"
mkdir -p "${custom%/*}"
printf '%s\n' 'mame2003-xtreme-amped-turboboost = "X2"' >"$custom"
"$REPAIR" "$custom" "$TMP_ROOT/custom-recovery" disabled >"$TMP_ROOT/custom.log"
grep -q 'reason=user-selection-preserved' "$TMP_ROOT/custom.log"
grep -q '^mame2003-xtreme-amped-turboboost = "X2"$' "$custom"

printf 'test-mame2003-xtreme-config-repair: ok\n'
