#!/usr/bin/env python3
"""Compare actual common integer maths and GL viewport at Pixel2 rotation."""
import pathlib, re, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]/'output/build/retroarch-pixel2'
def function(path, name):
    text=(root/path).read_text()
    m=re.search(r'(?:static )?void '+name+r'\([^;{]*\)\s*\{',text)
    assert m,name
    depth=1; end=m.end()
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[m.start():end]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define ASPECT_RATIO_CUSTOM 23
enum { VIDEO_SCALE_INTEGER_AXIS_Y,VIDEO_SCALE_INTEGER_AXIS_Y_X,
 VIDEO_SCALE_INTEGER_AXIS_Y_XHALF,VIDEO_SCALE_INTEGER_AXIS_YHALF_XHALF,
 VIDEO_SCALE_INTEGER_AXIS_X,VIDEO_SCALE_INTEGER_AXIS_XHALF };
enum { VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE,VIDEO_SCALE_INTEGER_SCALING_OVERSCALE,
 VIDEO_SCALE_INTEGER_SCALING_SMART };
struct video_viewport { int x,y; unsigned width,height,full_width,full_height; };
typedef struct video_viewport video_viewport_t;
typedef struct { struct { unsigned video_aspect_ratio_idx,video_scale_integer_scaling,video_scale_integer_axis; } uints;
 struct { float video_vp_bias_x,video_vp_bias_y; } floats; struct video_viewport video_vp_custom; } settings_t;
static settings_t settings={.floats={.5,.5}};
static settings_t *config_get_ptr(void) { return &settings; }
typedef struct { unsigned frame_cache_width,frame_cache_height,scale_width,scale_height;
 struct { struct { unsigned base_width,base_height; } geometry; } av_info; } video_driver_state_t;
static video_driver_state_t video_driver_st={.frame_cache_width=256,.frame_cache_height=224,.av_info={{256,224}}};
static float aspect=224.0f/ (224.0f*8.0f/7.0f*8.0f/7.0f);
static unsigned retroarch_get_rotation(void) { return 1; }
static void video_viewport_get_scaled_aspect2(struct video_viewport *,unsigned,unsigned,bool,float,float);
void video_viewport_get_scaled_integer_rotation(struct video_viewport *,unsigned,unsigned,float,bool,bool,unsigned);
'''
code+=function('gfx/video_driver.c','video_viewport_get_scaled_integer_rotation')+'\n'
code+=function('gfx/video_driver.c','video_viewport_get_scaled_integer')+'\n'
code+=r'''
#define GL2_FLAG_KEEP_ASPECT 1
static int default_ortho;
typedef struct { struct video_viewport vp; unsigned flags,rotation,video_width,video_height,out_vp_width,out_vp_height;
 struct { float (*translate_aspect)(void *,unsigned,unsigned); } *ctx_driver; void *ctx_data; } gl2_t;
static bool gl2_menu_display_rotation(void) { return true; }
static float video_driver_get_aspect_ratio(void) { return aspect; }
static void glViewport(int x,int y,unsigned w,unsigned h) { (void)x;(void)y;(void)w;(void)h; }
static void gl2_set_projection(gl2_t *g,void *p,bool b) { (void)g;(void)p;(void)b; }
static void video_viewport_get_scaled_aspect2(struct video_viewport *vp,unsigned w,unsigned h,bool y,float d,float a) {
 (void)vp;(void)w;(void)h;(void)y;(void)d;(void)a;
}
'''
code+=function('gfx/drivers/gl2.c','gl2_set_viewport')+'\n'
code+=r'''
int main(void) {
 struct video_viewport logical={0};
 gl2_t gl={.rotation=90,.flags=GL2_FLAG_KEEP_ASPECT,.video_width=480,.video_height=640};
 typeof(*gl.ctx_driver) ctx={0}; gl.ctx_driver=&ctx;
 setenv("PLUMOS_GL_MENU_ROTATION","display",1);
 aspect=1.0f/ (256.0f/224.0f * 8.0f/7.0f);
 video_viewport_get_scaled_integer_rotation(&logical,640,480,1.0f/aspect,true,false,0);
 assert(logical.height==448);
 gl2_set_viewport(&gl,480,640,false,true,true);
 assert(gl.vp.height==logical.width && gl.vp.width==logical.height);
 assert(gl.vp.x==480-logical.y-(int)logical.height && gl.vp.y==logical.x);
 printf("NES Y: logical=%ux%u GL=%ux%u\n",logical.width,logical.height,gl.vp.width,gl.vp.height);
 settings.uints.video_scale_integer_axis=VIDEO_SCALE_INTEGER_AXIS_Y_X;
 video_viewport_get_scaled_integer_rotation(&logical,640,480,1.0f/aspect,true,false,0);
 gl2_set_viewport(&gl,480,640,false,true,true);
 assert(logical.width%256==0 && logical.height%224==0);
 assert(gl.vp.height==logical.width && gl.vp.width==logical.height);
 gl.rotation=270;gl2_set_viewport(&gl,480,640,false,true,true);
 assert(gl.vp.x==logical.y && gl.vp.y==640-logical.x-(int)logical.width);
 gl2_set_viewport(&gl,640,480,true,true,true);
 assert(gl.vp.width==640 && gl.vp.height==480);
 puts("integer_source_coordinates_test=result-ok");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path=pathlib.Path(tmp); (path/'test.c').write_text(code)
    subprocess.run(['cc','-std=gnu99','-Wall','-Wextra','-Werror',str(path/'test.c'),'-lm','-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
