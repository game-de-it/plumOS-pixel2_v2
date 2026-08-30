#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
REPAIR="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-np2kai-config-repair"
TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

invalid="$TMP_ROOT/invalid.cfg"
recovery="$TMP_ROOT/recovery"
core_options="$TMP_ROOT/Neko Project II kai.opt"
cat >"$invalid" <<'EOF'
[NekoProjectIIkai]
pc_model = VX
MEMswtch = 00 00 00 00 00 00 00 00 
custom = keep
EOF
cat >"$core_options" <<'EOF'
np2kai_joymode = "OFF"
np2kai_joynp2menu = "L2"
EOF

"$REPAIR" "$invalid" "$recovery" "$core_options" "Arrows 3button" \
    >"$TMP_ROOT/repair.log"
grep -q '^np2kai_config=result-repaired field=MEMswtch reason=all-zero ' \
    "$TMP_ROOT/repair.log"
grep -q '^np2kai_config=result-repaired field=joymode reason=stale-off value=Arrows 3button ' \
    "$TMP_ROOT/repair.log"
grep -q '^MEMswtch = 48 05 04 08 01 00 00 6e $' "$invalid"
grep -q '^custom = keep$' "$invalid"
grep -q '^np2kai_joymode = "Arrows 3button"$' "$core_options"
grep -q '^np2kai_joynp2menu = "L2"$' "$core_options"
grep -q '^MEMswtch = 00 00 00 00 00 00 00 00 $' \
    "$recovery/np2kai.cfg.before-invalid-memsw-repair"
grep -q '^np2kai_joymode = "OFF"$' \
    "$recovery/Neko-Project-II-kai.opt.before-joymode-migration"
test -e "$recovery/np2kai-joymode-v1-migrated"

backup_sha="$(sha256sum "$recovery/np2kai.cfg.before-invalid-memsw-repair" | awk '{print $1}')"
"$REPAIR" "$invalid" "$recovery" "$core_options" "Arrows 3button" \
    >"$TMP_ROOT/valid-after.log"
grep -q '^np2kai_config=result-valid field=MEMswtch$' "$TMP_ROOT/valid-after.log"
grep -q '^np2kai_config=result-valid field=joymode reason=migration-complete$' \
    "$TMP_ROOT/valid-after.log"
test "$backup_sha" = \
    "$(sha256sum "$recovery/np2kai.cfg.before-invalid-memsw-repair" | awk '{print $1}')"

custom="$TMP_ROOT/custom.cfg"
cat >"$custom" <<'EOF'
[NekoProjectIIkai]
MEMswtch = 48 05 04 00 01 00 00 6e 
EOF
custom_options="$TMP_ROOT/custom.opt"
printf '%s\n' 'np2kai_joymode = "Mouse"' >"$custom_options"
"$REPAIR" "$custom" "$TMP_ROOT/custom-recovery" "$custom_options" \
    "Arrows 3button" >"$TMP_ROOT/custom.log"
grep -q '^np2kai_config=result-valid field=MEMswtch$' "$TMP_ROOT/custom.log"
grep -q '^np2kai_config=result-valid field=joymode reason=user-selection-preserved$' \
    "$TMP_ROOT/custom.log"
grep -q '^MEMswtch = 48 05 04 00 01 00 00 6e $' "$custom"
grep -q '^np2kai_joymode = "Mouse"$' "$custom_options"
test -e "$TMP_ROOT/custom-recovery/np2kai-joymode-v1-migrated"

# Once migrated, a deliberate user choice of OFF must not be replaced again.
printf '%s\n' 'np2kai_joymode = "OFF"' >"$custom_options"
"$REPAIR" "$custom" "$TMP_ROOT/custom-recovery" "$custom_options" \
    "Arrows 3button" >"$TMP_ROOT/custom-after.log"
grep -q '^np2kai_joymode = "OFF"$' "$custom_options"
grep -q '^np2kai_config=result-valid field=joymode reason=migration-complete$' \
    "$TMP_ROOT/custom-after.log"

"$REPAIR" "$TMP_ROOT/missing.cfg" "$TMP_ROOT/missing-recovery" \
    "$TMP_ROOT/missing.opt" "Arrows 3button" >"$TMP_ROOT/missing.log"
grep -q '^np2kai_config=result-skip reason=config-missing$' "$TMP_ROOT/missing.log"
grep -q '^np2kai_config=result-skip field=joymode reason=core-options-missing$' \
    "$TMP_ROOT/missing.log"

printf 'test-np2kai-config-repair: ok\n'
