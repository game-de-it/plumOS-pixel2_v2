// SPDX-License-Identifier: MIT
#define _GNU_SOURCE

#include <SDL.h>
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GL_VERTEX_ARRAY_BINDING
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

/*
 * SDL_Renderer ports are rotated by plumos_portmaster_sdl_rotate.  LÖVE and
 * other SDL/OpenGL ports bypass SDL_Renderer, so give them a landscape
 * 640x480 default framebuffer and rotate that framebuffer onto Pixel2's
 * panel-native 480x640 scanout immediately before SDL_GL_SwapWindow.
 */

static SDL_GLContext (*real_sdl_gl_create_context)(SDL_Window *);
static int (*real_sdl_gl_make_current)(SDL_Window *, SDL_GLContext);
static void *(*real_sdl_gl_get_proc_address)(const char *);
static void (*real_sdl_gl_get_drawable_size)(SDL_Window *, int *, int *);
static void (*real_sdl_gl_swap_window)(SDL_Window *);

static void (GL_APIENTRYP real_gl_bind_framebuffer)(GLenum, GLuint);
static void (GL_APIENTRYP real_gl_get_integerv)(GLenum, GLint *);
static const GLubyte *(GL_APIENTRYP real_gl_get_string)(GLenum);
static void (GL_APIENTRYP real_gl_gen_framebuffers)(GLsizei, GLuint *);
static void (GL_APIENTRYP real_gl_delete_framebuffers)(GLsizei, const GLuint *);
static void (GL_APIENTRYP real_gl_framebuffer_texture_2d)(GLenum, GLenum, GLenum,
                                                          GLuint, GLint);
static void (GL_APIENTRYP real_gl_gen_renderbuffers)(GLsizei, GLuint *);
static void (GL_APIENTRYP real_gl_delete_renderbuffers)(GLsizei, const GLuint *);
static void (GL_APIENTRYP real_gl_bind_renderbuffer)(GLenum, GLuint);
static void (GL_APIENTRYP real_gl_renderbuffer_storage)(GLenum, GLenum, GLsizei,
                                                        GLsizei);
static void (GL_APIENTRYP real_gl_framebuffer_renderbuffer)(GLenum, GLenum,
                                                            GLenum, GLuint);
static GLenum (GL_APIENTRYP real_gl_check_framebuffer_status)(GLenum);
static void (GL_APIENTRYP real_gl_gen_textures)(GLsizei, GLuint *);
static void (GL_APIENTRYP real_gl_delete_textures)(GLsizei, const GLuint *);
static void (GL_APIENTRYP real_gl_bind_texture)(GLenum, GLuint);
static void (GL_APIENTRYP real_gl_tex_parameteri)(GLenum, GLenum, GLint);
static void (GL_APIENTRYP real_gl_tex_image_2d)(GLenum, GLint, GLint, GLsizei,
                                                GLsizei, GLint, GLenum, GLenum,
                                                const void *);
static GLuint (GL_APIENTRYP real_gl_create_shader)(GLenum);
static void (GL_APIENTRYP real_gl_shader_source)(GLuint, GLsizei,
                                                  const GLchar *const *,
                                                  const GLint *);
static void (GL_APIENTRYP real_gl_compile_shader)(GLuint);
static void (GL_APIENTRYP real_gl_get_shader_iv)(GLuint, GLenum, GLint *);
static void (GL_APIENTRYP real_gl_get_shader_info_log)(GLuint, GLsizei, GLsizei *,
                                                        GLchar *);
static void (GL_APIENTRYP real_gl_delete_shader)(GLuint);
static GLuint (GL_APIENTRYP real_gl_create_program)(void);
static void (GL_APIENTRYP real_gl_attach_shader)(GLuint, GLuint);
static void (GL_APIENTRYP real_gl_bind_attrib_location)(GLuint, GLuint,
                                                        const GLchar *);
static void (GL_APIENTRYP real_gl_link_program)(GLuint);
static void (GL_APIENTRYP real_gl_get_program_iv)(GLuint, GLenum, GLint *);
static void (GL_APIENTRYP real_gl_get_program_info_log)(GLuint, GLsizei,
                                                         GLsizei *, GLchar *);
