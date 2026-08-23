#!/bin/sh
# Run repeated Pixel2 Neo Geo launch/exit checks entirely on the device.
# Keeping DRM raw captures local avoids turning the test into a Wi-Fi transfer
# benchmark while FBNeo is using the RK3326 CPU.

set -u

CYCLES="${1:-4}"
FIRST_CYCLE="${2:-1}"
ROM_RELATIVE="${PLUMOS_NEOGEO_ROM:-NEOGEO/aof.zip}"
ROM_PATH="/mnt/plumos-user/roms/${ROM_RELATIVE}"
EXPECTED_ROM_SHA256="${PLUMOS_NEOGEO_ROM_SHA256:-7df2835107f64ad3354b80fd7da81e15c93171b8f3b32d65c6957f79ab5611ec}"
CAPTURE_TOOL="${PLUMOS_DRM_CAPTURE_TOOL:-/tmp/drm-scanout-capture}"
ERROR_PATTERN='mmc0:.*(error|timeout|failed)|dwmmc_rockchip.*(bad|error|fail)|FAT-fs.*(error|corrupt)|EXT4-fs error|I/O error'
BASELINE_ERRORS="${PLUMOS_STORAGE_ERROR_BASELINE:-1}"
RUN_SECONDS="${PLUMOS_NEOGEO_RUN_SECONDS:-5}"
FAILURES=0

count_storage_errors() {
  dmesg | grep -Eic "$ERROR_PATTERN" 2>/dev/null || true
}

analyze_capture() {
  prefix="$1"
  /usr/bin/python3 - "$prefix" <<'PY'
import glob
import os
import sys

best_ratio = 0.0
best_colors = 0
visible = False
for path in glob.glob(sys.argv[1] + "-plane-*.xrgb8888"):
    data = open(path, "rb").read()
    if len(data) < 4:
        continue
    pixels = len(data) // 4
    nonblack = 0
    sampled_colors = set()
    for offset in range(0, pixels * 4, 4):
        color = bytes(data[offset : offset + 3])
        if color != b"\x00\x00\x00":
            nonblack += 1
        # Cap memory while inspecting the complete plane. Sampling a fixed
        # stride can repeatedly hit the same scanline position and undercount
        # a perfectly valid low-resolution game frame.
        if len(sampled_colors) < 16:
            sampled_colors.add(color)
    ratio = nonblack / pixels
    colors = len(sampled_colors)
    plane_visible = ratio >= 0.01 and colors >= 3
    if plane_visible:
        visible = True
    if (plane_visible and best_colors < 3) or (
        plane_visible == (best_colors >= 3) and ratio > best_ratio
    ):
        best_ratio = ratio
        best_colors = colors
print(f"{int(visible)} {best_ratio:.6f} {best_colors}")
PY
}

