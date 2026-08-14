#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BUILDER="$ROOT_DIR/scripts/build-pixel2-update-package.py"
UPDATER="$ROOT_DIR/scripts/plumos-system-update.py"
SYSTEM_ABI=plumos-pixel2-v1
RUNTIME_ABI=plumos-pixel2-app-layer-v1
VENDOR=pixel2-rockchip-r1

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

assert_file_value() {
    path=$1
    expected=$2
    [ -f "$path" ] || fail "missing file: $path"
    actual=$(tr -d '\r\n' <"$path")
    [ "$actual" = "$expected" ] || \
        fail "unexpected value in $path: expected=$expected actual=$actual"
}

write_runtime_fixture() {
    root=$1
    version=$2
    tool_value=$3
    mkdir -p "$root/bin" "$root/config/frontend" "$root/config/system" \
        "$root/fonts" "$root/network/bin" "$root/ssh/libexec"
    printf '%s\n' "$version" >"$root/VERSION"
    printf '%s\n' "$VENDOR" >"$root/COMPAT_VENDOR"
    printf '%s\n' "$RUNTIME_ABI" >"$root/RUNTIME_ABI"
    printf '%s\n' "$tool_value" >"$root/bin/test-tool"
    printf '%s\n' '{"device":"pixel2"}' >"$root/manifest.json"
    printf '%s\n' '{"device":"pixel2","contract":"complete"}' \
        >"$root/config/frontend/feature-contract.json"
    printf '%s\n' 'managed-input-map' >"$root/config/system/input-map.env"
    printf '%s\n' 'font-fixture' >"$root/fonts/default.otf"
    printf '%s\n' 'network-fixture' >"$root/network/bin/daemon"
    printf '%s\n' 'ssh-fixture' >"$root/ssh/libexec/server"
    python3 - "$root" <<'PY'
from hashlib import sha256
from pathlib import Path
import sys

root = Path(sys.argv[1])
with (root / "checksums.sha256").open("w", encoding="ascii") as output:
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        if path.name == "checksums.sha256":
            continue
        digest = sha256(path.read_bytes()).hexdigest()
        output.write(f"{digest}  {path.relative_to(root).as_posix()}\n")
PY
}

run_updater() {
    PLUMOS_ROOT="$runtime_root" \
    PLUMOS_USERDATA_ROOT="$user_root" \
    PLUMOS_BOOT_ROOT="$boot_root" \
    PLUMOS_UPDATE_PUBLIC_KEY="$public_key" \
    PLUMOS_UPDATE_LOCK_FILE="$temp_root/update.lock" \
    PLUMOS_UPDATE_TMP="$temp_root" \
    PLUMOS_UPDATE_PROGRESS=0 \
    PLUMOS_UPDATE_BOOT_REMOUNT=0 \
    PLUMOS_SYSTEM_ABI_FILE="$system_abi_file" \
    PLUMOS_SYSTEM_VERSION_FILE="$system_version_file" \
    python3 "$UPDATER" "$@"
}

temp_root=$(mktemp -d "${TMPDIR:-/tmp}/plumos-pixel2-update.XXXXXX")
trap 'rm -rf "$temp_root"' EXIT
private_key="$temp_root/update-private.pem"
public_key="$temp_root/update-public.pem"
openssl genpkey -algorithm Ed25519 -out "$private_key" >/dev/null 2>&1
openssl pkey -in "$private_key" -pubout -out "$public_key" >/dev/null 2>&1

runtime_root="$temp_root/runtime-installed"
user_root="$temp_root/user"
boot_root="$temp_root/boot"
system_abi_file="$temp_root/plumos-system-abi"
system_version_file="$temp_root/plumos-system-version"
base_100="$temp_root/runtime-1.0.0"
runtime_101="$temp_root/runtime-1.0.1"
runtime_102="$temp_root/runtime-1.0.2"
dist="$temp_root/dist"
mkdir -p "$user_root/updates" "$boot_root/system-slots" "$dist"
printf '%s\n' "$SYSTEM_ABI" >"$system_abi_file"
printf '%s\n' '1.0.0' >"$system_version_file"
write_runtime_fixture "$base_100" 1.0.0 old
write_runtime_fixture "$runtime_101" 1.0.1 new
write_runtime_fixture "$runtime_102" 1.0.2 newer
cp -a "$base_100" "$runtime_root"
mkdir -p "$runtime_root/config/user"
printf '%s\n' 'preserve-me' >"$runtime_root/config/user/settings.ini"
run_updater verify-runtime >/dev/null
printf '%s\n' corrupt >"$runtime_root/bin/test-tool"
if run_updater verify-runtime >/dev/null 2>&1; then
    fail 'Explicit Runtime verification accepted corrupted content'
