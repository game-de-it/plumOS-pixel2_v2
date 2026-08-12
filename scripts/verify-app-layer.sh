#!/usr/bin/env bash
set -euo pipefail

ROOT="${1:-}"
[ -d "$ROOT" ] || { printf 'usage: %s APP_LAYER_ROOT\n' "$0" >&2; exit 2; }
for path in \
    manifest.json checksums.sha256 VERSION COMPAT_VENDOR RUNTIME_ABI \
    bin/plumos-frontend-pixel2 bin/plumos-library-scan bin/plumos-text-ui \
    bin/plumos-retroarch-launch bin/plumos-ensure-udev-input-db \
    bin/plumos-picoarch-launch bin/plumos-picoarch-stop \
    bin/plumos-standalone-launch bin/plumos-standalone-stop \
    bin/retroarch cores/quicknes_libretro.so \
    picoarch/bin/picoarch picoarch/lib/libSDL-1.2.so.0 \
    config/standalone/picoarch.env config/standalone/pixel2.env \
    bin/plumos-safe-shutdown bin/plumos-run-with-input-map \
    bin/plumos-hardware-keys bin/plumos-hardware-keys-service \
    bin/plumos-display-control bin/plumos-volume-control \
    bin/plumos-network-control bin/plumos-network-services \
    bin/plumos-audio-output lib/alsa-lib/libasound_module_pcm_plumos_hotplug.so \
    bin/plumos-python-pixel2 bin/plumos-pyxel-pixel2-launch bin/plumos-pyxel-setup \
    apps/python/bin/python3.11 apps/pyxel/site/pyxel/__init__.py \
    emulator/lib/libpthread.so.0 \
    factory-defaults/alsa/alsa.conf \
    config/frontend/systems.json factory-defaults/retroarch/retroarch.cfg \
    factory-defaults/retroarch/autoconfig/udev/pixel2_joypad.cfg \
    config/system/input-map.env config/system/input-map.json \
    components/frontend/manifest.json components/retroarch/manifest.json \
    components/picoarch/manifest.json components/standalone/manifest.json \
    components/audio-router/manifest.json \
    components/pyxel/manifest.json \
    components/libretro-cores/manifest.json; do
    [ -f "$ROOT/$path" ] || { printf 'error: app-layer file missing: %s\n' "$path" >&2; exit 1; }
done
(cd "$ROOT" && sha256sum -c checksums.sha256 >/dev/null)
(cd "$ROOT" && sha256sum -c components/frontend/checksums.sha256 >/dev/null)
(cd "$ROOT" && sha256sum -c components/retroarch/checksums.sha256 >/dev/null)
(cd "$ROOT" && sha256sum -c components/picoarch/checksums.sha256 >/dev/null)
(cd "$ROOT" && sha256sum -c components/standalone/checksums.sha256 >/dev/null)
(cd "$ROOT" && sha256sum -c components/audio-router/checksums.sha256 >/dev/null)
(cd "$ROOT" && sha256sum -c components/pyxel/checksums.sha256 >/dev/null)
(cd "$ROOT" && sha256sum -c components/libretro-cores/checksums.sha256 >/dev/null)
grep -q '"device": "pixel2"' "$ROOT/manifest.json"
grep -q '"complete": true' "$ROOT/manifest.json"
grep -q '"retroarch:quicknes"' "$ROOT/config/frontend/systems.json"
grep -q '^input_device = "pixel2_joypad"$' \
    "$ROOT/factory-defaults/retroarch/autoconfig/udev/pixel2_joypad.cfg"
grep -q '^input_joypad_driver = "udev"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^audio_device = "plumos_output"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^video_rotation = "3"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^video_force_aspect = "true"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^aspect_ratio_index = "0"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^audio_latency = "96"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^video_refresh_rate = "60.000000"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^video_threaded = "true"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^vrr_runloop_enable = "false"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^input_player1_analog_dpad_mode = "1"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg" && exit 1
grep -q '^input_player1_analog_dpad_mode = "0"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^input_player1_down_btn = "11"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '^config_save_on_exit = "false"$' \
    "$ROOT/factory-defaults/retroarch/retroarch.cfg"