cycle=0
while [ "$cycle" -lt "$CYCLES" ]; do
  number=$((FIRST_CYCLE + cycle))
  prefix="/tmp/plumos-neogeo-loop-${number}"
  rm -f "${prefix}"* 2>/dev/null || true

  /mnt/plumos/bin/plumos-frontend-stop stop >/dev/null 2>&1 || true
  launch_command="exec env PLUMOS_ROOT=/mnt/plumos PLUMOS_SDCARD_ROOT=/mnt/plumos-user PLUMOS_RUNTIME_ROOT=/run/plumos /mnt/plumos/bin/plumos-text-ui launch neogeo '${ROM_RELATIVE}' --profile retroarch:fbneo --execute --no-scan"
  setsid /bin/sh -c "$launch_command" >"${prefix}.launch.log" 2>&1 </dev/null &
  group_pid=$!
  sleep "$RUN_SECONDS"

  emulator_pids="$(ps -eo pid,pgid,stat,comm | awk -v pgid="$group_pid" '$2 == pgid && $3 !~ /^Z/ && $4 == "retroarch" {print $1}')"
  if [ -n "$emulator_pids" ]; then
    startup=1
  else
    startup=0
  fi

  if command -v timeout >/dev/null 2>&1; then
    timeout 8 "$CAPTURE_TOOL" /dev/dri/card0 "$prefix" >"${prefix}.capture.log" 2>&1 || true
  else
    "$CAPTURE_TOOL" /dev/dri/card0 "$prefix" >"${prefix}.capture.log" 2>&1 || true
  fi
  capture_result="$(analyze_capture "$prefix" 2>/dev/null || echo '0 0.000000 0')"
  display="$(printf '%s\n' "$capture_result" | awk '{print $1}')"
  display_ratio="$(printf '%s\n' "$capture_result" | awk '{print $2}')"
  display_colors="$(printf '%s\n' "$capture_result" | awk '{print $3}')"

  audio_state_1="$(awk '/^state:/ {print $2}' /proc/asound/card0/pcm0p/sub0/status 2>/dev/null || true)"
  audio_ptr_1="$(awk '/^hw_ptr/ {print $3}' /proc/asound/card0/pcm0p/sub0/status 2>/dev/null || true)"
  sleep 1
  audio_state_2="$(awk '/^state:/ {print $2}' /proc/asound/card0/pcm0p/sub0/status 2>/dev/null || true)"
  audio_ptr_2="$(awk '/^hw_ptr/ {print $3}' /proc/asound/card0/pcm0p/sub0/status 2>/dev/null || true)"
  if [ "$audio_state_1" = RUNNING ] && [ "$audio_state_2" = RUNNING ] &&
      [ -n "$audio_ptr_1" ] && [ "$audio_ptr_1" != "$audio_ptr_2" ]; then
    audio=1
  else
    audio=0
  fi

  for emulator_pid in $emulator_pids; do
    kill -TERM "$emulator_pid" 2>/dev/null || true
  done
  tries=0
  while [ "$tries" -lt 50 ]; do
    live="$(ps -eo pgid,stat | awk -v pgid="$group_pid" '$1 == pgid && $2 !~ /^Z/ {n++} END {print n+0}')"
    [ "$live" -eq 0 ] && break
    sleep 0.1
    tries=$((tries + 1))
  done
  kill -TERM -"$group_pid" 2>/dev/null || true
  sleep 1
  kill -KILL -"$group_pid" 2>/dev/null || true

  /mnt/plumos/bin/plumos-frontend-stop stop >/dev/null 2>&1 || true
  /mnt/plumos/bin/plumos-frontend-launch >/dev/null 2>&1 || true
  sleep 2
  frontend_count="$(ps | grep '/mnt/plumos/bin/plumos-frontend-pixel2' | grep -v grep | wc -l)"
  emulator_count="$(ps | grep -E 'retroarch|fbneo_libretro' | grep -v grep | wc -l)"
  storage_errors="$(count_storage_errors)"
  rom_sha256="$(sha256sum "$ROM_PATH" 2>/dev/null | awk '{print $1}')"
  runtime_read=0
  if (cd /mnt/plumos && sha256sum -c components/frontend/checksums.sha256 >/dev/null 2>&1); then
    runtime_read=1
  fi

  result=pass
  if [ "$startup" -ne 1 ] || [ "$display" -ne 1 ] || [ "$audio" -ne 1 ] ||
      [ "$frontend_count" -ne 1 ] || [ "$emulator_count" -ne 0 ] ||
      [ "$storage_errors" -ne "$BASELINE_ERRORS" ] ||
      [ "$rom_sha256" != "$EXPECTED_ROM_SHA256" ] || [ "$runtime_read" -ne 1 ]; then
    result=fail
    FAILURES=$((FAILURES + 1))
  fi

  printf 'cycle=%s result=%s startup=%s display=%s display_ratio=%s colors=%s audio=%s frontend=%s emulator=%s storage_errors=%s rom_sha256=%s runtime_read=%s\n' \
    "$number" "$result" "$startup" "$display" "$display_ratio" "$display_colors" \
    "$audio" "$frontend_count" "$emulator_count" "$storage_errors" \
    "$rom_sha256" "$runtime_read"
  cycle=$((cycle + 1))
done

exit "$FAILURES"
