// SPDX-License-Identifier: MIT
#define _GNU_SOURCE

#include <SDL.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Rockbox's SDL application receives the Pixel2 panel's initial 480x640
 * SDL_WINDOWEVENT_RESIZED before it has created gui_texture.  Its resize
 * handler then clears new_gui_texture_needed, so every later UpdateTexture
 * and RenderCopy operates on NULL and the DRM plane stays black.
 *
 * Ignore only that first RESIZED notification for the Rockbox executable.
 * SIZE_CHANGED still reaches SDL itself, and all RESIZED notifications after
 * Rockbox has created its streaming GUI texture retain their normal meaning.
 */

static SDL_Texture *(*real_create_texture)(SDL_Renderer *, Uint32, int, int, int);
static int (*real_wait_event)(SDL_Event *);
static int (*real_poll_event)(SDL_Event *);
static int streaming_texture_ready;
static int initial_resize_reported;
static int process_checked;
static int rockbox_process;

#define LOAD_NEXT(variable, symbol)                                           \
    do {                                                                      \
        if (!(variable))                                                      \
            *(void **)(&(variable)) = dlsym(RTLD_NEXT, (symbol));            \
    } while (0)

static int is_rockbox_process(void) {
    char executable[256];
    ssize_t length;
    const char *name;

    if (process_checked)
        return rockbox_process;
    process_checked = 1;

    if (getenv("PLUMOS_PORTMASTER_ROCKBOX_TEST")) {
        rockbox_process = 1;
        return 1;
    }
    length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (length <= 0 || (size_t)length >= sizeof(executable))
        return 0;
    executable[length] = '\0';
    name = strrchr(executable, '/');
    name = name ? name + 1 : executable;
    rockbox_process = strcmp(name, "rockbox") == 0;
    return rockbox_process;
}

static int filter_event(SDL_Event *event, int result) {
    if (result <= 0 || !event || !is_rockbox_process() ||
        streaming_texture_ready)
        return result;
    if (event->type == SDL_WINDOWEVENT &&
        event->window.event == SDL_WINDOWEVENT_RESIZED) {
        if (!initial_resize_reported) {
            fprintf(stderr,
                    "[plumOS] Rockbox compatibility: ignored initial resize "
                    "%dx%d until GUI texture creation\n",
                    event->window.data1, event->window.data2);
            initial_resize_reported = 1;
        }
        event->window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    }
    return result;
}

SDL_Texture *SDL_CreateTexture(SDL_Renderer *renderer, Uint32 format, int access,
                               int width, int height) {
    SDL_Texture *texture;

    LOAD_NEXT(real_create_texture, "SDL_CreateTexture");
    if (!real_create_texture)
        return NULL;
    texture = real_create_texture(renderer, format, access, width, height);
    if (texture && access == SDL_TEXTUREACCESS_STREAMING &&
        is_rockbox_process()) {
        streaming_texture_ready = 1;
        fprintf(stderr,
                "[plumOS] Rockbox compatibility: GUI texture ready %dx%d\n",
                width, height);
    }
    return texture;
}

int SDL_WaitEvent(SDL_Event *event) {
    int result;

    LOAD_NEXT(real_wait_event, "SDL_WaitEvent");
    result = real_wait_event ? real_wait_event(event) : 0;
    return filter_event(event, result);
}

int SDL_PollEvent(SDL_Event *event) {
    int result;

    LOAD_NEXT(real_poll_event, "SDL_PollEvent");
    result = real_poll_event ? real_poll_event(event) : 0;
    return filter_event(event, result);
}
