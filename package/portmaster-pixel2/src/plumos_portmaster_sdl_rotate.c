// SPDX-License-Identifier: MIT
#define _GNU_SOURCE

#include <SDL.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Pixel2's DRM mode is the panel-native 480x640 mode even though the handheld
 * is used as a 640x480 landscape device.  PortMaster games using SDL_Renderer
 * therefore need the same final quarter-turn as the PortMaster GUI adapter.
 *
 * Keep the game's logical renderer unchanged by drawing it into a target
 * texture.  At present time, copy that texture to the native scanout with a
 * 270-degree turn.  NULL render targets remain NULL from the application's
 * point of view, so games using intermediate target textures keep their SDL
 * semantics.
 */

#define MAX_RENDERERS 8

struct renderer_state {
    SDL_Renderer *renderer;
    SDL_Texture *frame;
    int active;
};

static struct renderer_state states[MAX_RENDERERS];
static int internal_render;

static SDL_Renderer *(*real_create_renderer)(SDL_Window *, int, Uint32);
static const char *(*real_get_error)(void);
static void (*real_destroy_renderer)(SDL_Renderer *);
static SDL_Texture *(*real_create_texture)(SDL_Renderer *, Uint32, int, int, int);
static void (*real_destroy_texture)(SDL_Texture *);
static int (*real_set_texture_blend_mode)(SDL_Texture *, SDL_BlendMode);
static int (*real_set_render_target)(SDL_Renderer *, SDL_Texture *);
static SDL_Texture *(*real_get_render_target)(SDL_Renderer *);
static int (*real_get_renderer_output_size)(SDL_Renderer *, int *, int *);
static int (*real_render_set_logical_size)(SDL_Renderer *, int, int);
static void (*real_render_get_logical_size)(SDL_Renderer *, int *, int *);
static int (*real_render_set_scale)(SDL_Renderer *, float, float);
static void (*real_render_get_scale)(SDL_Renderer *, float *, float *);
static void (*real_render_get_viewport)(SDL_Renderer *, SDL_Rect *);
static int (*real_render_set_viewport)(SDL_Renderer *, const SDL_Rect *);
static void (*real_render_get_clip_rect)(SDL_Renderer *, SDL_Rect *);
static SDL_bool (*real_render_is_clip_enabled)(SDL_Renderer *);
static int (*real_render_set_clip_rect)(SDL_Renderer *, const SDL_Rect *);
static int (*real_set_render_draw_color)(SDL_Renderer *, Uint8, Uint8, Uint8, Uint8);
static int (*real_render_clear)(SDL_Renderer *);
static int (*real_render_copy_ex)(SDL_Renderer *, SDL_Texture *, const SDL_Rect *,
                                  const SDL_Rect *, double, const SDL_Point *,
                                  SDL_RendererFlip);
static void (*real_render_present)(SDL_Renderer *);

#define LOAD(name, symbol)                                                     \
    do {                                                                       \
        if (!real_##name)                                                      \
            *(void **)(&real_##name) = dlsym(RTLD_NEXT, "SDL_" #symbol);     \
    } while (0)

static int rotation_enabled(void) {
    const char *value = getenv("PLUMOS_PORTMASTER_SDL_ROTATION");
    return value && strcmp(value, "270") == 0;
}

static const char *sdl_error(void) {
    LOAD(get_error, GetError);
    return real_get_error ? real_get_error() : "unknown SDL error";
}

static struct renderer_state *find_state(SDL_Renderer *renderer) {
    size_t index;

    for (index = 0; index < MAX_RENDERERS; ++index) {
        if (states[index].active && states[index].renderer == renderer)
            return &states[index];
    }
    return NULL;
}

static struct renderer_state *allocate_state(void) {
    size_t index;

    for (index = 0; index < MAX_RENDERERS; ++index) {
        if (!states[index].active)
            return &states[index];
    }
    return NULL;
}

SDL_Renderer *SDL_CreateRenderer(SDL_Window *window, int index, Uint32 flags) {
    SDL_Renderer *renderer;
    SDL_Texture *frame;
    struct renderer_state *state;

    LOAD(create_renderer, CreateRenderer);
    if (!real_create_renderer)
        return NULL;
    renderer = real_create_renderer(window, index, flags);
    if (!renderer || !rotation_enabled())
        return renderer;

    LOAD(create_texture, CreateTexture);
    LOAD(destroy_texture, DestroyTexture);
    LOAD(set_texture_blend_mode, SetTextureBlendMode);
    LOAD(set_render_target, SetRenderTarget);
    if (!real_create_texture || !real_destroy_texture || !real_set_render_target)
        return renderer;

    state = allocate_state();
    if (!state) {
        fprintf(stderr, "[plumOS] PortMaster SDL rotation: renderer limit reached\n");
        return renderer;
    }
    frame = real_create_texture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_TARGET, 640, 480);
    if (!frame) {
        fprintf(stderr, "[plumOS] PortMaster SDL rotation: target unavailable: %s\n",
                sdl_error());
        return renderer;
    }
    if (real_set_texture_blend_mode)
        real_set_texture_blend_mode(frame, SDL_BLENDMODE_NONE);
    if (real_set_render_target(renderer, frame) != 0) {
        fprintf(stderr, "[plumOS] PortMaster SDL rotation: target rejected: %s\n",
                sdl_error());
        real_destroy_texture(frame);
        return renderer;
    }

    state->renderer = renderer;
    state->frame = frame;
    state->active = 1;
    fprintf(stderr, "[plumOS] PortMaster SDL rotation: 640x480 -> 480x640 @ 270\n");
    return renderer;
}

void SDL_DestroyRenderer(SDL_Renderer *renderer) {
    struct renderer_state *state = find_state(renderer);

    LOAD(destroy_renderer, DestroyRenderer);
    LOAD(destroy_texture, DestroyTexture);
    LOAD(set_render_target, SetRenderTarget);
    if (state) {
        internal_render = 1;
        if (real_set_render_target)
            real_set_render_target(renderer, NULL);
        if (real_destroy_texture)
            real_destroy_texture(state->frame);
        memset(state, 0, sizeof(*state));
        internal_render = 0;
    }
    if (real_destroy_renderer)
        real_destroy_renderer(renderer);
}

int SDL_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture) {
    struct renderer_state *state;

    LOAD(set_render_target, SetRenderTarget);
    if (!real_set_render_target)
        return -1;
    state = find_state(renderer);
    if (state && !internal_render && texture == NULL)
        texture = state->frame;
    return real_set_render_target(renderer, texture);
}

