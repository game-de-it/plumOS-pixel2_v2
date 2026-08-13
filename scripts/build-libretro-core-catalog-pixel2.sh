#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
RECIPES="$ROOT_DIR/docker/pixel2-tools/libretro-core-recipes.tsv"
FILTER="${PLUMOS_PIXEL2_CATALOG_FILTER:-all}"
OUT_ROOT="$ROOT_DIR/${PLUMOS_PIXEL2_CATALOG_OUT:-output/libretro-cores/pixel2}"
WORK_ROOT="$ROOT_DIR/${PLUMOS_PIXEL2_CATALOG_WORK:-output/build/libretro-core-catalog-pixel2}"
HOST_JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
CONCURRENCY="${PLUMOS_PIXEL2_CORE_BUILD_CONCURRENCY:-4}"
FAIL_ON_CORE_ERROR="${FAIL_ON_CORE_ERROR:-1}"
REUSE_EXISTING="${PLUMOS_PIXEL2_CORE_REUSE_EXISTING:-1}"
REBUILD_IDS=""
OUT_DIR_EXPLICIT=0

usage() {
    printf '%s\n' \
        'Usage: scripts/build-libretro-core-catalog-pixel2.sh [--filter all|pixel2|class-a|class-b|class-o|ID[,ID...]] [--out-dir PATH] [--work-dir PATH] [--concurrency N] [--rebuild ID[,ID...]] [--fresh]' \
        '' \
        'Builds independent libretro cores with concurrent workers, then aggregates a canonical Pixel2 libretro-cores component.'
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --filter) FILTER="$2"; shift 2 ;;
        --out-dir) OUT_ROOT="$ROOT_DIR/$2"; OUT_DIR_EXPLICIT=1; shift 2 ;;
        --work-dir) WORK_ROOT="$ROOT_DIR/$2"; shift 2 ;;
        --concurrency) CONCURRENCY="$2"; shift 2 ;;
        --rebuild) REBUILD_IDS="$2"; shift 2 ;;
        --fresh) REUSE_EXISTING=0; shift ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'error: unknown argument: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

case "$HOST_JOBS:$CONCURRENCY" in
    *[!0-9:]*|0:*|*:0)
        printf 'error: jobs and concurrency must be positive integers\n' >&2
        exit 2
        ;;
esac

case "$OUT_ROOT" in
    "$ROOT_DIR"/*) ;;
    /*) ;;
    *) OUT_ROOT="$ROOT_DIR/$OUT_ROOT" ;;
esac
case "$WORK_ROOT" in
    "$ROOT_DIR"/*) ;;
    /*) ;;
    *) WORK_ROOT="$ROOT_DIR/$WORK_ROOT" ;;
esac

case "$FILTER" in
    all|ALL) complete_filter=1 ;;
    *) complete_filter=0 ;;
esac
if [ "$complete_filter" -eq 0 ] && [ "$OUT_DIR_EXPLICIT" -eq 0 ]; then
    safe_filter="$(printf '%s' "$FILTER" |
        tr '[:upper:]' '[:lower:]' |
        sed 's/[^a-z0-9._-]/_/g; s/^_*//; s/_*$//')"
    [ -n "$safe_filter" ] || safe_filter=custom
    OUT_ROOT="$ROOT_DIR/output/libretro-cores/pixel2-filtered/$safe_filter"
fi

per_core_jobs=$((HOST_JOBS / CONCURRENCY))
[ "$per_core_jobs" -gt 0 ] || per_core_jobs=1

selected() {
    local id="$1"
    local class="$2"
    local token
    local normalized

    IFS=',' read -r -a filters <<< "$FILTER"
    for token in "${filters[@]}"; do
        normalized="$(printf '%s' "$token" | tr -d '[:space:]')"
        [ -n "$normalized" ] || continue
        case "$normalized" in
            all|ALL)
                return 0
                ;;
            pixel2|plumos|default|class-plumos|Class-plumOS)
                { [ "$class" = A ] || [ "$class" = B ]; } && return 0
                ;;
            class-a|Class-A|a|A)
                [ "$class" = A ] && return 0
                ;;
            class-b|Class-B|b|B)
                [ "$class" = B ] && return 0
                ;;
            class-ab|Class-AB|ab|AB)
                { [ "$class" = A ] || [ "$class" = B ]; } && return 0
                ;;
            class-o|Class-O|o|O|extended-extra|Extended-extra)
                [ "$class" = O ] && return 0
                ;;
            "$id")
                return 0
                ;;
        esac
    done
    return 1
}