grep -q '"device": "pixel2"' "$ROOT/config/frontend/menus.json"
grep -q 'ID_INPUT_JOYSTICK=1' "$ROOT/bin/plumos-ensure-udev-input-db"
grep -q 'plumos-audio-output' "$ROOT/bin/plumos-retroarch-launch"
grep -q 'plumos-audio-output' "$ROOT/bin/plumos-picoarch-launch"
grep -q 'plumos-audio-output' "$ROOT/bin/plumos-standalone-launch"
grep -q 'ALSA_PLUGIN_DIR' "$ROOT/bin/plumos-retroarch-launch"
grep -q 'ALSA_PLUGIN_DIR' "$ROOT/bin/plumos-picoarch-launch"
grep -q 'ALSA_PLUGIN_DIR' "$ROOT/bin/plumos-standalone-launch"
grep -q 'pcm.plumos_output' "$ROOT/bin/plumos-audio-output"
grep -q '"component": "audio-router"' "$ROOT/components/audio-router/manifest.json"
grep -q '"component": "picoarch"' "$ROOT/components/picoarch/manifest.json"
grep -q '"component": "standalone"' "$ROOT/components/standalone/manifest.json"
grep -q '"component": "pyxel"' "$ROOT/components/pyxel/manifest.json"
grep -q '"device": "pixel2"' "$ROOT/components/picoarch/manifest.json"
grep -q '"device": "pixel2"' "$ROOT/components/standalone/manifest.json"
grep -q '"device": "pixel2"' "$ROOT/components/pyxel/manifest.json"
grep -q '"id": "pyxel"' "$ROOT/config/frontend/systems.json"
if jq -e '.emulators[] | select(.id == "pcsx_rearmed" and .status == "built")' \
    "$ROOT/components/standalone/manifest.json" >/dev/null; then
    for path in \
        standalone/pcsx_rearmed/bin/pcsx \
        standalone/pcsx_rearmed/lib/libSDL-1.2.so.0 \
        standalone/pcsx_rearmed/build-manifest.json \
        factory-defaults/standalone/pcsx_rearmed/pcsx.cfg; do
        [ -f "$ROOT/$path" ] || {
            printf 'error: built PCSX-ReARMed file missing: %s\n' "$path" >&2
            exit 1
        }
    done
    grep -q '^Gpu3 = builtin_gpu$' \
        "$ROOT/factory-defaults/standalone/pcsx_rearmed/pcsx.cfg"
    grep -q '^thread_rendering = 1$' \
        "$ROOT/factory-defaults/standalone/pcsx_rearmed/pcsx.cfg"
    grep -q '"renderer": "builtin-neon-threaded-pixel2-fbdev-ccw"' \
        "$ROOT/standalone/pcsx_rearmed/build-manifest.json"
fi
python3 - "$ROOT/config/frontend/systems.json" <<'PY'
import json
import sys
from pathlib import Path

systems = json.loads(Path(sys.argv[1]).read_text())["systems"]
pyxel = next((system for system in systems if system.get("id") == "pyxel"), None)
if not pyxel:
    raise SystemExit("pyxel system missing")
if pyxel.get("enabled") is False:
    raise SystemExit("pyxel system disabled")
if pyxel.get("default_launch_profile") != "pyxel:pixel2":
    raise SystemExit("pyxel default launch profile is not pyxel:pixel2")
if "pyxel:pixel2" not in pyxel.get("launch_profiles", []):
    raise SystemExit("pyxel:pixel2 launch profile missing")
PY
grep -q '^PLUMOS_INPUT_AB_LAYOUT=east-confirm$' "$ROOT/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_A_CODE=305$' "$ROOT/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_B_CODE=304$' "$ROOT/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_A_UDEV_BTN=1$' "$ROOT/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_B_UDEV_BTN=0$' "$ROOT/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_DOWN_UDEV_AXIS=+1$' "$ROOT/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_DOWN_UDEV_BTN=11$' "$ROOT/config/system/input-map.env"
grep -q '^PLUMOS_INPUT_ANALOG_DPAD_MODE=none$' "$ROOT/config/system/input-map.env"
grep -q '"ab_layout": "east-confirm"' "$ROOT/config/system/input-map.json"
grep -q '"udev": {' "$ROOT/config/system/input-map.json"
grep -q '"action": "internal:system-settings"' \
    "$ROOT/config/frontend/menus.json"
grep -q '"action": "internal:network-settings"' \
    "$ROOT/config/frontend/menus.json"
grep -q '"action": "menu:apps"' \
    "$ROOT/config/frontend/menus.json"
