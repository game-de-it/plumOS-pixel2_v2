#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
LAUNCHER="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-frontend-launch"

if [[ "$(uname -s)" == Darwin ]]; then
    command -v docker >/dev/null 2>&1 || {
        echo 'Docker is required to test the Pixel2 frontend environment on macOS' >&2
        exit 1
    }
    exec docker run --rm \
        -v "$ROOT_DIR:/repo:ro" \
        plumos-pixel2-tools:dev \
        /repo/tests/test-pixel2-frontend-clean-environment.sh
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/root/bin" "$work/run"

cat >"$work/root/bin/plumos-frontend-stop" <<'EOF'
#!/bin/sh
if [ -f "$PLUMOS_ROOT/frontend.started" ]; then
    printf 'frontend=running pid=123\n'
fi
EOF
chmod 0755 "$work/root/bin/plumos-frontend-stop"

cat >"$work/fake-init" <<'EOF'
#!/bin/sh
env | sort >"$PLUMOS_ROOT/frontend.environment"
touch "$PLUMOS_ROOT/frontend.started"
EOF
chmod 0755 "$work/fake-init"

LD_PRELOAD=/poison/previous-app.so \
LD_LIBRARY_PATH=/poison/previous-app/lib \
SDL_VIDEODRIVER=poison \
SDL_RENDER_DRIVER=poison \
PLUMOS_PORTMASTER_SESSION_ID=poison-session \
PLUMOS_PORTMASTER_REQUIRED_LD_PRELOAD=/poison/required.so \
PLUMOS_PYXEL_FIT=poison \
PLUMOS_ROOT="$work/root" \
PLUMOS_RUNTIME_ROOT="$work/run" \
PLUMOS_FRONTEND_INIT="$work/fake-init" \
    /bin/sh "$LAUNCHER" >"$work/launcher.log" 2>&1

grep -q '^frontend=started$' "$work/launcher.log"
grep -q "^PLUMOS_ROOT=$work/root$" "$work/root/frontend.environment"
grep -q "^PLUMOS_RUNTIME_ROOT=$work/run$" "$work/root/frontend.environment"
grep -q '^PATH=/sbin:/usr/sbin:/bin:/usr/bin$' "$work/root/frontend.environment"
! grep -Eq '^(LD_PRELOAD|LD_LIBRARY_PATH|SDL_|PLUMOS_PORTMASTER_|PLUMOS_PYXEL_)' \
    "$work/root/frontend.environment"

printf 'pixel2_frontend_clean_environment=result-ok\n'
