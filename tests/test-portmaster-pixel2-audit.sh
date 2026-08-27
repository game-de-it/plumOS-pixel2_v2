#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
AUDIT="$ROOT_DIR/package/portmaster-pixel2/plumos/apps/portmaster/adapter/plumos_portmaster_audit.py"
work="$(mktemp -d /tmp/plumos-portmaster-audit-test.XXXXXX)"
trap 'rm -rf "$work"' EXIT

python3 -m py_compile "$AUDIT"
mkdir -p "$work/port" "$work/libs" "$work/cache"
cat > "$work/port/Test.sh" <<'EOF'
#!/bin/bash
GAMEDIR=/$directory/ports/port
export LD_PRELOAD=/opt/vendor/libscale.so
./fixture
EOF

python3 - "$work/port/fixture" "$work/libs/libmissing.so.9" <<'PY'
import struct
import sys
from pathlib import Path

BASE = 0x400000


def elf(path: str, needed=(), soname=None, machine=183, interp=True):
    strings = bytearray(b"\0")
    offsets = {}
    for value in [*needed, *([soname] if soname else [])]:
        offsets[value] = len(strings)
        strings.extend(value.encode() + b"\0")
    phnum = 3 if interp else 2
    header = b"\x7fELF" + bytes([2, 1, 1, 0]) + bytes(8)
    header += struct.pack(
        "<HHIQQQIHHHHHH", 3, machine, 1, 0, 64, 0, 0, 64, 56, phnum, 0, 0, 0
    )
    interp_data = b"/lib/ld-linux-aarch64.so.1\0" if interp else b""
    dynamic = []
    dynamic.append((5, BASE + 0x280))
    dynamic.append((10, len(strings)))
    dynamic.extend((1, offsets[value]) for value in needed)
    if soname:
        dynamic.append((14, offsets[soname]))
    dynamic.append((0, 0))
    dynamic_data = b"".join(struct.pack("<qQ", *entry) for entry in dynamic)
    size = 0x280 + len(strings)
    phdrs = [struct.pack("<IIQQQQQQ", 1, 5, 0, BASE, 0, size, size, 0x1000)]
    if interp:
        phdrs.append(
            struct.pack("<IIQQQQQQ", 3, 4, 0x180, BASE + 0x180, 0, len(interp_data), len(interp_data), 1)
        )
    phdrs.append(
        struct.pack("<IIQQQQQQ", 2, 4, 0x200, BASE + 0x200, 0, len(dynamic_data), len(dynamic_data), 8)
    )
    payload = bytearray(size)
    payload[: len(header)] = header
    payload[64 : 64 + 56 * phnum] = b"".join(phdrs)
    payload[0x180 : 0x180 + len(interp_data)] = interp_data
    payload[0x200 : 0x200 + len(dynamic_data)] = dynamic_data
    payload[0x280 : 0x280 + len(strings)] = strings
    Path(path).write_bytes(payload)


elf(sys.argv[1], needed=("libmissing.so.9",))
elf(sys.argv[2], soname="libmissing.so.9", interp=False)
PY
chmod 0755 "$work/port/Test.sh" "$work/port/fixture"

python3 "$AUDIT" \
  --script "$work/port/Test.sh" \
  --ports-root "$work" \
  --cache-dir "$work/cache" \
  --output "$work/missing.json"
jq -e '.status == "blocked" and .errors == 1 and .cache == "miss"' \
  "$work/missing.json" >/dev/null
jq -e '.findings[] | select(.code == "missing_soname" and .detail == "libmissing.so.9")' \
  "$work/missing.json" >/dev/null
jq -e '.findings[] | select(.code == "environment_replaced" and (.detail | contains("LD_PRELOAD")))' \
  "$work/missing.json" >/dev/null
if python3 "$AUDIT" \
  --script "$work/port/Test.sh" \
  --ports-root "$work" \
  --cache-dir "$work/cache" \
  --enforce >/dev/null; then
  echo 'PortMaster audit enforcement accepted a missing referenced SONAME' >&2
  exit 1
fi

python3 "$AUDIT" \
  --script "$work/port/Test.sh" \
  --ports-root "$work" \
  --library-dir "$work/libs" \
  --no-cache \
  --output "$work/resolved.json"
jq -e '.status == "warning" and .errors == 0 and .warnings == 1' \
  "$work/resolved.json" >/dev/null

python3 "$AUDIT" \
  --script "$work/port/Test.sh" \
  --ports-root "$work" \
  --cache-dir "$work/cache" \
  --output "$work/cached.json"
jq -e '.cache == "hit"' "$work/cached.json" >/dev/null

printf 'portmaster_pixel2_audit=result-ok\n'