static void (GL_APIENTRYP real_gl_delete_program)(GLuint);
static void (GL_APIENTRYP real_gl_use_program)(GLuint);
static GLint (GL_APIENTRYP real_gl_get_uniform_location)(GLuint, const GLchar *);
static void (GL_APIENTRYP real_gl_uniform_1i)(GLint, GLint);
static void (GL_APIENTRYP real_gl_gen_buffers)(GLsizei, GLuint *);
static void (GL_APIENTRYP real_gl_delete_buffers)(GLsizei, const GLuint *);
static void (GL_APIENTRYP real_gl_bind_buffer)(GLenum, GLuint);
static void (GL_APIENTRYP real_gl_buffer_data)(GLenum, GLsizeiptr, const void *,
                                               GLenum);
static void (GL_APIENTRYP real_gl_enable_vertex_attrib_array)(GLuint);
static void (GL_APIENTRYP real_gl_disable_vertex_attrib_array)(GLuint);
static void (GL_APIENTRYP real_gl_vertex_attrib_pointer)(GLuint, GLint, GLenum,
                                                         GLboolean, GLsizei,
                                                         const void *);
static void (GL_APIENTRYP real_gl_active_texture)(GLenum);
static void (GL_APIENTRYP real_gl_get_booleanv)(GLenum, GLboolean *);
static GLboolean (GL_APIENTRYP real_gl_is_enabled)(GLenum);
static void (GL_APIENTRYP real_gl_enable)(GLenum);
static void (GL_APIENTRYP real_gl_disable)(GLenum);
static void (GL_APIENTRYP real_gl_color_mask)(GLboolean, GLboolean, GLboolean,
                                              GLboolean);
static void (GL_APIENTRYP real_gl_viewport)(GLint, GLint, GLsizei, GLsizei);
static void (GL_APIENTRYP real_gl_clear_color)(GLfloat, GLfloat, GLfloat, GLfloat);
static void (GL_APIENTRYP real_gl_clear)(GLbitfield);
static void (GL_APIENTRYP real_gl_draw_arrays)(GLenum, GLint, GLsizei);
static void (GL_APIENTRYP real_gl_gen_vertex_arrays)(GLsizei, GLuint *);
static void (GL_APIENTRYP real_gl_delete_vertex_arrays)(GLsizei, const GLuint *);
static void (GL_APIENTRYP real_gl_bind_vertex_array)(GLuint);

static GLuint logical_fbo;
static GLuint logical_texture;
static GLuint logical_depth_stencil;
static GLuint rotate_program;
static GLuint rotate_vbo;
static GLuint rotate_vao;
static int gl_ready;
static int gl_initialise_attempted;
static int internal_gl;
static unsigned redirected_default_count;
static SDL_GLContext active_context;