SDL_Texture *SDL_GetRenderTarget(SDL_Renderer *renderer) {
    SDL_Texture *target;
    struct renderer_state *state;

    LOAD(get_render_target, GetRenderTarget);
    if (!real_get_render_target)
        return NULL;
    target = real_get_render_target(renderer);
    state = find_state(renderer);
    if (state && !internal_render && target == state->frame)
        return NULL;
    return target;
}

void SDL_RenderPresent(SDL_Renderer *renderer) {
    struct renderer_state *state = find_state(renderer);
    SDL_Texture *target;
    SDL_Rect viewport = { 0, 0, 0, 0 };
    SDL_Rect clip = { 0, 0, 0, 0 };
    SDL_Rect destination;
    int logical_width = 0;
    int logical_height = 0;
    int output_width = 0;
    int output_height = 0;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    SDL_bool clip_enabled = SDL_FALSE;

    LOAD(render_present, RenderPresent);
    if (!real_render_present)
        return;
    if (!state || internal_render) {
        real_render_present(renderer);
        return;
    }

    LOAD(get_render_target, GetRenderTarget);
    LOAD(get_renderer_output_size, GetRendererOutputSize);
    LOAD(render_set_logical_size, RenderSetLogicalSize);
    LOAD(render_get_logical_size, RenderGetLogicalSize);
    LOAD(render_set_scale, RenderSetScale);
    LOAD(render_get_scale, RenderGetScale);
    LOAD(render_get_viewport, RenderGetViewport);
    LOAD(render_set_viewport, RenderSetViewport);
    LOAD(render_get_clip_rect, RenderGetClipRect);
    LOAD(render_is_clip_enabled, RenderIsClipEnabled);
    LOAD(render_set_clip_rect, RenderSetClipRect);
    LOAD(set_render_draw_color, SetRenderDrawColor);
    LOAD(render_clear, RenderClear);
    LOAD(render_copy_ex, RenderCopyEx);
    LOAD(set_render_target, SetRenderTarget);
    if (!real_get_render_target || !real_get_renderer_output_size ||
        !real_render_set_logical_size || !real_render_get_logical_size ||
        !real_render_set_scale || !real_render_get_scale ||
        !real_render_get_viewport || !real_render_set_viewport ||
        !real_render_get_clip_rect || !real_render_is_clip_enabled ||
        !real_render_set_clip_rect || !real_set_render_draw_color ||
        !real_render_clear || !real_render_copy_ex || !real_set_render_target) {
        real_render_present(renderer);
        return;
    }

    target = real_get_render_target(renderer);
    if (target != state->frame) {
        real_render_present(renderer);
        return;
    }
    if (real_get_renderer_output_size(renderer, &output_width, &output_height) != 0 ||
        output_width < 1 || output_height < 1) {
        real_render_present(renderer);
        return;
    }

    real_render_get_logical_size(renderer, &logical_width, &logical_height);
    real_render_get_scale(renderer, &scale_x, &scale_y);
    real_render_get_viewport(renderer, &viewport);
    clip_enabled = real_render_is_clip_enabled(renderer);
    real_render_get_clip_rect(renderer, &clip);

    internal_render = 1;
    real_set_render_target(renderer, NULL);
    real_render_set_logical_size(renderer, 0, 0);
    real_render_set_scale(renderer, 1.0f, 1.0f);
    real_render_set_viewport(renderer, NULL);
    real_render_set_clip_rect(renderer, NULL);
    real_set_render_draw_color(renderer, 0, 0, 0, 255);
    real_render_clear(renderer);

    destination.x = (output_width - output_height) / 2;
    destination.y = (output_height - output_width) / 2;
    destination.w = output_height;
    destination.h = output_width;
    real_render_copy_ex(renderer, state->frame, NULL, &destination, 270.0, NULL,
                        SDL_FLIP_NONE);
    real_render_present(renderer);

    real_set_render_target(renderer, state->frame);
    real_render_set_logical_size(renderer, logical_width, logical_height);
    real_render_set_scale(renderer, scale_x, scale_y);
    real_render_set_viewport(renderer, &viewport);
    real_render_set_clip_rect(renderer, clip_enabled ? &clip : NULL);
    internal_render = 0;
}
