#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-bios-test.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
mkdir -p \
    "$tmp/app/config/frontend" "$tmp/app/info" \
    "$tmp/rom/bios/Databases" "$tmp/rom/bios/Machines/Test" \
    "$tmp/out"
cat >"$tmp/app/config/frontend/systems.json" <<'EOF'
{"systems":[{"id":"test","enabled":true,"launch_profiles":["retroarch:test","retroarch:beetle_saturn","standalone:drastic"]}]}
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
printf 'required\n' >"$tmp/rom/bios/REQUIRED.BIN"
printf 'saturn\n' >"$tmp/rom/bios/saturn-required.bin"
printf 'database\n' >"$tmp/rom/bios/Databases/test.xml"
printf 'extra\n' >"$tmp/rom/bios/Databases/extra.xml"
printf 'machine\n' >"$tmp/rom/bios/Machines/Test/machine.rom"
printf 'arm7\n' >"$tmp/rom/bios/drastic_bios_arm7.bin"
printf 'arm9\n' >"$tmp/rom/bios/drastic_bios_arm9.bin"

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
PY
printf 'pixel2_bios_prepare=result-ok\n'
