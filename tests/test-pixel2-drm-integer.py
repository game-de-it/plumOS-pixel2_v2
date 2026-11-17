#!/usr/bin/env python3
"""Exercise the patched DRM viewport functions without a display device."""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / 'output/build/retroarch-pixel2/gfx/drivers/drm_gfx.c').read_text()

def function(name):
    start = source.rfind('static ', 0, source.index(name + '('))
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

frame = function('drm_frame')
frame = frame[frame.index('   /* Settings changes'):frame.index('   if (   (width')]
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
struct video_viewport { int x,y; unsigned width,height,full_width,full_height; };
typedef struct { struct { bool video_force_aspect,video_scale_integer; } bools; } settings_t;
static settings_t settings = {{true, true}};
static settings_t *config_get_ptr(void) { return &settings; }
struct drm_surface { unsigned rotation; int layer; float aspect; struct video_viewport viewport; };
struct drm_video { bool viewport_dirty,menu_active; struct drm_surface *main_surface,*menu_surface; int core_width,core_height; };
static void drm_plane_setup(struct drm_surface *s) { assert(s); }
static void drm_surface_free(void *d,struct drm_surface **s) { (void)d; *s=NULL; }
static struct { unsigned hdisplay,vdisplay; } mode = {480,640};
static struct { typeof(mode) *current_mode; } drm = {&mode};
static void video_viewport_get_scaled_integer_rotation(struct video_viewport *vp,
 unsigned w,unsigned h,float a,bool b,bool c,unsigned rotation) {
 (void)w;(void)h;(void)a;(void)b;(void)c;assert(rotation==0);
 vp->width=512;vp->height=448;vp->x=64;vp->y=16;
}
static void video_viewport_get_scaled_aspect2(struct video_viewport *vp,
 unsigned w,unsigned h,bool a,float b,float c) {
 (void)vp;(void)w;(void)h;(void)a;(void)b;(void)c;
}
'''
harness += function('drm_surface_update_viewport') + '\n'
harness += function('drm_apply_state_changes') + '\n'
harness += function('drm_set_texture_enable') + '\n'
harness += 'static void next_frame(struct drm_video *_drmvars, const void *frame) {\n' + frame + '\n}\n'
harness += r'''
int main(void) {
 struct drm_surface game = {.rotation=3,.layer=0,.aspect=4.0f/3};
 struct drm_surface menu = {.rotation=3,.layer=2,.aspect=4.0f/3};
 struct drm_video vid = {.main_surface=&game,.core_width=256,.core_height=224};
 settings.bools.video_scale_integer=false;
 drm_surface_update_viewport(&game);
 assert(game.viewport.width==640 && game.viewport.height==480);
 settings.bools.video_scale_integer=true;
 drm_surface_update_viewport(&menu);
 assert(menu.viewport.width==640 && menu.viewport.height==480);
 drm_apply_state_changes(&vid);
 assert(vid.viewport_dirty);
 vid.menu_active=true;
 next_frame(&vid,&vid);
 assert(vid.viewport_dirty && vid.core_width==256);
 vid.menu_active=false;
 next_frame(&vid,NULL);
 assert(vid.viewport_dirty && vid.core_width==256);
 next_frame(&vid,&vid);
 assert(!vid.viewport_dirty && vid.core_width==0 && vid.core_height==0);
 drm_surface_update_viewport(&game);
 assert(game.viewport.width==512 && game.viewport.height==448);
 vid.core_width=256; vid.core_height=224;
 settings.bools.video_scale_integer=false;
 drm_apply_state_changes(&vid); next_frame(&vid,&vid);
 assert(vid.core_width==0 && !vid.viewport_dirty);
 vid.main_surface=NULL; vid.menu_active=true; vid.menu_surface=&menu;
 drm_set_texture_enable(&vid,false,false);
 assert(!vid.menu_active && !vid.menu_surface && vid.viewport_dirty);
 puts("drm_integer_menu_test=result-ok");
}
'''
with tempfile.TemporaryDirectory() as temp:
    path = pathlib.Path(temp)
    (path / 'test.c').write_text(harness)
    subprocess.run(['cc', '-std=gnu99', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
