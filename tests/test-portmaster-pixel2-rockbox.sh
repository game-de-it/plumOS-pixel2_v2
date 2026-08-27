#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
SOURCE="$ROOT_DIR/package/portmaster-pixel2/src/plumos_portmaster_rockbox.c"

if [[ "$(uname -s)" == Darwin ]]; then
    command -v docker >/dev/null 2>&1 || {
        echo 'Docker is required to exercise the Linux Rockbox interposer on macOS' >&2
        exit 1
    }
    exec docker run --rm \
        -v "$ROOT_DIR:/repo:ro" \
        plumos-pixel2-tools:dev \
        /repo/tests/test-portmaster-pixel2-rockbox.sh
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cat > "$work/SDL.h" <<'EOF'
#ifndef TEST_SDL_H
#define TEST_SDL_H
#include <stdint.h>
typedef uint8_t Uint8;
typedef uint32_t Uint32;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_Rect SDL_Rect;
#define SDL_WINDOWEVENT 0x200
#define SDL_WINDOWEVENT_RESIZED 5
#define SDL_WINDOWEVENT_SIZE_CHANGED 6
#define SDL_TEXTUREACCESS_STREAMING 1
typedef union SDL_Event {
    Uint32 type;
    struct {
        Uint32 type;
        Uint32 timestamp;
        Uint32 windowID;
        Uint8 event;
        Uint8 padding1;
        Uint8 padding2;
        Uint8 padding3;
        int32_t data1;
        int32_t data2;
    } window;
} SDL_Event;
SDL_Texture *SDL_CreateTexture(SDL_Renderer *, Uint32, int, int, int);
int SDL_WaitEvent(SDL_Event *);
int SDL_PollEvent(SDL_Event *);
#endif
EOF

cat > "$work/fake-sdl.c" <<'EOF'
#include "SDL.h"
#include <stdint.h>
static void make_resize(SDL_Event *event) {
    event->type = SDL_WINDOWEVENT;
    event->window.event = SDL_WINDOWEVENT_RESIZED;
    event->window.data1 = 480;
    event->window.data2 = 640;
}
SDL_Texture *SDL_CreateTexture(SDL_Renderer *renderer, Uint32 format, int access,
                               int width, int height) {
    (void)renderer; (void)format; (void)access; (void)width; (void)height;
    return (SDL_Texture *)(uintptr_t)0x1;
}
int SDL_WaitEvent(SDL_Event *event) { make_resize(event); return 1; }
int SDL_PollEvent(SDL_Event *event) { make_resize(event); return 1; }
EOF

cat > "$work/probe.c" <<'EOF'
#include "SDL.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    SDL_Event event;
    int use_poll = argc > 1 && strcmp(argv[1], "poll") == 0;
    int result = use_poll ? SDL_PollEvent(&event) : SDL_WaitEvent(&event);
    printf("before=%d,%u,%d,%d\n", result, event.window.event,
           event.window.data1, event.window.data2);
    SDL_CreateTexture(NULL, 0, SDL_TEXTUREACCESS_STREAMING, 320, 240);
    result = use_poll ? SDL_PollEvent(&event) : SDL_WaitEvent(&event);
    printf("after=%d,%u,%d,%d\n", result, event.window.event,
           event.window.data1, event.window.data2);
    return 0;
}
EOF

cc -shared -fPIC -I"$work" -o "$work/libfake-sdl.so" "$work/fake-sdl.c"
cc -shared -fPIC -Wall -Wextra -Werror -I"$work" \
    -Wl,-z,defs -Wl,-soname,libplumos-portmaster-rockbox.so \
    -o "$work/libplumos-portmaster-rockbox.so" "$SOURCE" -ldl
cc -I"$work" -L"$work" -Wl,-rpath,'$ORIGIN' \
    -o "$work/probe" "$work/probe.c" -lfake-sdl

baseline="$($work/probe)"
grep -q '^before=1,5,480,640$' <<<"$baseline"
grep -q '^after=1,5,480,640$' <<<"$baseline"

for mode in wait poll; do
    output="$(
        PLUMOS_PORTMASTER_ROCKBOX_TEST=1 \
        LD_PRELOAD="$work/libplumos-portmaster-rockbox.so" \
        "$work/probe" "$mode"
    )"
    grep -q '^before=1,6,480,640$' <<<"$output"
    grep -q '^after=1,5,480,640$' <<<"$output"
done

printf 'portmaster_pixel2_rockbox=result-ok\n'