fi
printf '%s\n' old >"$runtime_root/bin/test-tool"
run_updater verify-runtime >/dev/null

# Runtime packages must fail during construction, rather than only on-device,
# when a symlink uses a parent traversal rejected by the updater contract.
ln -s ../network/bin/busybox "$runtime_102/bin/invalid-parent-link"
if python3 "$BUILDER" --type runtime --input "$runtime_102" \
    --base-version 1.0.1 --version 1.0.2 \
    --signing-key "$private_key" --output-dir "$dist" >/dev/null 2>&1; then
    fail 'Runtime package builder accepted an updater-incompatible symlink'
fi
rm -f "$runtime_102/bin/invalid-parent-link"

full_dist="$temp_root/full-dist"
mkdir -p "$full_dist"
python3 "$BUILDER" --type runtime --input "$runtime_102" \
    --version 1.0.2 --signing-key "$private_key" --output-dir "$full_dist" \
    >/dev/null
python3 - "$full_dist/plumos-pixel2-runtime-1.0.2.tar.gz" <<'PY'
import json
from pathlib import Path
import sys
import tarfile

with tarfile.open(Path(sys.argv[1]), "r:gz") as archive:
    manifest = json.load(archive.extractfile("META/manifest.json"))
paths = {entry["path"] for entry in manifest["files"]}
required = {
    "config/frontend/feature-contract.json",
    "fonts/default.otf",
    "network/bin/daemon",
    "ssh/libexec/server",
}
assert required <= paths, sorted(required - paths)
PY
run_updater inspect "$full_dist/plumos-pixel2-runtime-1.0.2.tar.gz" >/dev/null

base_checksums="$temp_root/runtime-1.0.0-checksums.sha256"
python3 - "$base_100" "$base_checksums" <<'PY'
from hashlib import sha256
from pathlib import Path
import sys

root = Path(sys.argv[1])
with Path(sys.argv[2]).open("w", encoding="ascii") as output:
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        output.write(f"{sha256(path.read_bytes()).hexdigest()}  {path.relative_to(root).as_posix()}\n")
PY
python3 "$BUILDER" --type runtime --input "$runtime_101" \
    --base-checksums "$base_checksums" --base-version 1.0.0 --version 1.0.1 \
    --signing-key "$private_key" --output-dir "$dist" >/dev/null
python3 - "$dist/plumos-pixel2-runtime-1.0.1.tar.gz" <<'PY'
import json
from pathlib import Path
import sys
import tarfile

with tarfile.open(Path(sys.argv[1]), "r:gz") as archive:
    manifest = json.load(archive.extractfile("META/manifest.json"))
assert manifest["full_payload"] is False
assert {entry["path"] for entry in manifest["files"]} == {
    "VERSION", "bin/test-tool", "checksums.sha256"
}
PY
cp "$dist/plumos-pixel2-runtime-1.0.1.tar.gz" "$user_root/updates/"
run_updater request-latest >/dev/null
run_updater apply-pending
assert_file_value "$runtime_root/VERSION" 1.0.1
assert_file_value "$runtime_root/bin/test-tool" new
assert_file_value "$runtime_root/config/user/settings.ini" preserve-me
[ -f "$runtime_root/update-state/runtime-pending.json" ] || \
    fail 'Runtime update did not enter pending-health state'
run_updater mark-healthy
[ ! -e "$runtime_root/update-state/runtime-pending.json" ] || \
    fail 'Runtime pending state survived health promotion'
grep -q '"status": "healthy"' \
    "$runtime_root/update-state/runtime-transaction.json" || \
    fail 'Runtime journal was not promoted healthy'
assert_file_value \
    "$runtime_root/backups/update-previous/files/bin/test-tool" old

python3 "$BUILDER" --type runtime --input "$runtime_102" \
    --base-dir "$runtime_101" --base-version 1.0.1 --version 1.0.2 \
    --signing-key "$private_key" --output-dir "$dist" >/dev/null
cp "$dist/plumos-pixel2-runtime-1.0.2.tar.gz" "$user_root/updates/"
run_updater request "$user_root/updates/plumos-pixel2-runtime-1.0.2.tar.gz" \
    >/dev/null
run_updater apply-pending
assert_file_value "$runtime_root/VERSION" 1.0.2
assert_file_value "$runtime_root/bin/test-tool" newer
run_updater apply-pending
assert_file_value "$runtime_root/VERSION" 1.0.1
assert_file_value "$runtime_root/bin/test-tool" new
grep -q '"status": "rolled_back"' \
    "$runtime_root/update-state/runtime-transaction.json" || \
    fail 'Unhealthy Runtime update was not rolled back'
