#!/bin/sh
set -u

# This helper is streamed to the device by smoke-test-pixel2-romset.py.  It is
# intentionally not installed in /mnt/plumos, so a validation run cannot leave
# an unmanaged app-layer file behind.

bb=/bin/busybox
root=/mnt/plumos
runtime=/run/plumos
backup=$runtime/romset-smoke-backup
output=$runtime/romset-smoke-launch.log
action=${ACTION:-}

fail() {
    printf 'SMOKE_RESULT=fail reason=%s\n' "$*" >&2
    return 1
}

frontend_stop() {
    pid=$($bb cat "$runtime/frontend.pid" 2>/dev/null || true)
    case "$pid" in
        ''|*[!0-9]*) $bb rm -f "$runtime/frontend.pid"; return 0 ;;
    esac
    exe=$($bb readlink "/proc/$pid/exe" 2>/dev/null || true)
    case "$exe" in
        "$root/bin/plumos-frontend-pixel2"|/usr/bin/plumos-frontend-pixel2) ;;
        *) fail "frontend-ownership-mismatch:$pid:$exe"; return 1 ;;
    esac
    $bb kill -TERM "$pid" 2>/dev/null || true
    count=0
    while $bb kill -0 "$pid" 2>/dev/null && [ "$count" -lt 5 ]; do
        $bb sleep 1
        count=$((count + 1))
    done
    if $bb kill -0 "$pid" 2>/dev/null; then
        $bb kill -KILL "$pid" 2>/dev/null || true
    fi
    $bb rm -f "$runtime/frontend.pid"
}

frontend_start() {
    /usr/lib/plumos/init.d/40-frontend start || return 1
    pid=$($bb cat "$runtime/frontend.pid" 2>/dev/null || true)
    case "$pid" in ''|*[!0-9]*) return 1 ;; esac
    $bb kill -0 "$pid" 2>/dev/null
}

backup_frontend_state() {
    $bb rm -rf "$backup"
    $bb mkdir -p "$backup"
    for name in recent.json resume-session.json; do
        source="$root/state/frontend/$name"
        if [ -f "$source" ]; then
            $bb cp -p "$source" "$backup/$name"
            printf 'present\n' >"$backup/$name.status"
        else
            printf 'absent\n' >"$backup/$name.status"
        fi
    done
}

restore_frontend_state() {
    [ -d "$backup" ] || return 0
    $bb mkdir -p "$root/state/frontend"
    for name in recent.json resume-session.json; do
        status=$($bb cat "$backup/$name.status" 2>/dev/null || true)
        case "$status" in
            present) $bb cp -p "$backup/$name" "$root/state/frontend/$name" ;;
            absent) $bb rm -f "$root/state/frontend/$name" ;;
        esac
    done
    $bb rm -rf "$backup"
}

runtime_pid_for_profile() {
    profile=$1
    family=${profile%%:*}
    id=${profile#*:}
    case "$family" in
        retroarch)
            for pid in $($bb pidof retroarch 2>/dev/null || true); do
                [ "$($bb readlink "/proc/$pid/exe" 2>/dev/null || true)" = "$root/bin/retroarch" ] || continue
                printf '%s\n' "$pid"
                return 0
            done
            ;;
        picoarch)
            pid=$($bb cat "$runtime/picoarch/runtime.pid" 2>/dev/null || true)
            [ -n "$pid" ] && [ -r "/proc/$pid/exe" ] && printf '%s\n' "$pid"
            ;;
        standalone)
            pid=$($bb cat "$runtime/standalone/$id.pid" 2>/dev/null || true)
            [ -n "$pid" ] && [ -r "/proc/$pid/exe" ] && printf '%s\n' "$pid"
            ;;
        pyxel)
            for proc in /proc/[0-9]*/exe; do
                [ -r "$proc" ] || continue
                exe=$($bb readlink "$proc" 2>/dev/null || true)
                [ "$exe" = "$root/apps/python/bin/python3.11" ] || continue
                pid=${proc#/proc/}
                printf '%s\n' "${pid%/exe}"
                return 0
            done
            ;;
    esac
}

runtime_stop_for_profile() {
    profile=$1
    pid=$2
    family=${profile%%:*}
    id=${profile#*:}
    case "$family" in
        picoarch) "$root/bin/plumos-picoarch-stop" >/dev/null 2>&1 || true ;;
        standalone) "$root/bin/plumos-standalone-stop" "$id" >/dev/null 2>&1 || true ;;
        retroarch|pyxel) $bb kill -TERM "$pid" 2>/dev/null || true ;;
    esac
    count=0
    while $bb kill -0 "$pid" 2>/dev/null && [ "$count" -lt 5 ]; do
        $bb sleep 1
        count=$((count + 1))
    done
    $bb kill -0 "$pid" 2>/dev/null && $bb kill -KILL "$pid" 2>/dev/null || true
}

