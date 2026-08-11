#!/bin/sh
set -eu

find_adb() {
    if [ -n "${PLUMOS_ADB_BIN:-}" ]; then
        [ -x "$PLUMOS_ADB_BIN" ] || {
            printf 'error: PLUMOS_ADB_BIN is not executable: %s\n' "$PLUMOS_ADB_BIN" >&2
            return 1
        }
        printf '%s\n' "$PLUMOS_ADB_BIN"
        return 0
    fi

    best=''
    best_key=0
    path_adb=$(command -v adb 2>/dev/null || true)
    for candidate in \
        /opt/homebrew/bin/adb \
        /usr/local/bin/adb \
        "$HOME/Library/Android/sdk/platform-tools/adb" \
        "$path_adb"; do
        [ -n "$candidate" ] && [ -x "$candidate" ] || continue
        version=$($candidate version 2>/dev/null | \
            sed -n 's/^Version \([0-9][0-9.]*\).*/\1/p' | head -n 1)
        [ -n "$version" ] || continue
        key=$(printf '%s\n' "$version" | \
            awk -F. '{ printf "%03d%03d%03d\n", $1, $2, $3 }')
        key=$(printf '%s\n' "$key" | sed 's/^0*//')
        [ -n "$key" ] || key=0
        if [ "$key" -gt "$best_key" ]; then
            best=$candidate
            best_key=$key
        fi
    done

    [ -n "$best" ] || {
        printf 'error: adb was not found; set PLUMOS_ADB_BIN\n' >&2
        return 1
    }
    printf '%s\n' "$best"
}

ADB=$(find_adb)

pixel2_transport_present() {
    "$ADB" devices 2>/dev/null | \
        grep -Eq '^plumos-pixel2-[^[:space:]]+[[:space:]]+device$'
}

pixel2_usb_present() {
    [ "$(uname -s)" = Darwin ] || return 1
    command -v ioreg >/dev/null 2>&1 || return 1
    ioreg -p IOUSB -w0 -l 2>/dev/null | grep -q 'plumOS Pixel2 ADB'
}

recover_host_transport() {
    pixel2_transport_present && return 0
    pixel2_usb_present || return 1
    printf '%s\n' \
        'pixel2-adb: USB is enumerated but ADB transport is absent; restarting host ADB server' >&2
    "$ADB" kill-server >/dev/null 2>&1 || true
    sleep 1
    "$ADB" start-server >/dev/null
    pixel2_transport_present
}

status() {
    printf 'adb=%s\n' "$ADB"
    "$ADB" version | sed -n '1,2p'
    printf 'usb_enumerated=%s\n' "$(pixel2_usb_present && echo 1 || echo 0)"
    printf 'transport_ready=%s\n' "$(pixel2_transport_present && echo 1 || echo 0)"
    "$ADB" devices -l
}

case "${1:-status}" in
    status) status ;;
    recover)
        if recover_host_transport; then status; else status; exit 1; fi
        ;;
    kill-server|start-server) exec "$ADB" "$@" ;;
    *)
        recover_host_transport || true
        exec "$ADB" "$@"
        ;;
esac
