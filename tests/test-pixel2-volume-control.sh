#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CONTROL="$ROOT_DIR/package/app-layer-pixel2/bin/plumos-volume-control"
TASK_TMP="$(mktemp -d "${TMPDIR:-/tmp}/plumos-volume-test.XXXXXX")"
trap 'rm -rf "$TASK_TMP"' EXIT

export PLUMOS_ROOT="$TASK_TMP/plumos"
export PLUMOS_SYSTEM_SETTINGS_JSON="$TASK_TMP/plumos/config/system/settings.json"
export PLUMOS_VOLUME_RUNTIME_STATE="$TASK_TMP/run/volume/current"
export PLUMOS_SPEAKER_BOOST_RUNTIME_STATE="$TASK_TMP/run/volume/speaker-boost-step"
export PLUMOS_SPEAKER_BOOST_SAVED_STATE="$TASK_TMP/plumos/config/system/speaker-boost-step"
export PLUMOS_VOLUME_LOG="$TASK_TMP/run/volume/last-apply.log"

[ "$($CONTROL speaker-boost get)" = "3.0" ]

$CONTROL speaker-boost apply 4.5
[ "$(cat "$PLUMOS_SPEAKER_BOOST_RUNTIME_STATE")" = "9" ]
[ "$(cat "$PLUMOS_SPEAKER_BOOST_SAVED_STATE")" = "9" ]
[ "$($CONTROL speaker-boost get)" = "4.5" ]

$CONTROL speaker-boost runtime 5.5
[ "$(cat "$PLUMOS_SPEAKER_BOOST_RUNTIME_STATE")" = "11" ]
[ "$(cat "$PLUMOS_SPEAKER_BOOST_SAVED_STATE")" = "9" ]

$CONTROL speaker-boost runtime 20
[ "$(cat "$PLUMOS_SPEAKER_BOOST_RUNTIME_STATE")" = "40" ]
[ "$($CONTROL speaker-boost get)" = "20.0" ]

rm -f "$PLUMOS_SPEAKER_BOOST_RUNTIME_STATE"
$CONTROL apply 20
[ "$(cat "$PLUMOS_SPEAKER_BOOST_RUNTIME_STATE")" = "9" ]

if $CONTROL speaker-boost apply 4.2 >/dev/null 2>&1; then
  printf 'invalid boost value was accepted\n' >&2
  exit 1
fi
if $CONTROL speaker-boost apply 20.5 >/dev/null 2>&1; then
  printf 'out-of-range boost value was accepted\n' >&2
  exit 1
fi

printf 'pixel2_volume_control=result-ok\n'