case "$action" in
    prepare)
        backup_frontend_state
        frontend_stop
        printf 'SMOKE_RESULT=prepared\n'
        ;;
    scan)
        system=${SMOKE_SYSTEM:-}
        [ -n "$system" ] || {
            fail missing-scan-system
            exit 1
        }
        export PLUMOS_ROOT="$root"
        export PLUMOS_SDCARD_ROOT=/mnt/plumos-user
        export PLUMOS_ROM_ROOT=/mnt/plumos-user/roms
        export PLUMOS_BIOS_ROOT=/mnt/plumos-user/bios
        export LD_LIBRARY_PATH="$root/frontend/lib:$root/emulator/lib:$root/lib"
        "$root/bin/plumos-library-scan" --system "$system" || {
            fail "scan:$system"
            exit 1
        }
        printf 'SMOKE_RESULT=scanned system=%s\n' "$system"
        ;;
    launch)
        system=${SMOKE_SYSTEM:-}
        relative=${SMOKE_RELATIVE:-}
        profile=${SMOKE_PROFILE:-}
        seconds=${SMOKE_SECONDS:-8}
        [ -n "$system" ] && [ -n "$relative" ] && [ -n "$profile" ] || {
            fail missing-launch-arguments
            exit 1
        }
        case "$seconds" in ''|*[!0-9]*) fail invalid-duration; exit 1 ;; esac
        if [ -s "$runtime/frontend.pid" ]; then
            fail frontend-still-running
            exit 1
        fi
        : >"$output"
        export PLUMOS_ROOT="$root"
        export PLUMOS_RUNTIME_ROOT="$runtime"
        export PLUMOS_SDCARD_ROOT=/mnt/plumos-user
        export PLUMOS_ROM_ROOT=/mnt/plumos-user/roms
        export PLUMOS_BIOS_ROOT=/mnt/plumos-user/bios
        export PLUMOS_DEVICE_ID=pixel2
        export PLUMOS_DEVICE_NAME='GKD Pixel2'
        export PLUMOS_BUSYBOX="$bb"
        export LD_LIBRARY_PATH="$root/frontend/lib:$root/emulator/lib:$root/lib"
        "$root/bin/plumos-text-ui" launch "$system" "$relative" \
            --profile "$profile" --execute --no-scan >"$output" 2>&1 &
        launch_pid=$!
        count=0
        runtime_pid=
        while [ "$count" -lt "$seconds" ]; do
            if ! $bb kill -0 "$launch_pid" 2>/dev/null; then
                $bb tail -n 40 "$output" 2>/dev/null || true
                fail "early-exit:$profile"
                exit 1
            fi
            runtime_pid=$(runtime_pid_for_profile "$profile" || true)
            $bb sleep 1
            count=$((count + 1))
        done
        runtime_pid=$(runtime_pid_for_profile "$profile" || true)
        if [ -z "$runtime_pid" ] || [ ! -r "/proc/$runtime_pid/exe" ]; then
            $bb tail -n 40 "$output" 2>/dev/null || true
            $bb kill -TERM "$launch_pid" 2>/dev/null || true
            $bb sleep 1
            $bb kill -0 "$launch_pid" 2>/dev/null && \
                $bb kill -KILL "$launch_pid" 2>/dev/null || true
            fail "runtime-not-found:$profile"
            exit 1
        fi
        runtime_exe=$($bb readlink "/proc/$runtime_pid/exe" 2>/dev/null || true)
        printf 'SMOKE_RUNTIME_PID=%s\nSMOKE_RUNTIME_EXE=%s\n' "$runtime_pid" "$runtime_exe"
        runtime_stop_for_profile "$profile" "$runtime_pid"
        count=0
        while $bb kill -0 "$launch_pid" 2>/dev/null && [ "$count" -lt 5 ]; do
            $bb sleep 1
            count=$((count + 1))
        done
        $bb kill -0 "$launch_pid" 2>/dev/null && $bb kill -TERM "$launch_pid" 2>/dev/null || true
        wait "$launch_pid" 2>/dev/null || true
        $bb tail -n 20 "$output" 2>/dev/null || true
        printf 'SMOKE_RESULT=pass profile=%s survived=%ss\n' "$profile" "$seconds"
        ;;
    restore)
        restore_frontend_state
        frontend_start || {
            fail frontend-restart
            exit 1
        }
        printf 'SMOKE_RESULT=restored\n'
        ;;
    *)
        fail "unknown-action:$action"
        exit 2
        ;;
esac
