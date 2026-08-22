#!/bin/sh
set -eu

# Test-only power-overlay UI used by validate-pixel2-sleep-matrix.py.  The real
# overlay still performs display-owner pause/resume and the normal sleep action;
# this helper replaces only the physical menu selection.
selection=${PLUMOS_POWER_MENU_SELECTION:-}
[ -n "$selection" ] || {
    printf 'pixel2-sleep-auto-select: selection path is missing\n' >&2
    exit 2
}
printf 'action=sleep\n' >"$selection"
