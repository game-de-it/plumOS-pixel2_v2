#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-bios-test.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
mkdir -p \
    "$tmp/app/config/frontend" "$tmp/app/info" \
    "$tmp/rom/bios/Databases" "$tmp/rom/bios/Machines/Test" \
    "$tmp/blue/Machines/MSX" "$tmp/blue/Databases" \
    "$tmp/out"
cat >"$tmp/app/config/frontend/systems.json" <<'EOF'
{"systems":[{"id":"test","enabled":true,"launch_profiles":["retroarch:test","retroarch:beetle_saturn","retroarch:bluemsx","standalone:drastic"]}]}
EOF
cat >"$tmp/app/manifest.json" <<'EOF'
{"source_ref":"fixture"}
EOF
cat >"$tmp/app/info/test_libretro.info" <<'EOF'
firmware_count = "3"
firmware0_path = "required.bin"
firmware0_opt = "false"
firmware1_path = "optional.bin"
firmware1_opt = "true"
firmware2_path = "Databases/test.xml"
firmware2_desc = "'Databases' folder"
firmware2_opt = "false"
EOF
cat >"$tmp/app/info/mednafen_saturn_libretro.info" <<'EOF'
firmware_count = "1"
firmware0_path = "saturn-required.bin"
firmware0_opt = "false"
EOF
cat >"$tmp/app/info/bluemsx_libretro.info" <<'EOF'
firmware_count = "2"
firmware0_path = "Databases/msxromdb.xml"
firmware0_desc = "'Databases' folder"
firmware0_opt = "false"
firmware1_path = "Machines/Shared Roms/MSX.rom"
firmware1_desc = "'Machines' folder"
firmware1_opt = "false"
EOF
printf 'required\n' >"$tmp/rom/bios/REQUIRED.BIN"
printf 'saturn\n' >"$tmp/rom/bios/saturn-required.bin"
printf 'database\n' >"$tmp/rom/bios/Databases/test.xml"
printf 'extra\n' >"$tmp/rom/bios/Databases/extra.xml"
printf 'machine\n' >"$tmp/rom/bios/Machines/Test/machine.rom"
printf 'arm7\n' >"$tmp/rom/bios/drastic_bios_arm7.bin"
printf 'arm9\n' >"$tmp/rom/bios/drastic_bios_arm9.bin"
printf 'msx-machine\n' >"$tmp/blue/Machines/MSX/config.ini"
printf 'msx-shared\n' >"$tmp/blue/Machines/MSX.rom"
printf 'msx-database\n' >"$tmp/blue/Databases/msxromdb.xml"
python3 - "$tmp/blue" "$tmp/rom/bios/blueMSXv282full.zip" <<'PY'
import pathlib, sys, zipfile
source = pathlib.Path(sys.argv[1])
with zipfile.ZipFile(sys.argv[2], "w") as archive:
    archive.write(source / "Machines/MSX/config.ini", "Machines/MSX/config.ini")
    archive.write(source / "Machines/MSX.rom", "Machines/Shared Roms/MSX.rom")
    archive.write(source / "Databases/msxromdb.xml", "Databases/msxromdb.xml")
    archive.writestr("../escape", "unsafe")
PY

python3 "$ROOT_DIR/scripts/prepare-pixel2-bios.py" \
    --app-root "$tmp/app" --rom-root "$tmp/rom" \
    --output "$tmp/out/bios" --report "$tmp/out/report.json" \
    >"$tmp/result.log"

grep -q '^bios_prepare=result-ok ' "$tmp/result.log"
grep -q '^missing_required=0$' "$tmp/result.log"
grep -q '^missing_optional=1$' "$tmp/result.log"
test -f "$tmp/out/bios/required.bin"
test -f "$tmp/out/bios/saturn-required.bin"
test -f "$tmp/out/bios/Databases/test.xml"
test -f "$tmp/out/bios/Databases/extra.xml"
test -f "$tmp/out/bios/drastic_bios_arm7.bin"
test -f "$tmp/out/bios/drastic_bios_arm9.bin"
test -f "$tmp/out/bios/Machines/MSX/config.ini"
test -f "$tmp/out/bios/Machines/Shared Roms/MSX.rom"
test -f "$tmp/out/bios/Databases/msxromdb.xml"
test ! -e "$tmp/out/escape"
(cd "$tmp/out/bios" && sha256sum -c plumos-bios-checksums.sha256 >/dev/null)
python3 - "$tmp/out/report.json" <<'PY'
import json, sys
data=json.load(open(sys.argv[1]))
assert data["device"] == "pixel2"
assert data["source_ref"] == "fixture"
assert data["missing_required"] == 0
assert [x["destination"] for x in data["missing"]] == ["optional.bin"]
assert any(x["destination"] == "required.bin" and x["match"] == "relative" for x in data["files"])
assert any(x["destination"] == "saturn-required.bin" and x["consumers"] == ["libretro:beetle_saturn"] for x in data["files"])
assert any(x["destination"] == "Machines/MSX/config.ini" and x["match"] == "archive" for x in data["files"])
PY
python3 - "$ROOT_DIR/scripts/prepare-pixel2-bios.py" <<'PY'
import importlib.util, pathlib, sys
spec = importlib.util.spec_from_file_location("pixel2_bios", sys.argv[1])
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assert ("freechaf", "sl90025.bin") in module.OPTIONAL_FIRMWARE_OVERRIDES
assert [name for name, _ in module.CHANNEL_F_COMBINED_HALVES] == [
    "sl31253.bin", "sl31254.bin"
]
PY
printf 'pixel2_bios_prepare=result-ok\n'
