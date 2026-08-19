#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
PATCH="$ROOT_DIR/patches/retroarch/016-pixel2-drm-osd-rotation-thread-safety.patch"
CFG="$ROOT_DIR/package/retroarch-pixel2/retroarch.cfg"

grep -q 'drm_surface_map_logical_pixel' "$PATCH"
grep -q 'x >= surface->logical_width' "$PATCH"
grep -q 'slock_lock(_drmvars->pending_mutex)' "$PATCH"
grep -q 'slock_lock(vid->pending_mutex)' "$PATCH"
grep -q 'glyph->atlas_offset_x >= atlas->width' "$PATCH"
grep -q '^video_font_enable = "true"$' "$CFG"
grep -q '^video_font_path = "/mnt/plumos/fonts/default.otf"$' "$CFG"
grep -q '^video_message_color = "ffff00"$' "$CFG"
grep -q '^notification_show_save_state = "true"$' "$CFG"

printf '%s\n' 'pixel2_retroarch_drm_osd=result-ok'
