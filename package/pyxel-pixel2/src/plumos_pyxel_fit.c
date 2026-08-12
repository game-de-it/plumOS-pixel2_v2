#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int GLuint;
typedef int GLint;
typedef void *(*SDLGLGetProcAddress)(const char *name);
typedef void (*GLUseProgram)(GLuint program);
typedef GLint (*GLGetUniformLocation)(GLuint program, const char *name);
typedef void (*GLUniform1f)(GLint location, float value);
typedef void (*GLUniform2f)(GLint location, float x, float y);

enum { MAX_PROGRAMS = 32 };

struct program_state {
    GLuint program;
    GLint screen_pos;
    GLint screen_size;
    GLint screen_scale;
    int has_source_size;
    float fit_factor;
};

static SDLGLGetProcAddress real_sdl_gl_get_proc_address;
static GLUseProgram real_gl_use_program;
static GLGetUniformLocation real_gl_get_uniform_location;
static GLUniform1f real_gl_uniform_1f;
static GLUniform2f real_gl_uniform_2f;
static struct program_state programs[MAX_PROGRAMS];
static GLuint current_program;
static int fit_enabled = -1;
static float output_width = 640.0f;
static float output_height = 480.0f;
static int fit_reported;

static int env_enabled(const char *name, int default_value)
{
    const char *value = getenv(name);

    if (!value || !value[0]) {
        return default_value;
    }
    return strcmp(value, "0") != 0 && strcasecmp(value, "false") != 0 &&
           strcasecmp(value, "no") != 0;
}

static float env_dimension(const char *name, float default_value)
{
    const char *value = getenv(name);
    char *end = NULL;
    float parsed;

    if (!value || !value[0]) {
        return default_value;
    }
    parsed = strtof(value, &end);
    return end != value && parsed > 0.0f ? parsed : default_value;
}

static void init_config(void)
{
    if (fit_enabled >= 0) {
        return;
    }
    fit_enabled = env_enabled("PLUMOS_PYXEL_FIT", 1);
    output_width = env_dimension("PLUMOS_PYXEL_FIT_WIDTH", 640.0f);
    output_height = env_dimension("PLUMOS_PYXEL_FIT_HEIGHT", 480.0f);
}

static struct program_state *program_state(GLuint program, int create)
{
    struct program_state *free_slot = NULL;
    int i;

    for (i = 0; i < MAX_PROGRAMS; ++i) {
        if (programs[i].program == program) {
            return &programs[i];
        }
        if (!programs[i].program && !free_slot) {
            free_slot = &programs[i];
        }
    }
    if (!create || !free_slot || !program) {
        return NULL;
    }
    free_slot->program = program;
    free_slot->screen_pos = -1;
    free_slot->screen_size = -1;
    free_slot->screen_scale = -1;
    return free_slot;
}

static void fit_gl_use_program(GLuint program)
{
    current_program = program;
    real_gl_use_program(program);
}

static GLint fit_gl_get_uniform_location(GLuint program, const char *name)
{
    struct program_state *state;
    GLint location = real_gl_get_uniform_location(program, name);

    if (!name || location < 0) {
        return location;
    }
    state = program_state(program, 1);
    if (!state) {
        return location;
    }
    if (strcmp(name, "u_screenPos") == 0) {
        state->screen_pos = location;
    } else if (strcmp(name, "u_screenSize") == 0) {
        state->screen_size = location;
    } else if (strcmp(name, "u_screenScale") == 0) {
        state->screen_scale = location;
    }
    return location;
}

static void fit_gl_uniform_2f(GLint location, float x, float y)
{
    struct program_state *state = program_state(current_program, 0);

    if (fit_enabled && state && location == state->screen_size) {
        float factor = 1.0f;
        float fitted_width;
        float fitted_height;
        float fitted_x;
        float fitted_y;

        state->has_source_size = 1;
        if (x > output_width) {
            factor = output_width / x;
        }
        if (y * factor > output_height) {
            factor = output_height / y;
        }
        state->fit_factor = factor;
        if (factor < 1.0f - 0.0001f && state->screen_pos >= 0) {
            fitted_width = x * factor;
            fitted_height = y * factor;
            fitted_x = (output_width - fitted_width) * 0.5f;
            fitted_y = (output_height - fitted_height) * 0.5f;
            if (!fit_reported) {
                fprintf(stderr,
                        "plumos-pyxel-fit: source=%.0fx%.0f output=%.0fx%.0f "
                        "factor=%.6f offset=%.1f,%.1f\n",
                        x, y, fitted_width, fitted_height, factor, fitted_x,
                        fitted_y);
                fit_reported = 1;
            }
            real_gl_uniform_2f(state->screen_pos, fitted_x, fitted_y);
            real_gl_uniform_2f(location, fitted_width, fitted_height);
            return;
        }
    }
    real_gl_uniform_2f(location, x, y);
}

static void fit_gl_uniform_1f(GLint location, float value)
{
    struct program_state *state = program_state(current_program, 0);
    if (!fit_enabled || !state || location != state->screen_scale ||
        !state->has_source_size || state->fit_factor >= 1.0f - 0.0001f ||
        state->fit_factor <= 0.0f || value <= 0.0f) {
        real_gl_uniform_1f(location, value);
        return;
    }
    real_gl_uniform_1f(location, value * state->fit_factor);
}

void *SDL_GL_GetProcAddress(const char *name)
{
    void *address;

    init_config();
    if (!real_sdl_gl_get_proc_address) {
        real_sdl_gl_get_proc_address =
            (SDLGLGetProcAddress)dlsym(RTLD_NEXT, "SDL_GL_GetProcAddress");
        if (!real_sdl_gl_get_proc_address) {
            return NULL;
        }
    }
    address = real_sdl_gl_get_proc_address(name);
    if (!fit_enabled || !name) {
        return address;
    }
    if (strcmp(name, "glUseProgram") == 0) {
        real_gl_use_program = (GLUseProgram)address;
        return (void *)fit_gl_use_program;
    }
    if (strcmp(name, "glGetUniformLocation") == 0) {
        real_gl_get_uniform_location = (GLGetUniformLocation)address;
        return (void *)fit_gl_get_uniform_location;
    }
    if (strcmp(name, "glUniform1f") == 0) {
        real_gl_uniform_1f = (GLUniform1f)address;
        return (void *)fit_gl_uniform_1f;
    }
    if (strcmp(name, "glUniform2f") == 0) {
        real_gl_uniform_2f = (GLUniform2f)address;
        return (void *)fit_gl_uniform_2f;
    }
    return address;
}