#define LOAD_NEXT(name, symbol)                                                \
    do {                                                                       \
        if (!real_##name)                                                      \
            *(void **)(&real_##name) = dlsym(RTLD_NEXT, symbol);              \
    } while (0)

#define LOAD_GL(name, symbol)                                                  \
    do {                                                                       \
        if (!real_##name && real_sdl_gl_get_proc_address)                     \
            *(void **)(&real_##name) = real_sdl_gl_get_proc_address(symbol);  \
    } while (0)

static int rotation_enabled(void) {
    const char *value = getenv("PLUMOS_PORTMASTER_GL_ROTATION");
    return value && strcmp(value, "270") == 0;
}

static void reset_context_state(SDL_GLContext context) {
    active_context = context;
    logical_fbo = 0;
    logical_texture = 0;
    logical_depth_stencil = 0;
    rotate_program = 0;
    rotate_vbo = 0;
    rotate_vao = 0;
    gl_ready = 0;
    gl_initialise_attempted = 0;
    redirected_default_count = 0;
}

static int load_gl(void) {
    LOAD_GL(gl_bind_framebuffer, "glBindFramebuffer");
    LOAD_GL(gl_get_integerv, "glGetIntegerv");
    LOAD_GL(gl_get_string, "glGetString");
    LOAD_GL(gl_gen_framebuffers, "glGenFramebuffers");
    LOAD_GL(gl_delete_framebuffers, "glDeleteFramebuffers");
    LOAD_GL(gl_framebuffer_texture_2d, "glFramebufferTexture2D");
    LOAD_GL(gl_gen_renderbuffers, "glGenRenderbuffers");
    LOAD_GL(gl_delete_renderbuffers, "glDeleteRenderbuffers");
    LOAD_GL(gl_bind_renderbuffer, "glBindRenderbuffer");
    LOAD_GL(gl_renderbuffer_storage, "glRenderbufferStorage");
    LOAD_GL(gl_framebuffer_renderbuffer, "glFramebufferRenderbuffer");
    LOAD_GL(gl_check_framebuffer_status, "glCheckFramebufferStatus");
    LOAD_GL(gl_gen_textures, "glGenTextures");
    LOAD_GL(gl_delete_textures, "glDeleteTextures");
    LOAD_GL(gl_bind_texture, "glBindTexture");
    LOAD_GL(gl_tex_parameteri, "glTexParameteri");
    LOAD_GL(gl_tex_image_2d, "glTexImage2D");
    LOAD_GL(gl_create_shader, "glCreateShader");
    LOAD_GL(gl_shader_source, "glShaderSource");
    LOAD_GL(gl_compile_shader, "glCompileShader");
    LOAD_GL(gl_get_shader_iv, "glGetShaderiv");
    LOAD_GL(gl_get_shader_info_log, "glGetShaderInfoLog");
    LOAD_GL(gl_delete_shader, "glDeleteShader");
    LOAD_GL(gl_create_program, "glCreateProgram");
    LOAD_GL(gl_attach_shader, "glAttachShader");
    LOAD_GL(gl_bind_attrib_location, "glBindAttribLocation");
    LOAD_GL(gl_link_program, "glLinkProgram");
    LOAD_GL(gl_get_program_iv, "glGetProgramiv");
    LOAD_GL(gl_get_program_info_log, "glGetProgramInfoLog");
    LOAD_GL(gl_delete_program, "glDeleteProgram");
    LOAD_GL(gl_use_program, "glUseProgram");
    LOAD_GL(gl_get_uniform_location, "glGetUniformLocation");
    LOAD_GL(gl_uniform_1i, "glUniform1i");
    LOAD_GL(gl_gen_buffers, "glGenBuffers");
    LOAD_GL(gl_delete_buffers, "glDeleteBuffers");
    LOAD_GL(gl_bind_buffer, "glBindBuffer");
    LOAD_GL(gl_buffer_data, "glBufferData");
    LOAD_GL(gl_enable_vertex_attrib_array, "glEnableVertexAttribArray");
    LOAD_GL(gl_disable_vertex_attrib_array, "glDisableVertexAttribArray");
    LOAD_GL(gl_vertex_attrib_pointer, "glVertexAttribPointer");
    LOAD_GL(gl_active_texture, "glActiveTexture");
    LOAD_GL(gl_get_booleanv, "glGetBooleanv");
    LOAD_GL(gl_is_enabled, "glIsEnabled");
    LOAD_GL(gl_enable, "glEnable");
    LOAD_GL(gl_disable, "glDisable");
    LOAD_GL(gl_color_mask, "glColorMask");
    LOAD_GL(gl_viewport, "glViewport");
    LOAD_GL(gl_clear_color, "glClearColor");
    LOAD_GL(gl_clear, "glClear");
    LOAD_GL(gl_draw_arrays, "glDrawArrays");
    LOAD_GL(gl_gen_vertex_arrays, "glGenVertexArrays");
    if (!real_gl_gen_vertex_arrays)
        LOAD_GL(gl_gen_vertex_arrays, "glGenVertexArraysOES");
    LOAD_GL(gl_delete_vertex_arrays, "glDeleteVertexArrays");
    if (!real_gl_delete_vertex_arrays)
        LOAD_GL(gl_delete_vertex_arrays, "glDeleteVertexArraysOES");
    LOAD_GL(gl_bind_vertex_array, "glBindVertexArray");
    if (!real_gl_bind_vertex_array)
        LOAD_GL(gl_bind_vertex_array, "glBindVertexArrayOES");
    return real_gl_bind_framebuffer && real_gl_get_integerv && real_gl_get_string &&
           real_gl_gen_framebuffers && real_gl_framebuffer_texture_2d &&
           real_gl_gen_renderbuffers && real_gl_bind_renderbuffer &&
           real_gl_renderbuffer_storage && real_gl_framebuffer_renderbuffer &&
           real_gl_check_framebuffer_status && real_gl_gen_textures &&
           real_gl_bind_texture && real_gl_tex_parameteri && real_gl_tex_image_2d &&
           real_gl_create_shader && real_gl_shader_source && real_gl_compile_shader &&
           real_gl_get_shader_iv && real_gl_create_program && real_gl_attach_shader &&
           real_gl_bind_attrib_location && real_gl_link_program &&
           real_gl_get_program_iv && real_gl_use_program &&
           real_gl_get_uniform_location && real_gl_uniform_1i &&
           real_gl_gen_buffers && real_gl_bind_buffer && real_gl_buffer_data &&
           real_gl_enable_vertex_attrib_array &&
           real_gl_disable_vertex_attrib_array && real_gl_vertex_attrib_pointer &&
           real_gl_active_texture && real_gl_get_booleanv &&
           real_gl_is_enabled && real_gl_enable && real_gl_disable &&
           real_gl_color_mask && real_gl_viewport && real_gl_clear_color &&
           real_gl_clear && real_gl_draw_arrays && real_gl_gen_vertex_arrays &&
           real_gl_bind_vertex_array;
}

static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = real_gl_create_shader(type);
    GLint ok = 0;
    GLchar log[512];
    GLsizei length = 0;

    if (!shader)
        return 0;
    real_gl_shader_source(shader, 1, &source, NULL);
    real_gl_compile_shader(shader);
    real_gl_get_shader_iv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        if (real_gl_get_shader_info_log) {
            real_gl_get_shader_info_log(shader, (GLsizei)sizeof(log), &length, log);
            fprintf(stderr, "[plumOS] PortMaster GL rotation shader: %.*s\n",
                    (int)length, log);
        }
        if (real_gl_delete_shader)
            real_gl_delete_shader(shader);
        return 0;
    }
    return shader;
}

static int initialise_rotation(void) {
    static const char vertex_source_es[] =
        "#version 100\n"
        "attribute vec2 a_position;\n"
        "attribute vec2 a_texcoord;\n"
        "varying vec2 v_texcoord;\n"
        "void main() { gl_Position = vec4(a_position, 0.0, 1.0);"
        " v_texcoord = a_texcoord; }\n";
    static const char fragment_source_es[] =
        "#version 100\n"
        "precision mediump float;\n"
        "uniform sampler2D u_texture;\n"
        "varying vec2 v_texcoord;\n"
        "void main() { gl_FragColor = texture2D(u_texture, v_texcoord); }\n";
    static const char vertex_source_gl[] =
        "#version 130\n"
        "in vec2 a_position;\n"
        "in vec2 a_texcoord;\n"
        "out vec2 v_texcoord;\n"
        "void main() { gl_Position = vec4(a_position, 0.0, 1.0);"
        " v_texcoord = a_texcoord; }\n";
    static const char fragment_source_gl[] =
        "#version 130\n"
        "uniform sampler2D u_texture;\n"
        "in vec2 v_texcoord;\n"
        "out vec4 frag_color;\n"
        "void main() { frag_color = texture(u_texture, v_texcoord); }\n";
    /* Native scanout is the logical image rotated 90 degrees counter-clockwise. */
    static const GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, 1.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
    };
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLint linked = 0;
    GLint previous_fbo = 0;
    GLint previous_texture = 0;
    GLint previous_buffer = 0;
    GLint previous_vao = 0;
    GLint previous_renderbuffer = 0;
    const char *vertex_source;
    const char *fragment_source;
    const char *gl_version;

    if (gl_ready)
        return 1;
    if (gl_initialise_attempted)
        return 0;
    if (!rotation_enabled() || !load_gl())
        return 0;
    gl_initialise_attempted = 1;
    gl_version = (const char *)real_gl_get_string(GL_VERSION);
    if (gl_version && strstr(gl_version, "OpenGL ES")) {
        vertex_source = vertex_source_es;
        fragment_source = fragment_source_es;
    } else {
        vertex_source = vertex_source_gl;
        fragment_source = fragment_source_gl;
    }
    fprintf(stderr, "[plumOS] PortMaster GL rotation context: %s\n",
            gl_version ? gl_version : "unknown");

    real_gl_get_integerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);
    real_gl_get_integerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    real_gl_get_integerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);
    real_gl_get_integerv(GL_VERTEX_ARRAY_BINDING, &previous_vao);
    real_gl_get_integerv(GL_RENDERBUFFER_BINDING, &previous_renderbuffer);

    internal_gl = 1;
    real_gl_gen_textures(1, &logical_texture);
    real_gl_bind_texture(GL_TEXTURE_2D, logical_texture);
    real_gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    real_gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    real_gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    real_gl_tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    real_gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGBA, 640, 480, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, NULL);
    real_gl_gen_framebuffers(1, &logical_fbo);
    real_gl_bind_framebuffer(GL_FRAMEBUFFER, logical_fbo);
    real_gl_framebuffer_texture_2d(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, logical_texture, 0);
    /*
     * SDL's real default framebuffer includes the depth/stencil storage that
     * LÖVE requests for a window.  Balatro uses stencil operations during its
     * first rendered frame, while PortMaster's patcher does not.  Redirecting
     * framebuffer zero to a colour-only FBO therefore violates the default
     * framebuffer contract and can crash the Panfrost GL path.  Mirror the
     * window contract on the logical framebuffer as packed depth/stencil.
     */
    real_gl_gen_renderbuffers(1, &logical_depth_stencil);
    real_gl_bind_renderbuffer(GL_RENDERBUFFER, logical_depth_stencil);
    real_gl_renderbuffer_storage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 640, 480);
    real_gl_framebuffer_renderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                     GL_RENDERBUFFER, logical_depth_stencil);
    real_gl_framebuffer_renderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                     GL_RENDERBUFFER, logical_depth_stencil);
    if (real_gl_check_framebuffer_status(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[plumOS] PortMaster GL rotation framebuffer incomplete\n");
        goto fail;
    }

    vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex_shader || !fragment_shader)
        goto fail;
    rotate_program = real_gl_create_program();
    real_gl_attach_shader(rotate_program, vertex_shader);
    real_gl_attach_shader(rotate_program, fragment_shader);
    real_gl_bind_attrib_location(rotate_program, 0, "a_position");
    real_gl_bind_attrib_location(rotate_program, 1, "a_texcoord");
    real_gl_link_program(rotate_program);
    real_gl_get_program_iv(rotate_program, GL_LINK_STATUS, &linked);
    if (real_gl_delete_shader) {
        real_gl_delete_shader(vertex_shader);
        real_gl_delete_shader(fragment_shader);
    }
    if (!linked) {
        if (real_gl_get_program_info_log) {
            GLchar log[512];
            GLsizei length = 0;
            real_gl_get_program_info_log(rotate_program, (GLsizei)sizeof(log),
                                         &length, log);
            fprintf(stderr, "[plumOS] PortMaster GL rotation program: %.*s\n",
                    (int)length, log);
        }
        goto fail;
    }
    real_gl_use_program(rotate_program);
    real_gl_uniform_1i(real_gl_get_uniform_location(rotate_program, "u_texture"), 0);
    real_gl_gen_vertex_arrays(1, &rotate_vao);
    real_gl_bind_vertex_array(rotate_vao);
    real_gl_gen_buffers(1, &rotate_vbo);
    real_gl_bind_buffer(GL_ARRAY_BUFFER, rotate_vbo);
    real_gl_buffer_data(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(vertices), vertices,
                        GL_STATIC_DRAW);
    real_gl_enable_vertex_attrib_array(0);
    real_gl_enable_vertex_attrib_array(1);
    real_gl_vertex_attrib_pointer(0, 2, GL_FLOAT, GL_FALSE,
                                  4 * (GLsizei)sizeof(GLfloat), (const void *)0);
    real_gl_vertex_attrib_pointer(1, 2, GL_FLOAT, GL_FALSE,
                                  4 * (GLsizei)sizeof(GLfloat),
                                  (const void *)(2 * sizeof(GLfloat)));

    real_gl_bind_texture(GL_TEXTURE_2D, (GLuint)previous_texture);
    real_gl_bind_buffer(GL_ARRAY_BUFFER, (GLuint)previous_buffer);
    real_gl_bind_vertex_array((GLuint)previous_vao);
    real_gl_bind_renderbuffer(GL_RENDERBUFFER, (GLuint)previous_renderbuffer);
    real_gl_bind_framebuffer(GL_FRAMEBUFFER, logical_fbo);
    internal_gl = 0;
    gl_ready = 1;
    fprintf(stderr, "[plumOS] PortMaster GL rotation: 640x480 -> 480x640 @ 270\n");
    return 1;

