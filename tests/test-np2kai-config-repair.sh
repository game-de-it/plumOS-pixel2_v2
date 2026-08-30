#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
REPAIR="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-np2kai-config-repair"
TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

invalid="$TMP_ROOT/invalid.cfg"
recovery="$TMP_ROOT/recovery"
cat >"$invalid" <<'EOF'
[NekoProjectIIkai]
pc_model = VX
MEMswtch = 00 00 00 00 00 00 00 00 
custom = keep
EOF

"$REPAIR" "$invalid" "$recovery" >"$TMP_ROOT/repair.log"
grep -q '^np2kai_config=result-repaired field=MEMswtch reason=all-zero ' \
    "$TMP_ROOT/repair.log"
grep -q '^MEMswtch = 48 05 04 08 01 00 00 6e $' "$invalid"
grep -q '^custom = keep$' "$invalid"
grep -q '^MEMswtch = 00 00 00 00 00 00 00 00 $' \
    "$recovery/np2kai.cfg.before-invalid-memsw-repair"

backup_sha="$(sha256sum "$recovery/np2kai.cfg.before-invalid-memsw-repair" | awk '{print $1}')"
"$REPAIR" "$invalid" "$recovery" >"$TMP_ROOT/valid-after.log"
grep -q '^np2kai_config=result-valid field=MEMswtch$' "$TMP_ROOT/valid-after.log"
test "$backup_sha" = \
    "$(sha256sum "$recovery/np2kai.cfg.before-invalid-memsw-repair" | awk '{print $1}')"

custom="$TMP_ROOT/custom.cfg"
cat >"$custom" <<'EOF'
[NekoProjectIIkai]
MEMswtch = 48 05 04 00 01 00 00 6e 
EOF
"$REPAIR" "$custom" "$TMP_ROOT/custom-recovery" >"$TMP_ROOT/custom.log"
grep -q '^np2kai_config=result-valid field=MEMswtch$' "$TMP_ROOT/custom.log"
grep -q '^MEMswtch = 48 05 04 00 01 00 00 6e $' "$custom"
test ! -e "$TMP_ROOT/custom-recovery"

"$REPAIR" "$TMP_ROOT/missing.cfg" "$TMP_ROOT/missing-recovery" \
    >"$TMP_ROOT/missing.log"
grep -q '^np2kai_config=result-skip reason=config-missing$' "$TMP_ROOT/missing.log"

printf 'test-np2kai-config-repair: ok\n'
