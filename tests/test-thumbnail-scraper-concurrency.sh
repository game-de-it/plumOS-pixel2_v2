#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/plumos-thumbnail-concurrency.XXXXXX")"
trap 'rm -rf "$TEST_ROOT"' EXIT

mkdir -p \
    "$TEST_ROOT/card/Roms/nes" \
    "$TEST_ROOT/card/Images" \
    "$TEST_ROOT/plumos/bin" \
    "$TEST_ROOT/plumos/config/frontend" \
    "$TEST_ROOT/run"

printf 'rom-one\n' >"$TEST_ROOT/card/Roms/nes/One.nes"
printf 'rom-two\n' >"$TEST_ROOT/card/Roms/nes/Two.nes"

cat >"$TEST_ROOT/plumos/config/frontend/systems.json" <<'EOF'
{
  "version": 1,
  "systems": [
    {
      "id": "nes",
      "directory_aliases": [
        {
          "name": "nes",
          "source": "test",
          "priority": 10
        }
      ],
      "scraper": {
        "enabled": true,
        "reason": "test_parallel_index_bootstrap",
        "extensions": [
          "nes"
        ],
        "crc_workers": {
          "default": 2,
          "bulk": 2,
          "max": 2
        },
        "download_workers": {
          "default": 2,
          "bulk": 2,
          "max": 2
        }
      }
    }
  ]
}
EOF
printf 'nes\tNintendo - Nintendo Entertainment System\tmetadat/no-intro/Nintendo - Nintendo Entertainment System.dat\n' \
    >"$TEST_ROOT/plumos/config/frontend/scraper-sources.tsv"

cat >"$TEST_ROOT/fake-busybox" <<'EOF'
#!/bin/sh
applet="$(/usr/bin/basename "$0")"
if [ "$applet" = fake-busybox ]; then
  applet="$1"
  shift
fi
case "$applet" in
  crc32)
    printf 'deadbeef\n'
    exit 0
    ;;
  awk|cmp|cut|dirname|find|grep|head|od|sed|sort|tail|tr|wc)
    exec "/usr/bin/$applet" "$@"
    ;;
  cat|date|dd|ln|ls|mkdir|mv|rm|sh|sleep)
    exec "/bin/$applet" "$@"
    ;;
  unzip)
    exec /usr/bin/unzip "$@"
    ;;
  wget)
    exit 127
    ;;
esac
exit 127
EOF
chmod 0755 "$TEST_ROOT/fake-busybox"

cat >"$TEST_ROOT/plumos/bin/curl" <<'EOF'
#!/bin/sh
output=
url=
while [ "$#" -gt 0 ]; do
  case "$1" in
    -o)
      output="$2"
      shift 2
      ;;
    http://*|https://*)
      url="$1"
      shift
      ;;
    *)
      shift
      ;;
  esac
done
[ -n "$output" ] && [ -n "$url" ] || exit 2
printf '%s\n' "$url" >>"$MOCK_CURL_CALLS"
case "$url" in
  *libretro-database*)
    sleep 2
    cat >"$output" <<'DAT'
game (
  name "Test Game"
  rom ( name "Test Game.nes" size 8 crc DEADBEEF )
)
DAT
    ;;
  *thumbnails.libretro.com*/Named_Titles/)
    sleep 2
    printf '<a href="Test%%20Game.png">Test Game.png</a>\n' >"$output"
    ;;
  *libretro-thumbnails*)
    printf '\211PNG\r\n\032\n' >"$output"
    ;;
  *)
    exit 22
    ;;
esac
EOF
chmod 0755 "$TEST_ROOT/plumos/bin/curl"

MOCK_CURL_CALLS="$TEST_ROOT/curl.calls" \
PLUMOS_ROOT="$TEST_ROOT/plumos" \
PLUMOS_SDCARD_ROOT="$TEST_ROOT/card" \
PLUMOS_THUMBNAIL_STATE_DIR="$TEST_ROOT/run" \
PLUMOS_THUMBNAIL_CACHE_DIR="$TEST_ROOT/cache" \
PLUMOS_BUSYBOX="$TEST_ROOT/fake-busybox" \
PLUMOS_THUMBNAIL_FETCH_TIMEOUT=10 \
PLUMOS_THUMBNAIL_FETCH_RETRY=0 \
PLUMOS_THUMBNAIL_INDEX_LOCK_WAIT=15 \
    "$ROOT_DIR/package/app-layer-pixel2/bin/plumos-thumbnail-scraper" \
    --fetch --system nes --kind Named_Titles >"$TEST_ROOT/result.tsv"

awk -F '\t' '
  $1 == "fetch" && $2 == "nes" && $6 == 2 && $8 == 2 &&
  $9 == 2 && $10 == 2 && $11 == 2 && $18 == 0 { ok = 1 }
  END { exit ok ? 0 : 1 }
' "$TEST_ROOT/result.tsv"
test "$(grep -c 'libretro-database' "$TEST_ROOT/curl.calls")" -eq 1
test "$(grep -c 'thumbnails.libretro.com' "$TEST_ROOT/curl.calls")" -eq 1
test "$(grep -c 'libretro-thumbnails' "$TEST_ROOT/curl.calls")" -eq 2
test -s "$TEST_ROOT/card/Images/nes/One.png"
test -s "$TEST_ROOT/card/Images/nes/Two.png"

printf 'thumbnail_scraper_concurrency=result-ok\n'
