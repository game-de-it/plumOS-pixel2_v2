#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
PATCHER="$ROOT_DIR/package/portmaster-pixel2/plumos/apps/portmaster/adapter/plumos_moonlight_gui_patch.py"
FIXTURE_DIR="$(mktemp -d)"
trap 'rm -rf "$FIXTURE_DIR"' EXIT

python3 - "$PATCHER" "$FIXTURE_DIR/main.lua" <<'PY'
import importlib.util
from pathlib import Path
import sys

patcher_path = Path(sys.argv[1])
target = Path(sys.argv[2])
spec = importlib.util.spec_from_file_location("moonlight_patch", patcher_path)
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)

execute = module.EXEC_OLD.replace(module.RUNTIME_PATH, module.RUNTIME_PATHS[0])
target.write_text(
    "-- known upstream fixture\n"
    + module.FONT_OLD
    + "\n"
    + module.APP_FILTER_OLD
    + "\n"
    + execute,
    encoding="utf-8",
)
PY

python3 "$PATCHER" "$FIXTURE_DIR/main.lua" "$FIXTURE_DIR/backups"
first_hash="$(sha256sum "$FIXTURE_DIR/main.lua" | awk '{print $1}')"
python3 "$PATCHER" "$FIXTURE_DIR/main.lua" "$FIXTURE_DIR/backups"
second_hash="$(sha256sum "$FIXTURE_DIR/main.lua" | awk '{print $1}')"

test "$first_hash" = "$second_hash"
test "$(find "$FIXTURE_DIR/backups" -type f | wc -l)" -eq 1
grep -Fq -- '-- plumOS Pixel2 Moonlight GUI adapter 52' "$FIXTURE_DIR/main.lua"
grep -Fq 'math.max(28, 38 * scaleFactor)' "$FIXTURE_DIR/main.lua"
grep -Fq 'appName and appName ~= "Load apps first"' "$FIXTURE_DIR/main.lua"
grep -Fq 'local runInBackground = command:match("^moonlight pair ") ~= nil' "$FIXTURE_DIR/main.lua"
grep -Fq 'local backgroundSuffix = runInBackground and " &" or ""' "$FIXTURE_DIR/main.lua"
grep -Fq '2>&1%s' "$FIXTURE_DIR/main.lua"
! grep -Fq '2>&1 &' "$FIXTURE_DIR/main.lua"

printf 'portmaster_pixel2_moonlight_gui=result-ok\n'
