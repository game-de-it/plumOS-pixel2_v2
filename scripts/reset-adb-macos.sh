#!/bin/sh
set -eu

ADB=${ADB:-${ANDROID_HOME:-$HOME/Library/Android/sdk}/platform-tools/adb}
[ -x "$ADB" ] || {
    printf 'error: adb not found: %s\n' "$ADB" >&2
    exit 1
}

"$ADB" kill-server >/dev/null 2>&1 || true
sleep 1
if lsof -nP -iTCP:5037 -sTCP:LISTEN >/dev/null 2>&1; then
    printf 'error: TCP 5037 is still owned by another process:\n' >&2
    lsof -nP -iTCP:5037 -sTCP:LISTEN >&2
    exit 1
fi
"$ADB" start-server
"$ADB" devices -l
