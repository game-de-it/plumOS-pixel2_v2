#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

if [ ! -r /proc/self/status ]; then
    image="${PLUMOS_PIXEL2_TOOLS_IMAGE:-plumos-pixel2-tools:dev}"
    docker image inspect "$image" >/dev/null 2>&1 || \
        "$ROOT_DIR/scripts/build-tools-image.sh"
    exec docker run --rm --platform linux/arm64 \
        -v "$ROOT_DIR:/repo:ro" -w /repo "$image" \
        /repo/tests/test-portmaster-pixel2-gptokey-exit.sh
fi

work="$(mktemp -d)"
cleanup() {
    if [ -n "${session_pid:-}" ]; then
        kill -KILL "-$session_pid" 2>/dev/null || true
    fi
    rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM

plumos_root="$work/plumos"
run_root="$work/run"
card_root="$work/card"
rom_root="$card_root/roms"
pm_dir="$plumos_root/state/portmaster/data/upstream/PortMaster"
mkdir -p "$plumos_root/bin" "$pm_dir" "$run_root" "$rom_root/PORTS"

install -m 0755 \
    "$ROOT_DIR/package/portmaster-pixel2/plumos/bin/plumos-portmaster-port-stop" \
    "$plumos_root/bin/plumos-portmaster-port-stop"
install -m 0755 \
    "$ROOT_DIR/package/portmaster-pixel2/plumos/apps/portmaster/adapter/shims/pkill" \
    "$plumos_root/bin/pkill"

cat > "$rom_root/PORTS/Fixture.sh" <<EOF
#!/bin/sh
printf 'fixture\n'
EOF
chmod 0755 "$rom_root/PORTS/Fixture.sh"

cat > "$pm_dir/gptokeyb2" <<'EOF'
#!/bin/sh
set -eu
printf '%s\n' "$$" > "$PLUMOS_PORTMASTER_RUN_ROOT/gptokeyb2.pid"
"$PLUMOS_ROOT/bin/pkill" rockbox
printf 'gptokey-survived\n' > "$PLUMOS_PORTMASTER_RUN_ROOT/unexpected"
EOF
chmod 0755 "$pm_dir/gptokeyb2"

cat > "$work/session.sh" <<'EOF'
#!/bin/sh
set -eu
printf '%s\n' "$$" > "$PLUMOS_PORTMASTER_RUN_ROOT/port.pid"
sed 's/.*) //' "/proc/$$/stat" | awk '{print $20}' > \
    "$PLUMOS_PORTMASTER_RUN_ROOT/port.starttime"
printf '%s\n' "$PLUMOS_ROM_ROOT/PORTS/Fixture.sh" > \
    "$PLUMOS_PORTMASTER_RUN_ROOT/port.script"
"$PLUMOS_PORTMASTER_DATA_ROOT/upstream/PortMaster/gptokeyb2"
printf 'session-survived\n' > "$PLUMOS_PORTMASTER_RUN_ROOT/unexpected"
EOF
chmod 0755 "$work/session.sh"

setsid env \
    PLUMOS_ROOT="$plumos_root" \
    PLUMOS_SDCARD_ROOT="$card_root" \
    PLUMOS_ROM_ROOT="$rom_root" \
    PLUMOS_PORTMASTER_RUN_ROOT="$run_root" \
    PLUMOS_PORTMASTER_DATA_ROOT="$plumos_root/state/portmaster/data" \
    "$work/session.sh" &
session_pid=$!

for _ in $(seq 1 50); do
    kill -0 "$session_pid" 2>/dev/null || break
    sleep 0.1
done
if kill -0 "$session_pid" 2>/dev/null; then
    printf 'owned GPTokeYB exit did not stop the PortMaster session\n' >&2
    exit 1
fi
wait "$session_pid" 2>/dev/null || true
session_pid=""
[ ! -e "$run_root/unexpected" ]

# A normal port shell must not acquire global process-name kill authority.
if env \
    PLUMOS_ROOT="$plumos_root" \
    PLUMOS_PORTMASTER_RUN_ROOT="$run_root" \
    PLUMOS_PORTMASTER_DATA_ROOT="$plumos_root/state/portmaster/data" \
    "$plumos_root/bin/pkill" rockbox 2>"$work/unowned.err"; then
    printf 'unowned pkill was accepted\n' >&2
    exit 1
fi
grep -q 'denied unowned pkill pattern: rockbox' "$work/unowned.err"

printf 'portmaster_pixel2_gptokey_exit=result-ok\n'
