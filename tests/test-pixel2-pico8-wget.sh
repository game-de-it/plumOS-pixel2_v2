#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
WRAPPER="$ROOT_DIR/package/standalone-pixel2/plumos/standalone/pico8/bin/wget"
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/plumos-pico8-wget.XXXXXX")"
trap 'rm -rf "$TEST_ROOT"' EXIT INT TERM HUP

cat >"$TEST_ROOT/fake-curl" <<'EOF'
#!/bin/sh
set -u
: "${PICO8_TEST_ARGS:?}"
printf '%s\n' "$@" >"$PICO8_TEST_ARGS"
output=""
while [ "$#" -gt 0 ]; do
  case "$1" in
    --output)
      shift
      output=$1
      ;;
  esac
  shift
done
[ -n "$output" ] || exit 90
printf 'pico8-cart-fixture\n' >"$output"
EOF
chmod 0755 "$TEST_ROOT/fake-curl"

output="$TEST_ROOT/cart.p8.png"
args="$TEST_ROOT/curl.args"
PICO8_TEST_ARGS="$args" PLUMOS_PICO8_CURL="$TEST_ROOT/fake-curl" \
  "$WRAPPER" \
  'http://www.lexaloffle.com/bbs/get_cart.php?cat=7&play_src=2&lid=test' \
  -q -O "$output"
grep -Fqx -- '--location' "$args"
grep -Fqx -- '--fail' "$args"
grep -Fqx -- "$output.tmp.$$" "$args" && {
  printf 'FAIL: wrapper temp path unexpectedly used the parent shell PID\n' >&2
  exit 1
} || true
grep -Fqx 'pico8-cart-fixture' "$output"

post="$TEST_ROOT/post.txt"
printf 'rating=1\n' >"$post"
PICO8_TEST_ARGS="$args" PLUMOS_PICO8_CURL="$TEST_ROOT/fake-curl" \
  "$WRAPPER" 'https://www.lexaloffle.com/bbs/test' \
  -q -O "$output" --post-file="$post"
grep -Fqx -- '--data-binary' "$args"
grep -Fqx -- "@$post" "$args"

if PICO8_TEST_ARGS="$args" PLUMOS_PICO8_CURL="$TEST_ROOT/fake-curl" \
    "$WRAPPER" 'ftp://example.invalid/cart' -q -O "$output" 2>/dev/null; then
  printf 'FAIL: unsupported PICO-8 download protocol was accepted\n' >&2
  exit 1
fi

printf 'PASS: Pixel2 PICO-8 HTTPS wget adapter\n'
