#!/bin/sh
set -eu

root_dir="$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)"
scope=full
if [ "${1:-}" = --app-only ]; then
    scope=app
    shift
fi
app_layer="${1:-$root_dir/output/app-layer/pixel2/plumos}"
system_root="${2:-$root_dir/output/system-rootfs/pixel2/rootfs}"
failures=0

fail() {
    printf 'license-audit: FAIL: %s\n' "$*" >&2
    failures=$((failures + 1))
}

require_file() {
    base=$1
    relative=$2
    [ -s "$base/$relative" ] || fail "missing or empty: $relative"
}

require_glob() {
    base=$1
    pattern=$2
    label=$3
    find "$base" -type f -path "$base/$pattern" -print -quit 2>/dev/null |
        grep -q . || fail "missing $label: $pattern"
}

[ -d "$app_layer" ] || {
    printf 'license-audit: app-layer not found: %s\n' "$app_layer" >&2
    exit 1
}

for path in \
    licenses/plumOS-MIT.txt \
    licenses/NOTICE.txt \
    licenses/THIRD_PARTY_NOTICES.md \
    licenses/THIRD_PARTY_NOTICES.ja.md \
    licenses/RUNTIME_LICENSE_INDEX.tsv \
    licenses/pixel2-stock-vendor-runtime-NOTICE.txt \
    licenses/drastic-upstream-NOTICE.txt \
    licenses/nextcommander-upstream-NOTICE.txt \
    licenses/RetroArch-COPYING \
    licenses/picoarch-LICENSE \
    licenses/picoarch-sdl12-compat-LICENSE.txt \
    licenses/pcsx-rearmed-COPYING.txt \
    licenses/steward-fu-nds-LGPL-2.1 \
    licenses/drastic-upstream-release-readme.txt \
    licenses/ppsspp-LICENSE.txt \
    licenses/openbor-LICENSE.txt \
    licenses/mupen64plus-ui-console-LICENSES.txt \
    licenses/mupen64plus-core-LICENSES.txt \
    licenses/mupen64plus-audio-sdl-LICENSES.txt \
    licenses/mupen64plus-input-sdl-LICENSES.txt \
    licenses/mupen64plus-rsp-hle-LICENSES.txt \
    licenses/mupen64plus-video-rice-LICENSES.txt \
    licenses/plumos-audio-router-MIT.txt; do
    require_file "$app_layer" "$path"
done

components="frontend retroarch libretro-cores picoarch standalone audio-router pyxel nextcommander music-player network-services portmaster"
for component in $components; do
    component_manifest="components/$component/manifest.json"
    license_manifest="licenses/component-manifests/$component.json"
    require_file "$app_layer" "$component_manifest"
    require_file "$app_layer" "$license_manifest"
    if [ -s "$app_layer/$component_manifest" ] &&
       [ -s "$app_layer/$license_manifest" ] &&
       ! cmp -s "$app_layer/$component_manifest" "$app_layer/$license_manifest"; then
        fail "component license manifest drift: $component"
    fi
    awk -F '\t' -v id="$component" '
        $1 == id { found = 1 }
        END { exit found ? 0 : 1 }
    ' "$app_layer/licenses/RUNTIME_LICENSE_INDEX.tsv" ||
        fail "runtime license index missing component: $component"
done

core_manifest="$app_layer/components/libretro-cores/manifest.json"
if [ -s "$core_manifest" ]; then
    python3 - "$app_layer" "$core_manifest" <<'PY' || failures=$((failures + 1))
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
manifest = json.loads(Path(sys.argv[2]).read_text(encoding="utf-8"))
licenses = [path.name for path in (root / "licenses").iterdir() if path.is_file()]
missing = [
    core["id"]
    for core in manifest.get("cores", [])
    if not any(name.startswith(core["id"] + "-") for name in licenses)
]
if missing:
    print(
        "license-audit: FAIL: missing libretro core license material: "
        + ", ".join(missing),
        file=sys.stderr,
    )
    raise SystemExit(1)
PY
fi

require_glob "$app_layer" \
    'apps/portmaster/upstream/PortMaster/licenses/*' \
    'PortMaster upstream license material'
require_glob "$app_layer" \
    'apps/pyxel/site/*dist-info/*LICENSE*' \
    'Pyxel/Python package license metadata'
require_file "$app_layer" 'apps/python/lib/python3.11/LICENSE.txt'

for path in \
    licenses/openal-soft-LGPL-2.0-or-later.txt \
    licenses/ffmpeg-compat-LGPL-2.1-or-later.txt \
    licenses/libevdev-MIT.txt \
    licenses/flac-compat-Xiph-BSD.txt \
    licenses/libjpeg-compat-IJG.txt \
    licenses/readline-compat-GPL-3.0-or-later.txt; do
    require_file "$app_layer" "$path"
done

if [ "$scope" = full ]; then
    if [ ! -d "$system_root" ]; then
        fail "System rootfs output is missing: $system_root"
    else
        for path in \
            usr/share/licenses/plumOS-MIT.txt \
            usr/share/licenses/NOTICE.txt \
            usr/share/licenses/pixel2-stock-vendor-runtime-NOTICE.txt \
            usr/share/licenses/rtl8821cu/LICENSE \
            usr/share/licenses/debian/busybox-static-copyright \
            usr/share/licenses/debian/wpasupplicant-copyright \
            usr/share/licenses/debian/dropbear-bin-copyright \
            usr/share/licenses/debian/eject-copyright; do
            require_file "$system_root" "$path"
        done
    fi
fi

if [ "$failures" -ne 0 ]; then
    printf 'license-audit: failed=%s\n' "$failures" >&2
    exit 1
fi

license_count="$(find "$app_layer/licenses" -type f | wc -l | tr -d '[:space:]')"
core_count="$(python3 - "$core_manifest" <<'PY'
import json
import sys
print(len(json.load(open(sys.argv[1], encoding="utf-8")).get("cores", [])))
PY
)"
printf 'license-audit: PASS\n'
printf 'app_layer=%s\n' "$app_layer"
printf 'license_files=%s\n' "$license_count"
printf 'libretro_cores=%s\n' "$core_count"
printf 'scope=%s\n' "$scope"
