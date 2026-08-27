#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SOURCE="$ROOT_DIR/package/portmaster-pixel2/src/plumos_portmaster_exec_guard.c"

run_test() {
    local source=$1
    local work=$2
    local guard="$work/libplumos-portmaster-exec-guard.so"
    local output="$work/environment.txt"

    cc -O2 -fPIC -Wall -Wextra -Werror -shared -Wl,-z,defs \
        -Wl,-soname,libplumos-portmaster-exec-guard.so \
        -o "$guard" "$source" -ldl
    PLUMOS_PORTMASTER_REQUIRED_LD_LIBRARY_PATH=/required/common:/required/video \
    PLUMOS_PORTMASTER_REQUIRED_LD_PRELOAD="$guard:/required/rotate.so" \
    PLUMOS_PORTMASTER_SESSION_ID=pixel2-test-session \
    LD_PRELOAD="$guard" \
        /bin/sh -c \
        'LD_LIBRARY_PATH=/port/libs; LD_PRELOAD=/port/scaler.so; PLUMOS_PORTMASTER_SESSION_ID=foreign; export LD_LIBRARY_PATH LD_PRELOAD PLUMOS_PORTMASTER_SESSION_ID; exec /usr/bin/env' \
        >"$output" 2>"$work/loader.log"

    grep -q '^LD_LIBRARY_PATH=/port/libs:/required/common:/required/video$' "$output"
    grep -q "^LD_PRELOAD=$guard:/required/rotate.so:/port/scaler.so$" "$output"
    grep -q '^PLUMOS_PORTMASTER_SESSION_ID=pixel2-test-session$' "$output"
}

if [[ "$(uname -s)" == Darwin ]]; then
    command -v docker >/dev/null 2>&1 || {
        echo 'Docker is required to exercise the Linux exec guard on macOS' >&2
        exit 1
    }
    docker run --rm \
        -v "$ROOT_DIR:/repo:ro" \
        plumos-pixel2-tools:dev \
        /repo/tests/test-portmaster-pixel2-exec-guard.sh
else
    work="$(mktemp -d /tmp/plumos-exec-guard-test.XXXXXX)"
    trap 'rm -rf "$work"' EXIT
    run_test "$SOURCE" "$work"
fi

printf 'portmaster_pixel2_exec_guard=result-ok\n'