recipe_row() {
    local id="$1"
    awk -F'|' -v wanted="$id" '$1 == wanted { print; exit }' "$RECIPES"
}

forced_rebuild() {
    local id="$1"
    case ",$REBUILD_IDS," in
        *,"$id",*) return 0 ;;
        *) return 1 ;;
    esac
}

fingerprint_for() {
    local id="$1"
    local row
    row="$(recipe_row "$id")"
    {
        printf 'pixel2-libretro-catalog-v1\n'
        printf '%s\n' "$row"
        sha256sum "$ROOT_DIR/scripts/build-libretro-cores.sh" "$RECIPES" |
            awk '{ print $1 "  " $2 }'
    } | sha256sum | awk '{ print $1 }'
}

validate_existing() {
    local id="$1"
    local just_built="${2:-0}"
    local core_out="$OUT_ROOT/per-core/$id"
    local core_root="$core_out/plumos"
    local expected_ref
    local binary
    local recorded_fingerprint
    local expected_fingerprint

    if [ "$just_built" -eq 0 ]; then
        [ "$REUSE_EXISTING" -eq 1 ] || return 1
        forced_rebuild "$id" && return 1
    fi
    [ -f "$core_root/components/libretro-cores/checksums.sha256" ] &&
        [ -f "$core_root/components/libretro-cores/manifest.json" ] ||
        return 1

    expected_ref="$(recipe_row "$id" | awk -F'|' '{ print $4 }')"
    binary="$(
        jq -r --arg id "$id" --arg ref "$expected_ref" '
          .cores[] |
          select(.id == $id and .upstream_commit == $ref) |
          .binary
        ' "$core_root/components/libretro-cores/manifest.json"
    )"
    [ -n "$binary" ] && [ -f "$core_root/$binary" ] || return 1
    (
        cd "$core_root"
        sha256sum -c components/libretro-cores/checksums.sha256 >/dev/null
    ) || return 1

    expected_fingerprint="$(fingerprint_for "$id")"
    recorded_fingerprint="$(sed -n '1p' "$core_out/build-fingerprint" 2>/dev/null || true)"
    [ "$recorded_fingerprint" = "$expected_fingerprint" ]
}

ids=()
while IFS='|' read -r id class _; do
    case "$id" in
        ""|\#*) continue ;;
    esac
    selected "$id" "$class" && ids+=("$id")
done <"$RECIPES"
[ "${#ids[@]}" -gt 0 ] || {
    printf 'error: filter selected no cores: %s\n' "$FILTER" >&2
    exit 1
}

if [ "$REUSE_EXISTING" -eq 0 ]; then
    rm -rf "$OUT_ROOT"
else
    rm -rf "$OUT_ROOT/plumos" "$OUT_ROOT/logs" "$OUT_ROOT/status"
fi
mkdir -p "$OUT_ROOT/per-core" "$OUT_ROOT/logs" "$OUT_ROOT/status" "$WORK_ROOT/src"

printf 'catalog_build filter=%s cores=%s concurrency=%s per_core_jobs=%s\n' \
    "$FILTER" "${#ids[@]}" "$CONCURRENCY" "$per_core_jobs"

build_one() {
    local id="$1"
    local core_out="$OUT_ROOT/per-core/$id"
    local core_src="$WORK_ROOT/src/$id"
    local log="$OUT_ROOT/logs/$id.log"
    local fingerprint

    if validate_existing "$id"; then
        printf 'pass\n' >"$OUT_ROOT/status/$id"
        printf 'BUILD_CACHE_HIT %s\n' "$id"
        return 0
    fi

    core_args=(
        --filter "$id"
        --out-dir "${core_out#"$ROOT_DIR/"}"
        --src-root "${core_src#"$ROOT_DIR/"}"
        --jobs "$per_core_jobs"
        --fail-on-error 1
    )
    if { if [ -f /.dockerenv ] || [ "${PLUMOS_PIXEL2_CATALOG_NO_DOCKER:-0}" = 1 ]; then
             "$ROOT_DIR/scripts/build-libretro-cores.sh" --inside "${core_args[@]}"
         else
             "$ROOT_DIR/scripts/docker-build.sh" cores "${core_args[@]}"
         fi
       } >"$log" 2>&1; then
        fingerprint="$(fingerprint_for "$id")"
        printf '%s\n' "$fingerprint" >"$core_out/build-fingerprint"
        if validate_existing "$id" 1; then
            printf 'pass\n' >"$OUT_ROOT/status/$id"
            printf 'BUILD_PASS %s\n' "$id"
        else
            printf 'fail rc=cache-validation\n' >"$OUT_ROOT/status/$id"
            printf 'BUILD_FAIL %s rc=cache-validation log=%s\n' "$id" "$log"
        fi
    else
        rc=$?
        printf 'fail rc=%s\n' "$rc" >"$OUT_ROOT/status/$id"
        printf 'BUILD_FAIL %s rc=%s log=%s\n' "$id" "$rc" "$log"
    fi
}