if grep -Eq 'plumos-(nextcommander|music-player|retroarch-menu|portmaster)' \
    "$ROOT/config/frontend/apps.json"; then
    printf 'error: unavailable application exposed by Pixel2 frontend\n' >&2
    exit 1
fi
foreign_models='M[F]|V9[0]S|M[M]F|A3[0]|Miy[o]o'
if find "$ROOT/config" "$ROOT/share" "$ROOT/licenses" -type f \
    \( -name '*.json' -o -name '*.lang' -o -name '*.manifest' \
       -o -name '*.txt' -o -name 'LICENSE' \) -exec \
    grep -EIil "(^|[^[:alnum:]_])($foreign_models)([^[:alnum:]_]|$)" {} + | \
    grep -q .; then
    printf 'error: foreign device identity in Pixel2 user-facing files\n' >&2
    exit 1
fi
if strings "$ROOT/bin/plumos-frontend-pixel2" "$ROOT/bin/plumos-text-ui" | \
    grep -Eiq "(^|[^[:alnum:]_])($foreign_models)([^[:alnum:]_]|$)"; then
    printf 'error: foreign device identity in Pixel2 frontend binaries\n' >&2
    exit 1
fi
if strings "$ROOT/bin/plumos-hardware-keys" | \
    grep -Eiq "(^|[^[:alnum:]_])($foreign_models)([^[:alnum:]_]|$)"; then
    printf 'error: foreign device identity in Pixel2 hardware key daemon\n' >&2
    exit 1
fi
rom_like_content="$(
    find "$ROOT" -type f \( -iname '*.nes' -o -iname '*.gb' -o -iname '*.gba' \
        -o -iname '*.sfc' -o -iname '*.smc' -o -iname '*.bin' -o -iname '*.cue' \) |
    while IFS= read -r path; do
        rel=${path#"$ROOT"/}
        if [ "$rel" = "standalone/ppsspp/assets/shaders/smiley_16x16_rgba.bin" ]; then
            continue
        fi
        printf '%s\n' "$rel"
    done |
    head -n 1
)"
if [ -n "$rom_like_content" ]; then
    printf 'error: ROM or BIOS-like content in app layer\n' >&2
    printf 'error: first match: %s\n' "$rom_like_content" >&2
    exit 1
fi
if grep -R -a -E -i -n 'rock[n]ix|emuel[e]c|batocer[a]|knull[i]|stock[o]s' \
    "$ROOT" >/dev/null; then
    printf 'error: foreign distribution identity in app layer\n' >&2
    exit 1
fi
file "$ROOT/bin/retroarch" "$ROOT/cores/quicknes_libretro.so" |
    grep -q 'ARM aarch64'
python3 - "$ROOT" <<'PY'
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
systems = json.loads((root / "config/frontend/systems.json").read_text())["systems"]
manifest = json.loads((root / "components/libretro-cores/manifest.json").read_text())
standalone_manifest = json.loads((root / "components/standalone/manifest.json").read_text())
cores = {entry["id"] for entry in manifest["cores"]}
standalone_ids = {entry["id"] for entry in standalone_manifest.get("emulators", [])}
aliases = {
    "beetle_saturn": "mednafen_saturn",
    "mednafen_saturn": "beetle_saturn",
    "beetle_lynx": "mednafen_lynx",
    "beetle_ngp": "mednafen_ngp",
    "beetle_pce_fast": "mednafen_pce_fast",
    "beetle_supergrafx": "mednafen_supergrafx",
    "beetle_vb": "mednafen_vb",
    "beetle_wswan": "mednafen_wswan",
    "dosbox_pure_0.9.7": "dosbox_pure",
    "km_puae_xtreme_amped": "puae",
    "uae4arm": "puae2021",
}
missing = []
for system in systems:
    if system.get("enabled") is False:
        continue
    for profile in system.get("launch_profiles", []):
        if not profile.startswith("retroarch:"):
            if profile.startswith("picoarch:"):
                core = profile.split(":", 1)[1]
                if not (root / "bin/plumos-picoarch-launch").exists():
                    missing.append(profile)
                    continue
                if (root / "picoarch/cores" / f"{core}_libretro.so").exists():
                    continue
                if (root / "cores" / f"{core}_libretro.so").exists():
                    continue
                missing.append(profile)
            elif profile.startswith("standalone:"):
                emulator = profile.split(":", 1)[1]
                if not (root / "bin/plumos-standalone-launch").exists():
                    missing.append(profile)
                elif emulator not in standalone_ids:
                    missing.append(profile)
            elif profile.startswith("pyxel:"):
                pyxel_profile = profile.split(":", 1)[1]
                if pyxel_profile != "pixel2":
                    missing.append(profile)
                elif not (root / "bin/plumos-pyxel-pixel2-launch").exists():
                    missing.append(profile)
                elif not (root / "apps/pyxel/site/pyxel/__init__.py").exists():
                    missing.append(profile)
                elif not (root / "apps/python/bin/python3.11").exists():
                    missing.append(profile)
            else:
                missing.append(profile)
            continue
        core = profile.split(":", 1)[1]
        binary = root / "cores" / f"{core}_libretro.so"
        if core in cores and binary.exists():
            continue
        alias = aliases.get(core)
        if alias and alias in cores and (root / "cores" / f"{core}_libretro.so").exists():
            continue
        missing.append(profile)
