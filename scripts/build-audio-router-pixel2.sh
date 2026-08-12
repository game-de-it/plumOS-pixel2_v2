#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT_ROOT="${PLUMOS_PIXEL2_AUDIO_ROUTER_OUT:-${ROOT_DIR}/output/audio-router/pixel2}"
PLUMOS_DIR="${OUT_ROOT}/plumos"
PLUGIN_DIR="${PLUMOS_DIR}/lib/alsa-lib"
ARMHF_PLUGIN_DIR="${PLUMOS_DIR}/lib/alsa-lib-armhf"
COMPONENT_DIR="${PLUMOS_DIR}/components/audio-router"
DOC_DIR="${PLUMOS_DIR}/share/doc/audio-router"
LICENSE_DIR="${PLUMOS_DIR}/licenses"
SOURCE="${ROOT_DIR}/package/audio-router-pixel2/plumos_hotplug.c"
HELPER="${ROOT_DIR}/package/audio-router-pixel2/plumos/bin/plumos-audio-output"
LICENSE="${ROOT_DIR}/package/audio-router-pixel2/LICENSE"
CC="${CC:-gcc}"
ARMHF_CC="${PLUMOS_PIXEL2_ARMHF_CC:-arm-linux-gnueabihf-gcc}"
ARMHF_ALSA_LIB="${PLUMOS_PIXEL2_ARMHF_ALSA_LIB:-${ROOT_DIR}/output/standalone/pixel2/plumos/standalone/drastic/runtime/lib32/libasound.so.2}"
STRIP="${STRIP:-strip}"
ARMHF_STRIP="${PLUMOS_PIXEL2_ARMHF_STRIP:-arm-linux-gnueabihf-strip}"

rm -rf "$OUT_ROOT"
mkdir -p \
  "$PLUGIN_DIR" "$ARMHF_PLUGIN_DIR" "$COMPONENT_DIR" "$DOC_DIR" \
  "$LICENSE_DIR" "$PLUMOS_DIR/bin"
[ -r "$ARMHF_ALSA_LIB" ] || {
  printf 'error: ARMHF ALSA runtime is missing: %s\n' "$ARMHF_ALSA_LIB" >&2
  printf 'hint: build standalone with DraStic before building audio-router\n' >&2
  exit 1
}

"$CC" \
  -O3 \
  -DPIC \
  -fPIC \
  -Wall \
  -Wextra \
  -Werror \
  -shared \
  -Wl,-z,relro,-z,now \
  -o "$PLUGIN_DIR/libasound_module_pcm_plumos_hotplug.so" \
  "$SOURCE" \
  -lasound
"$STRIP" --strip-unneeded \
  "$PLUGIN_DIR/libasound_module_pcm_plumos_hotplug.so"

"$ARMHF_CC" \
  -O3 \
  -DPIC \
  -fPIC \
  -Wall \
  -Wextra \
  -Werror \
  -shared \
  -Wl,-z,relro,-z,now \
  -o "$ARMHF_PLUGIN_DIR/libasound_module_pcm_plumos_hotplug.so" \
  "$SOURCE" \
  -L"$(dirname "$ARMHF_ALSA_LIB")" \
  -Wl,-rpath-link,"$(dirname "$ARMHF_ALSA_LIB")" \
  -l:libasound.so.2
"$ARMHF_STRIP" --strip-unneeded \
  "$ARMHF_PLUGIN_DIR/libasound_module_pcm_plumos_hotplug.so"

install -m 0755 "$HELPER" "$PLUMOS_DIR/bin/plumos-audio-output"
install -m 0644 "$LICENSE" "$DOC_DIR/LICENSE.txt"
install -m 0644 "$LICENSE" "$LICENSE_DIR/plumos-audio-router-MIT.txt"

generated_at="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
source_ref="${PLUMOS_PIXEL2_SOURCE_REF:-$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || printf unknown)}"
version="${PLUMOS_PIXEL2_VERSION:-0.1.0-dev}"
cat >"$COMPONENT_DIR/manifest.json" <<EOF
{
  "name": "plumOS Pixel2 audio router",
  "component": "audio-router",
  "device": "pixel2",
  "architectures": ["aarch64", "armhf"],
  "version": "$version",
  "runtime_abi": "plumos-pixel2-audio-router-v1",
  "source_ref": "$source_ref",
  "generated_at": "$generated_at",
  "implementation": "alsa-ioplug",
  "logical_pcm": "plumos_output",
  "internal_route": "rockchiprk817-stereo",
  "usb_route": "first-usb-playback-card-stereo",
  "background_processes": 0
}
EOF

(
  cd "$PLUMOS_DIR"
  find \
    bin/plumos-audio-output \
    lib/alsa-lib/libasound_module_pcm_plumos_hotplug.so \
    lib/alsa-lib-armhf/libasound_module_pcm_plumos_hotplug.so \
    share/doc/audio-router/LICENSE.txt \
    licenses/plumos-audio-router-MIT.txt \
    components/audio-router/manifest.json \
    -type f -print |
    sort |
    while IFS= read -r path; do sha256sum "$path"; done
) >"$COMPONENT_DIR/checksums.sha256"
(
  cd "$PLUMOS_DIR"
  sha256sum -c components/audio-router/checksums.sha256
)

printf 'audio_router=result-ok output=%s\n' "$PLUMOS_DIR"
printf 'plugin_armhf=%s\n' \
  "$ARMHF_PLUGIN_DIR/libasound_module_pcm_plumos_hotplug.so"