for id in "${ids[@]}"; do
    build_one "$id" &
    while [ "$(jobs -pr | wc -l | tr -d ' ')" -ge "$CONCURRENCY" ]; do
        sleep 0.2
    done
done
wait || true

PLUMOS_DIR="$OUT_ROOT/plumos"
COMPONENT_DIR="$PLUMOS_DIR/components/libretro-cores"
mkdir -p "$PLUMOS_DIR/cores" "$PLUMOS_DIR/info" \
    "$PLUMOS_DIR/lib/libretro" "$PLUMOS_DIR/licenses" "$COMPONENT_DIR"

manifest_inputs=()
pass_count=0
fail_count=0
for id in "${ids[@]}"; do
    status="$(cat "$OUT_ROOT/status/$id")"
    case "$status" in
        pass)
            core_root="$OUT_ROOT/per-core/$id/plumos"
            for dir in cores info licenses; do
                [ -d "$core_root/$dir" ] && cp -a "$core_root/$dir/." "$PLUMOS_DIR/$dir/"
            done
            if [ -d "$core_root/lib/libretro" ]; then
                cp -a "$core_root/lib/libretro/." "$PLUMOS_DIR/lib/libretro/"
            fi
            manifest_inputs+=("$core_root/components/libretro-cores/manifest.json")
            pass_count=$((pass_count + 1))
            ;;
        *)
            fail_count=$((fail_count + 1))
            ;;
    esac
done

[ "$pass_count" -gt 0 ] || {
    printf 'error: no selected core built successfully\n' >&2
    exit 1
}

generated_at="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
source_ref="$(git -C "$ROOT_DIR" rev-parse --short HEAD 2>/dev/null || printf unknown)"
jq -s \
    --arg generated_at "$generated_at" \
    --arg source_ref "$source_ref" \
    --arg filter "$FILTER" \
    --argjson built "$pass_count" \
    --argjson failed "$fail_count" \
    '{
      name:"plumOS Pixel2 libretro core catalog",
      component:"libretro-cores",
      device:"pixel2",
      version:"1",
      architecture:"aarch64",
      rendering:"mixed",
      filter:$filter,
      built:$built,
      failed:$failed,
      source_ref:$source_ref,
      generated_at:$generated_at,
      cores:(
        map(.cores[]) |
        map(select((.upstream_commit // "") != "")) |
        map(. + {
          rendering:(
            if .id == "flycast" or
               .id == "flycast_xtreme" or
               .id == "km_duckswanstation_xtreme_amped" or
               .id == "parallel_n64"
            then "hardware-gles"
            else "software"
            end
          )
        }) |
        sort_by(.id)
      )
    }' "${manifest_inputs[@]}" >"$COMPONENT_DIR/manifest.json"

(
    cd "$PLUMOS_DIR"
    find cores info licenses lib/libretro components/libretro-cores \
        -type f ! -path 'components/libretro-cores/checksums.sha256' -print |
        LC_ALL=C sort |
        while IFS= read -r path; do sha256sum "$path"; done
) >"$COMPONENT_DIR/checksums.sha256"
(
    cd "$PLUMOS_DIR"
    sha256sum -c components/libretro-cores/checksums.sha256 >/dev/null
)

printf 'catalog_result pass=%s fail=%s output=%s\n' \
    "$pass_count" "$fail_count" "$OUT_ROOT"
[ "$fail_count" -eq 0 ] || [ "$FAIL_ON_CORE_ERROR" = 0 ]