if missing:
    raise SystemExit("missing launch profile runtimes: " + ", ".join(missing))
PY
if [ "$(uname -m)" = aarch64 ]; then
    LD_LIBRARY_PATH="$ROOT/emulator/lib:$ROOT/frontend/lib" \
        "$ROOT/bin/retroarch" --version | grep -q '^Version: 1\.22\.2'
    "$ROOT/bin/plumos-text-ui" --help >/dev/null

    runtime_tmp=$(mktemp -d /tmp/plumos-pixel2-app-verify.XXXXXX)
    trap 'rm -rf "$runtime_tmp"' EXIT
    mkdir -p "$runtime_tmp/app/state" "$runtime_tmp/roms/nes" "$runtime_tmp/roms/pyxel"
    ln -s "$ROOT/bin" "$runtime_tmp/app/bin"
    ln -s "$ROOT/apps" "$runtime_tmp/app/apps"
    ln -s "$ROOT/cores" "$runtime_tmp/app/cores"
    ln -s "$ROOT/config" "$runtime_tmp/app/config"
    ln -s "$ROOT/lib" "$runtime_tmp/app/lib"
    ln -s "$ROOT/share" "$runtime_tmp/app/share"
    PLUMOS_FRONTEND_MODE=manual \
        PLUMOS_ROOT="$runtime_tmp/app" \
        PLUMOS_SDCARD_ROOT="$runtime_tmp" \
        "$ROOT/bin/plumos-frontend-diagnostics" >/dev/null
    PLUMOS_ROOT="$runtime_tmp/app" \
        "$ROOT/bin/plumos-python-pixel2" \
        -c 'import pyxel, pygame, numpy, PIL' >/dev/null
    printf 'NES-test-fixture' >"$runtime_tmp/roms/nes/test.nes"
    printf 'print("Pyxel test fixture")\n' >"$runtime_tmp/roms/pyxel/test.pyxapp"
    PLUMOS_ROOT="$runtime_tmp/app" \
        PLUMOS_SDCARD_ROOT="$runtime_tmp" \
        PLUMOS_ROM_ROOT="$runtime_tmp/roms" \
        "$ROOT/bin/plumos-library-scan" >/dev/null
    PLUMOS_ROOT="$runtime_tmp/app" \
        PLUMOS_SDCARD_ROOT="$runtime_tmp" \
        PLUMOS_ROM_ROOT="$runtime_tmp/roms" \
        "$ROOT/bin/plumos-text-ui" launch nes nes/test.nes --no-scan \
        >"$runtime_tmp/launch-plan.txt"
    grep -q '^launch_profile: retroarch:quicknes$' "$runtime_tmp/launch-plan.txt"
    grep -q '^can_execute: yes$' "$runtime_tmp/launch-plan.txt"
    PLUMOS_ROOT="$runtime_tmp/app" \
        PLUMOS_SDCARD_ROOT="$runtime_tmp" \
        PLUMOS_ROM_ROOT="$runtime_tmp/roms" \
        "$ROOT/bin/plumos-text-ui" launch pyxel pyxel/test.pyxapp --no-scan \
        >"$runtime_tmp/pyxel-launch-plan.txt"
    grep -q '^launch_profile: pyxel:pixel2$' "$runtime_tmp/pyxel-launch-plan.txt"
    grep -q '^can_execute: yes$' "$runtime_tmp/pyxel-launch-plan.txt"
    rm -rf "$runtime_tmp"
    trap - EXIT
fi
printf 'app_layer_verify=result-ok root=%s\n' "$ROOT"