fail:
    if (rotate_program && real_gl_delete_program)
        real_gl_delete_program(rotate_program);
    if (rotate_vbo && real_gl_delete_buffers)
        real_gl_delete_buffers(1, &rotate_vbo);
    if (rotate_vao && real_gl_delete_vertex_arrays)
        real_gl_delete_vertex_arrays(1, &rotate_vao);
    if (logical_fbo && real_gl_delete_framebuffers)
        real_gl_delete_framebuffers(1, &logical_fbo);
    if (logical_depth_stencil && real_gl_delete_renderbuffers)
        real_gl_delete_renderbuffers(1, &logical_depth_stencil);
    if (logical_texture && real_gl_delete_textures)
        real_gl_delete_textures(1, &logical_texture);
    rotate_program = rotate_vbo = rotate_vao = logical_fbo = logical_texture = 0;
    logical_depth_stencil = 0;
    real_gl_bind_texture(GL_TEXTURE_2D, (GLuint)previous_texture);
    real_gl_bind_buffer(GL_ARRAY_BUFFER, (GLuint)previous_buffer);
    real_gl_bind_vertex_array((GLuint)previous_vao);
    real_gl_bind_renderbuffer(GL_RENDERBUFFER, (GLuint)previous_renderbuffer);
    real_gl_bind_framebuffer(GL_FRAMEBUFFER, (GLuint)previous_fbo);
    internal_gl = 0;
    return 0;
}