[ "$(find "$runtime_root/backups" -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' ')" = 1 ] || \
    fail 'Runtime updater retained more than one rollback generation'

system_payload="$temp_root/system.squashfs"
printf '%s\n' 'Pixel2 System fixture' >"$system_payload"
PLUMOS_UPDATE_SKIP_EMBEDDED_CHECK=1 python3 "$BUILDER" \
    --type system --input "$system_payload" --base-version 1.0.0 \
    --version 1.1.0 --signing-key "$private_key" --output-dir "$dist" \
    >/dev/null
cp "$dist/plumos-pixel2-system-1.1.0.tar.gz" "$user_root/updates/"

PLUMOS_UPDATE_SKIP_EMBEDDED_CHECK=1 python3 "$BUILDER" \
    --type system --input "$system_payload" --base-version 1.0.0 \
    --version 1.1.1 --unsigned --output-dir "$dist" >/dev/null
if run_updater inspect "$dist/plumos-pixel2-system-1.1.1.tar.gz" \
    >/dev/null 2>&1; then
    fail 'Unsigned System package was accepted'
fi

printf '%s\n' a >"$runtime_root/update-state/system-active"
printf '%s\n' a >"$runtime_root/update-state/system-booted"
run_updater request "$user_root/updates/plumos-pixel2-system-1.1.0.tar.gz" \
    >/dev/null
set +e
run_updater apply-pending
system_rc=$?
set -e
[ "$system_rc" -eq 20 ] || \
    fail "System update did not request reboot: rc=$system_rc"
assert_file_value "$runtime_root/update-state/system-active" a
assert_file_value "$runtime_root/update-state/system-pending" b
cmp -s "$system_payload" "$boot_root/system-slots/system-b.squashfs" || \
    fail 'Inactive System slot readback does not match package payload'
if run_updater mark-healthy >/dev/null 2>&1; then
    fail 'System slot was promoted before the pending slot booted'
fi
printf '%s\n' b >"$runtime_root/update-state/system-booted"
run_updater mark-healthy
assert_file_value "$runtime_root/update-state/system-active" b
[ ! -e "$runtime_root/update-state/system-pending" ] || \
    fail 'System pending marker survived health promotion'
grep -q '"result": "system_healthy"' \
    "$runtime_root/update-state/last-result.json" || \
    fail 'System healthy result was not recorded'

interrupted_root="$temp_root/runtime-interrupted"
runtime_root="$interrupted_root"
write_runtime_fixture "$runtime_root" 1.0.0 old
mkdir -p "$runtime_root/backups/update-previous/files/bin" \
    "$runtime_root/update-state"
mv "$runtime_root/bin/test-tool" \
    "$runtime_root/backups/update-previous/files/bin/test-tool"
printf '%s\n' partial >"$runtime_root/bin/test-tool"
printf '%s\n' partial-new >"$runtime_root/bin/new-tool"
printf '%s\n' \
    '{"status":"applying","operations":[{"path":"bin/test-tool","existed":true,"install_requested":true,"installed":false},{"path":"bin/new-tool","existed":false,"install_requested":true,"installed":false}]}' \
    >"$runtime_root/update-state/runtime-transaction.json"
run_updater apply-pending
assert_file_value "$runtime_root/bin/test-tool" old
[ ! -e "$runtime_root/bin/new-tool" ] || \
    fail 'Interrupted Runtime transaction left a new file installed'
grep -q '"result": "rolled_back"' \
    "$runtime_root/update-state/last-result.json" || \
    fail 'Interrupted Runtime rollback result was not recorded'

python3 - "$UPDATER" <<'PY'
import errno
import importlib.util
import sys
from unittest import mock

spec = importlib.util.spec_from_file_location("plumos_system_update", sys.argv[1])
assert spec is not None and spec.loader is not None
updater = importlib.util.module_from_spec(spec)
spec.loader.exec_module(updater)
with mock.patch.object(updater.os, "fsync", side_effect=OSError(0, "Error")), \
     mock.patch.object(updater.os, "sync") as sync:
    updater.fsync_file_descriptor(1)
    sync.assert_called_once_with()
with mock.patch.object(updater.os, "fsync", side_effect=OSError(errno.EIO, "I/O")), \
     mock.patch.object(updater.os, "sync") as sync:
    try:
        updater.fsync_file_descriptor(1)
    except OSError:
        pass
    else:
        raise AssertionError("non-vendor fsync error was swallowed")
    sync.assert_not_called()
PY

printf '%s\n' 'pixel2_update=result-ok'