GL_APICALL void GL_APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer) {
    LOAD_NEXT(gl_bind_framebuffer, "glBindFramebuffer");
    if (!real_gl_bind_framebuffer)
        return;
    if (!internal_gl && framebuffer == 0 && initialise_rotation()) {
        framebuffer = logical_fbo;
        if (redirected_default_count++ == 0)
            fprintf(stderr, "[plumOS] PortMaster GL rotation: redirected default framebuffer\n");
    }
    real_gl_bind_framebuffer(target, framebuffer);
}

GL_APICALL void GL_APIENTRY glGetIntegerv(GLenum pname, GLint *data) {
    LOAD_NEXT(gl_get_integerv, "glGetIntegerv");
    if (!real_gl_get_integerv)
        return;
    real_gl_get_integerv(pname, data);
    if (!internal_gl && gl_ready && pname == GL_FRAMEBUFFER_BINDING && data &&
        *data == (GLint)logical_fbo)
        *data = 0;
}

static int present_rotation(SDL_Window *window) {
    GLint framebuffer = 0;
    GLint program = 0;
    GLint buffer = 0;
    GLint active_texture = 0;
    GLint texture = 0;
    GLint texture_zero = 0;
    GLint vertex_array = 0;
    GLint viewport[4] = {0, 0, 480, 640};
    GLboolean blend;
    GLboolean cull;
    GLboolean depth;
    GLboolean scissor;
    GLboolean color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};

    if (!initialise_rotation())
        return 0;
    real_gl_get_integerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    real_gl_get_integerv(GL_CURRENT_PROGRAM, &program);
    real_gl_get_integerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    real_gl_get_integerv(GL_ACTIVE_TEXTURE, &active_texture);
    real_gl_get_integerv(GL_TEXTURE_BINDING_2D, &texture);
    real_gl_get_integerv(GL_VERTEX_ARRAY_BINDING, &vertex_array);
    real_gl_get_integerv(GL_VIEWPORT, viewport);
    real_gl_get_booleanv(GL_COLOR_WRITEMASK, color_mask);
    blend = real_gl_is_enabled(GL_BLEND);
    cull = real_gl_is_enabled(GL_CULL_FACE);
    depth = real_gl_is_enabled(GL_DEPTH_TEST);
    scissor = real_gl_is_enabled(GL_SCISSOR_TEST);

    internal_gl = 1;
    real_gl_bind_framebuffer(GL_FRAMEBUFFER, 0);
    real_gl_viewport(0, 0, 480, 640);
    real_gl_disable(GL_BLEND);
    real_gl_disable(GL_CULL_FACE);
    real_gl_disable(GL_DEPTH_TEST);
    real_gl_disable(GL_SCISSOR_TEST);
    /* LÖVE may leave colour writes disabled after a stencil pass.  The
     * physical presenter must always write all four channels; otherwise the
     * previous KMS buffer is exposed for one swap and appears as flicker. */
    real_gl_color_mask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    real_gl_use_program(rotate_program);
    real_gl_active_texture(GL_TEXTURE0);
    real_gl_get_integerv(GL_TEXTURE_BINDING_2D, &texture_zero);
    real_gl_bind_texture(GL_TEXTURE_2D, logical_texture);
    real_gl_bind_vertex_array(rotate_vao);
    real_gl_bind_buffer(GL_ARRAY_BUFFER, rotate_vbo);
    real_gl_draw_arrays(GL_TRIANGLE_STRIP, 0, 4);

    /* KMSDRM must swap while the real default framebuffer is still bound. */
    real_sdl_gl_swap_window(window);

    real_gl_bind_framebuffer(GL_FRAMEBUFFER, (GLuint)framebuffer);
    real_gl_use_program((GLuint)program);
    real_gl_bind_vertex_array((GLuint)vertex_array);
    real_gl_active_texture(GL_TEXTURE0);
    real_gl_bind_texture(GL_TEXTURE_2D, (GLuint)texture_zero);
    real_gl_active_texture((GLenum)active_texture);
    real_gl_bind_texture(GL_TEXTURE_2D, (GLuint)texture);
    real_gl_bind_buffer(GL_ARRAY_BUFFER, (GLuint)buffer);
    real_gl_viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    if (blend) real_gl_enable(GL_BLEND); else real_gl_disable(GL_BLEND);
    if (cull) real_gl_enable(GL_CULL_FACE); else real_gl_disable(GL_CULL_FACE);
    if (depth) real_gl_enable(GL_DEPTH_TEST); else real_gl_disable(GL_DEPTH_TEST);
    if (scissor) real_gl_enable(GL_SCISSOR_TEST); else real_gl_disable(GL_SCISSOR_TEST);
    real_gl_color_mask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
    internal_gl = 0;
    return 1;
}

SDL_GLContext SDL_GL_CreateContext(SDL_Window *window) {
    SDL_GLContext context;

    LOAD_NEXT(sdl_gl_create_context, "SDL_GL_CreateContext");
    if (!real_sdl_gl_create_context)
        return NULL;
    context = real_sdl_gl_create_context(window);
    if (context && rotation_enabled())
        reset_context_state(context);
    return context;
}

int SDL_GL_MakeCurrent(SDL_Window *window, SDL_GLContext context) {
    int result;

    LOAD_NEXT(sdl_gl_make_current, "SDL_GL_MakeCurrent");
    if (!real_sdl_gl_make_current)
        return -1;
    result = real_sdl_gl_make_current(window, context);
    if (result == 0 && rotation_enabled() && context != active_context)
        reset_context_state(context);
    return result;
}

void *SDL_GL_GetProcAddress(const char *name) {
    LOAD_NEXT(sdl_gl_get_proc_address, "SDL_GL_GetProcAddress");
    if (rotation_enabled() && name) {
        if (strcmp(name, "glBindFramebuffer") == 0 ||
            strcmp(name, "glBindFramebufferOES") == 0 ||
            strcmp(name, "glBindFramebufferEXT") == 0 ||
            strcmp(name, "glBindFramebufferARB") == 0)
            return (void *)&glBindFramebuffer;
        if (strcmp(name, "glGetIntegerv") == 0)
            return (void *)&glGetIntegerv;
    }
    return real_sdl_gl_get_proc_address ? real_sdl_gl_get_proc_address(name) : NULL;
}

void SDL_GL_GetDrawableSize(SDL_Window *window, int *width, int *height) {
    LOAD_NEXT(sdl_gl_get_drawable_size, "SDL_GL_GetDrawableSize");
    if (real_sdl_gl_get_drawable_size)
        real_sdl_gl_get_drawable_size(window, width, height);
    if (!rotation_enabled())
        return;
    initialise_rotation();
    if (width)
        *width = 640;
    if (height)
        *height = 480;
}

void SDL_GL_SwapWindow(SDL_Window *window) {
    LOAD_NEXT(sdl_gl_swap_window, "SDL_GL_SwapWindow");
    if (!real_sdl_gl_swap_window)
        return;
    if (rotation_enabled() && present_rotation(window))
        return;
    real_sdl_gl_swap_window(window);
    if (gl_ready) {
        internal_gl = 1;
        real_gl_bind_framebuffer(GL_FRAMEBUFFER, logical_fbo);
        internal_gl = 0;
    }
}
