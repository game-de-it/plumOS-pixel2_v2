#ifndef PLUMOS_FBDEV_RENDERER_H
#define PLUMOS_FBDEV_RENDERER_H

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef PLUMOS_FBDEV_ENABLE_DRM
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#endif

#ifdef PLUMOS_FBDEV_ENABLE_PNG
#include <png.h>
#endif

#ifdef PLUMOS_FBDEV_ENABLE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#ifndef PLUMOS_FBDEV_RENDER_LINE_MAX
#define PLUMOS_FBDEV_RENDER_LINE_MAX 512
#endif

#ifndef PLUMOS_FBDEV_SETTING_FLASH_MARKER
#define PLUMOS_FBDEV_SETTING_FLASH_MARKER "@{F:"
#endif

#ifndef PLUMOS_FBDEV_RENDERER_GLYPHS_ONLY
#ifdef PLUMOS_FBDEV_ENABLE_FREETYPE
#ifndef PLUMOS_FBDEV_FT_ADVANCE_CACHE_SIZE
#define PLUMOS_FBDEV_FT_ADVANCE_CACHE_SIZE 512
#endif
#ifndef PLUMOS_FBDEV_FT_GLYPH_CACHE_SIZE
#define PLUMOS_FBDEV_FT_GLYPH_CACHE_SIZE 256
#endif

struct plumos_fbdev_ft_advance_cache_entry {
  unsigned int codepoint;
  int scale;
  int advance;
  int valid;
};

struct plumos_fbdev_ft_glyph_cache_entry {
  unsigned int codepoint;
  int scale;
  int face_slot;
  int pixel_size;
  int bitmap_left;
  int bitmap_top;
  int width;
  int rows;
  unsigned char *alpha;
  unsigned long used_at;
  int valid;
};
#endif

#ifdef PLUMOS_FBDEV_ENABLE_PNG
#ifndef PLUMOS_FBDEV_PNG_CACHE_SLOTS
#define PLUMOS_FBDEV_PNG_CACHE_SLOTS 16
#endif

struct plumos_fbdev_png_cache_slot {
  char path[PLUMOS_FBDEV_RENDER_LINE_MAX];
  unsigned char *pixels;
  int width;
  int height;
  unsigned long last_used;
};
#endif

struct plumos_fbdev_renderer {
  int fd;
  unsigned char *mem;
  unsigned char *shadow;
  size_t map_size;
  int bytes_per_pixel;
  int rotation;
  uint32_t physical_xres;
  uint32_t physical_yres;
  long active_offset;
  long visible_offset;
  long frame_bytes;
  long long marquee_focus_ms;
  uint32_t visible_yoffset;
  uint32_t draw_yoffset;
  int double_buffer;
  struct fb_var_screeninfo var;
  struct fb_fix_screeninfo fix;
#ifdef PLUMOS_FBDEV_ENABLE_DRM
  int drm_fd;
  int drm_active;
  int drm_page_flip_pending;
  uint32_t drm_connector_id;
  uint32_t drm_crtc_id;
  uint32_t drm_fb_id[2];
  uint32_t drm_handle[2];
  uint32_t drm_pitch[2];
  uint64_t drm_size[2];
  unsigned char *drm_map[2];
  int drm_front;
  int drm_draw;
  drmModeModeInfo drm_mode;
  drmModeCrtc *drm_saved_crtc;
#endif
#ifdef PLUMOS_FBDEV_ENABLE_PNG
  struct plumos_fbdev_png_cache_slot png_cache[PLUMOS_FBDEV_PNG_CACHE_SLOTS];
  unsigned long png_cache_tick;
#endif
#ifdef PLUMOS_FBDEV_ENABLE_FREETYPE
  FT_Library ft_library;
  FT_Face ft_face;
  FT_Face ft_fallback_face;
  int ft_ready;
  int ft_pixel_size;
  int ft_fallback_ready;
  int ft_fallback_pixel_size;
  struct plumos_fbdev_ft_advance_cache_entry
      ft_advance_cache[PLUMOS_FBDEV_FT_ADVANCE_CACHE_SIZE];
  struct plumos_fbdev_ft_glyph_cache_entry
      ft_glyph_cache[PLUMOS_FBDEV_FT_GLYPH_CACHE_SIZE];
  unsigned long ft_glyph_cache_tick;
#endif
};

#ifdef PLUMOS_FBDEV_ENABLE_DRM
static void plumos_fbdev_drm_page_flip_handler(
    int fd, unsigned int sequence, unsigned int tv_sec, unsigned int tv_usec,
    void *user_data) {
  struct plumos_fbdev_renderer *r =
      (struct plumos_fbdev_renderer *)user_data;
  (void)fd;
  (void)sequence;
  (void)tv_sec;
  (void)tv_usec;
  if (r) {
    r->drm_page_flip_pending = 0;
  }
}

static void plumos_fbdev_drm_destroy_buffer(
    struct plumos_fbdev_renderer *r, int index) {
  struct drm_mode_destroy_dumb destroy;

  if (!r || index < 0 || index > 1 || r->drm_fd < 0) {
    return;
  }
  if (r->drm_map[index] && r->drm_size[index] > 0) {
    munmap(r->drm_map[index], (size_t)r->drm_size[index]);
    r->drm_map[index] = NULL;
  }
  if (r->drm_fb_id[index]) {
    drmModeRmFB(r->drm_fd, r->drm_fb_id[index]);
    r->drm_fb_id[index] = 0;
  }
  if (r->drm_handle[index]) {
    memset(&destroy, 0, sizeof(destroy));
    destroy.handle = r->drm_handle[index];
    (void)drmIoctl(r->drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    r->drm_handle[index] = 0;
  }
}

static int plumos_fbdev_drm_create_buffer(
    struct plumos_fbdev_renderer *r, int index, uint32_t width,
    uint32_t height, char *error, size_t error_size) {
  struct drm_mode_create_dumb create;
  struct drm_mode_map_dumb map;
  void *mapped;

  memset(&create, 0, sizeof(create));
  create.width = width;
  create.height = height;
  create.bpp = 32;
  if (drmIoctl(r->drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0) {
    snprintf(error, error_size, "DRM create dumb buffer: %s",
             strerror(errno));
    return 0;
  }
  r->drm_handle[index] = create.handle;
  r->drm_pitch[index] = create.pitch;
  r->drm_size[index] = create.size;
  if (drmModeAddFB(r->drm_fd, width, height, 24, 32, create.pitch,
                   create.handle, &r->drm_fb_id[index]) != 0) {
    snprintf(error, error_size, "DRM add framebuffer: %s", strerror(errno));
    plumos_fbdev_drm_destroy_buffer(r, index);
    return 0;
  }
  memset(&map, 0, sizeof(map));
  map.handle = create.handle;
  if (drmIoctl(r->drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0) {
    snprintf(error, error_size, "DRM map dumb buffer: %s", strerror(errno));
    plumos_fbdev_drm_destroy_buffer(r, index);
    return 0;
  }
  mapped = mmap(NULL, (size_t)create.size, PROT_READ | PROT_WRITE,
                MAP_SHARED, r->drm_fd, (off_t)map.offset);
  if (mapped == MAP_FAILED) {
    snprintf(error, error_size, "mmap DRM dumb buffer: %s", strerror(errno));
    plumos_fbdev_drm_destroy_buffer(r, index);
    return 0;
  }
  r->drm_map[index] = (unsigned char *)mapped;
  memset(r->drm_map[index], 0, (size_t)create.size);
  return 1;
}

static int plumos_fbdev_drm_init(struct plumos_fbdev_renderer *r,
                                 const char *path, char *error,
                                 size_t error_size) {
  drmModeRes *resources = NULL;
  drmModeConnector *connector = NULL;
  drmModeEncoder *encoder = NULL;
  int connector_index;
  int chosen_mode = 0;
  int ok = 0;

  r->drm_fd = open(path, O_RDWR | O_CLOEXEC);
  if (r->drm_fd < 0) {
    snprintf(error, error_size, "open DRM %.160s: %.60s", path,
             strerror(errno));
    return 0;
  }
  resources = drmModeGetResources(r->drm_fd);
  if (!resources) {
    snprintf(error, error_size, "DRM get resources: %s", strerror(errno));
    goto out;
  }
  for (connector_index = 0; connector_index < resources->count_connectors;
       connector_index++) {
    drmModeConnector *candidate =
        drmModeGetConnector(r->drm_fd, resources->connectors[connector_index]);
    if (!candidate) {
      continue;
    }
    if (candidate->connection == DRM_MODE_CONNECTED &&
        candidate->count_modes > 0 &&
        (candidate->connector_type == DRM_MODE_CONNECTOR_DSI || !connector)) {
      if (connector) {
        drmModeFreeConnector(connector);
      }
      connector = candidate;
      if (candidate->connector_type == DRM_MODE_CONNECTOR_DSI) {
        break;
      }
    } else {
      drmModeFreeConnector(candidate);
    }
  }
  if (!connector) {
    snprintf(error, error_size, "DRM connected display not found");
    goto out;
  }
  for (connector_index = 0; connector_index < connector->count_modes;
       connector_index++) {
    if (connector->modes[connector_index].type & DRM_MODE_TYPE_PREFERRED) {
      chosen_mode = connector_index;
      break;
    }
  }
  r->drm_connector_id = connector->connector_id;
  r->drm_mode = connector->modes[chosen_mode];
  if (connector->encoder_id) {
    encoder = drmModeGetEncoder(r->drm_fd, connector->encoder_id);
  }
  if (!encoder || !encoder->crtc_id) {
    int encoder_index;
    if (encoder) {
      drmModeFreeEncoder(encoder);
      encoder = NULL;
    }
    for (encoder_index = 0; encoder_index < connector->count_encoders;
         encoder_index++) {
      drmModeEncoder *candidate =
          drmModeGetEncoder(r->drm_fd, connector->encoders[encoder_index]);
      if (candidate && candidate->crtc_id) {
        encoder = candidate;
        break;
      }
      if (candidate) {
        drmModeFreeEncoder(candidate);
      }
    }
  }
  if (!encoder || !encoder->crtc_id) {
    snprintf(error, error_size, "DRM display CRTC not found");
    goto out;
  }
  r->drm_crtc_id = encoder->crtc_id;
  r->drm_saved_crtc = drmModeGetCrtc(r->drm_fd, r->drm_crtc_id);
  if (!plumos_fbdev_drm_create_buffer(
          r, 0, r->drm_mode.hdisplay, r->drm_mode.vdisplay, error,
          error_size) ||
      !plumos_fbdev_drm_create_buffer(
          r, 1, r->drm_mode.hdisplay, r->drm_mode.vdisplay, error,
          error_size)) {
    goto out;
  }
  if (drmModeSetCrtc(r->drm_fd, r->drm_crtc_id, r->drm_fb_id[0], 0, 0,
                     &r->drm_connector_id, 1, &r->drm_mode) != 0) {
    snprintf(error, error_size, "DRM set CRTC: %s", strerror(errno));
    goto out;
  }
  r->drm_front = 0;
  r->drm_draw = 1;
  r->drm_active = 1;
  r->mem = r->drm_map[r->drm_draw];
  r->map_size = (size_t)r->drm_size[r->drm_draw];
  r->bytes_per_pixel = 4;
  r->frame_bytes =
      (long)r->drm_pitch[r->drm_draw] * (long)r->drm_mode.vdisplay;
  r->fix.line_length = r->drm_pitch[r->drm_draw];
  r->fix.smem_len = (uint32_t)r->drm_size[r->drm_draw];
  r->var.xres = r->drm_mode.hdisplay;
  r->var.yres = r->drm_mode.vdisplay;
  r->var.xres_virtual = r->var.xres;
  r->var.yres_virtual = r->var.yres;
  r->var.bits_per_pixel = 32;
  r->var.red.offset = 16;
  r->var.red.length = 8;
  r->var.green.offset = 8;
  r->var.green.length = 8;
  r->var.blue.offset = 0;
  r->var.blue.length = 8;
  r->physical_xres = r->var.xres;
  r->physical_yres = r->var.yres;
  r->active_offset = 0;
  r->visible_offset = 0;
  r->double_buffer = 1;
  ok = 1;

out:
  if (!ok) {
    plumos_fbdev_drm_destroy_buffer(r, 1);
    plumos_fbdev_drm_destroy_buffer(r, 0);
    if (r->drm_saved_crtc) {
      drmModeFreeCrtc(r->drm_saved_crtc);
      r->drm_saved_crtc = NULL;
    }
    close(r->drm_fd);
    r->drm_fd = -1;
  }
  if (encoder) {
    drmModeFreeEncoder(encoder);
  }
  if (connector) {
    drmModeFreeConnector(connector);
  }
  if (resources) {
    drmModeFreeResources(resources);
  }
  return ok;
}

static int plumos_fbdev_drm_present(struct plumos_fbdev_renderer *r) {
  drmEventContext event_context;
  struct pollfd pfd;

  if (!r || !r->drm_active) {
    return 0;
  }
  r->drm_page_flip_pending = 1;
  if (drmModePageFlip(r->drm_fd, r->drm_crtc_id,
                      r->drm_fb_id[r->drm_draw],
                      DRM_MODE_PAGE_FLIP_EVENT, r) != 0) {
    r->drm_page_flip_pending = 0;
    return 0;
  }
  memset(&event_context, 0, sizeof(event_context));
  event_context.version = DRM_EVENT_CONTEXT_VERSION;
  event_context.page_flip_handler = plumos_fbdev_drm_page_flip_handler;
  pfd.fd = r->drm_fd;
  pfd.events = POLLIN;
  pfd.revents = 0;
  while (r->drm_page_flip_pending) {
    int poll_rc = poll(&pfd, 1, 100);
    if (poll_rc <= 0 || !(pfd.revents & POLLIN) ||
        drmHandleEvent(r->drm_fd, &event_context) != 0) {
      r->drm_page_flip_pending = 0;
      return 0;
    }
  }
  r->drm_front = r->drm_draw;
  r->drm_draw = r->drm_front == 0 ? 1 : 0;
  r->mem = r->drm_map[r->drm_draw];
  r->map_size = (size_t)r->drm_size[r->drm_draw];
  r->frame_bytes =
      (long)r->drm_pitch[r->drm_draw] * (long)r->drm_mode.vdisplay;
  r->fix.line_length = r->drm_pitch[r->drm_draw];
  return 1;
}
#endif

static uint32_t plumos_fbdev_scale_channel(uint8_t value, uint32_t length,
                                           uint32_t offset) {
  uint32_t max_value;
  if (length == 0) {
    return 0;
  }
  if (length >= 8) {
    return ((uint32_t)value) << offset;
  }
  max_value = (1U << length) - 1U;
  return (((uint32_t)value * max_value + 127U) / 255U) << offset;
}

static uint32_t plumos_fbdev_pack_color(const struct plumos_fbdev_renderer *r,
                                        uint8_t red, uint8_t green, uint8_t blue) {
  uint32_t color = 0;
  color |= plumos_fbdev_scale_channel(red, r->var.red.length, r->var.red.offset);
  color |= plumos_fbdev_scale_channel(green, r->var.green.length, r->var.green.offset);
  color |= plumos_fbdev_scale_channel(blue, r->var.blue.length, r->var.blue.offset);
  if (r->var.transp.length) {
    color |= plumos_fbdev_scale_channel(255, r->var.transp.length, r->var.transp.offset);
  }
  return color;
}

static void plumos_fbdev_put_pixel(struct plumos_fbdev_renderer *r, int x, int y,
                                   uint32_t color) {
  long offset;
  unsigned char *p;

  if (!r || !r->mem || x < 0 || y < 0 || x >= (int)r->var.xres ||
      y >= (int)r->var.yres) {
    return;
  }
  if (r->rotation == 1) {
    int physical_x = (int)r->physical_xres - 1 - y;
    y = x;
    x = physical_x;
  } else if (r->rotation == 2) {
    x = (int)r->physical_xres - 1 - x;
    y = (int)r->physical_yres - 1 - y;
  } else if (r->rotation == 3) {
    int physical_x = y;
    y = (int)r->physical_yres - 1 - x;
    x = physical_x;
  }
  if (r->shadow) {
    offset = (long)y * (long)r->fix.line_length +
             (long)x * (long)r->bytes_per_pixel;
    if (offset < 0 || offset + r->bytes_per_pixel > r->frame_bytes) {
      return;
    }
    p = r->shadow + offset;
  } else {
    offset = r->active_offset + (long)y * (long)r->fix.line_length +
             (long)x * (long)r->bytes_per_pixel;
    if (offset < 0 || offset + r->bytes_per_pixel > (long)r->map_size) {
      return;
    }
    p = r->mem + offset;
  }
  if (r->bytes_per_pixel == 4) {
    p[0] = (unsigned char)(color & 0xffU);
    p[1] = (unsigned char)((color >> 8) & 0xffU);
    p[2] = (unsigned char)((color >> 16) & 0xffU);
    p[3] = (unsigned char)((color >> 24) & 0xffU);
  } else if (r->bytes_per_pixel == 3) {
    p[0] = (unsigned char)(color & 0xffU);
    p[1] = (unsigned char)((color >> 8) & 0xffU);
    p[2] = (unsigned char)((color >> 16) & 0xffU);
  } else if (r->bytes_per_pixel == 2) {
    p[0] = (unsigned char)(color & 0xffU);
    p[1] = (unsigned char)((color >> 8) & 0xffU);
  }
}

static void plumos_fbdev_fill_rect(struct plumos_fbdev_renderer *r, int x, int y,
                                   int w, int h, uint32_t color) {
  unsigned char *base;
  int x_end;
  int y_end;
  int yy;
  int xx;
  if (!r || !r->mem || w <= 0 || h <= 0) {
    return;
  }
  x_end = x + w;
  y_end = y + h;
  if (x < 0) {
    x = 0;
  }
  if (y < 0) {
    y = 0;
  }
  if (x_end > (int)r->var.xres) {
    x_end = (int)r->var.xres;
  }
  if (y_end > (int)r->var.yres) {
    y_end = (int)r->var.yres;
  }
  if (x >= x_end || y >= y_end) {
    return;
  }
  if (r->rotation == 1) {
    int rotated_x = (int)r->physical_xres - y_end;
    int rotated_y = x;
    int rotated_w = y_end - y;
    int rotated_h = x_end - x;
    x = rotated_x;
    y = rotated_y;
    x_end = x + rotated_w;
    y_end = y + rotated_h;
  } else if (r->rotation == 2) {
    int rotated_x = (int)r->physical_xres - x_end;
    int rotated_y = (int)r->physical_yres - y_end;
    x_end = rotated_x + (x_end - x);
    y_end = rotated_y + (y_end - y);
    x = rotated_x;
    y = rotated_y;
  } else if (r->rotation == 3) {
    int rotated_x = y;
    int rotated_y = (int)r->physical_yres - x_end;
    int rotated_w = y_end - y;
    int rotated_h = x_end - x;
    x = rotated_x;
    y = rotated_y;
    x_end = x + rotated_w;
    y_end = y + rotated_h;
  }
  base = r->shadow ? r->shadow : r->mem + r->active_offset;
  for (yy = y; yy < y_end; yy++) {
    unsigned char *row =
        base + (long)yy * (long)r->fix.line_length +
        (long)x * (long)r->bytes_per_pixel;
    if (r->bytes_per_pixel == 4) {
      uint32_t *pixels = (uint32_t *)row;
      for (xx = x; xx < x_end; xx++) {
        *pixels++ = color;
      }
    } else if (r->bytes_per_pixel == 2) {
      uint16_t *pixels = (uint16_t *)row;
      for (xx = x; xx < x_end; xx++) {
        *pixels++ = (uint16_t)color;
      }
    } else {
      for (xx = x; xx < x_end; xx++) {
        row[0] = (unsigned char)(color & 0xffU);
        row[1] = (unsigned char)((color >> 8) & 0xffU);
        row[2] = (unsigned char)((color >> 16) & 0xffU);
        row += 3;
      }
    }
  }
}

static int plumos_fbdev_frame_offset_valid(const struct plumos_fbdev_renderer *r,
                                           long offset) {
  return r && offset >= 0 && r->frame_bytes > 0 &&
         offset + r->frame_bytes <= (long)r->map_size;
}

static long plumos_fbdev_yoffset_to_offset(const struct plumos_fbdev_renderer *r,
                                           uint32_t yoffset) {
  if (!r) {
    return -1;
  }
  return (long)yoffset * (long)r->fix.line_length;
}

static int plumos_fbdev_present(struct plumos_fbdev_renderer *r) {
  struct fb_var_screeninfo next_var;
  uint32_t next_draw_yoffset;
  long next_draw_offset;

  if (!r || !r->mem) {
    return 0;
  }
#ifdef PLUMOS_FBDEV_ENABLE_DRM
  if (r->drm_active) {
    return plumos_fbdev_drm_present(r);
  }
#endif
  if (r->shadow) {
    if (!plumos_fbdev_frame_offset_valid(r, r->visible_offset)) {
      return 0;
    }
    memcpy(r->mem + r->visible_offset, r->shadow, (size_t)r->frame_bytes);
    return 1;
  }
  if (!r->double_buffer) {
    return 1;
  }

  next_var = r->var;
  next_var.xoffset = 0;
  next_var.yoffset = r->draw_yoffset;
#ifdef FB_ACTIVATE_VBL
  next_var.activate = FB_ACTIVATE_VBL;
#endif

  if (ioctl(r->fd, FBIOPAN_DISPLAY, &next_var) != 0) {
    if (plumos_fbdev_frame_offset_valid(r, r->visible_offset) &&
        r->visible_offset != r->active_offset) {
      memcpy(r->mem + r->visible_offset, r->mem + r->active_offset,
             (size_t)r->frame_bytes);
      r->active_offset = r->visible_offset;
    }
    r->double_buffer = 0;
    return 0;
  }

  r->var = next_var;
  r->visible_yoffset = r->draw_yoffset;
  r->visible_offset = r->active_offset;
  next_draw_yoffset = r->visible_yoffset == 0 ? r->var.yres : 0;
  next_draw_offset = plumos_fbdev_yoffset_to_offset(r, next_draw_yoffset);
  if (!plumos_fbdev_frame_offset_valid(r, next_draw_offset) ||
      next_draw_yoffset + r->var.yres > r->var.yres_virtual) {
    r->double_buffer = 0;
    r->active_offset = r->visible_offset;
    r->draw_yoffset = r->visible_yoffset;
    return 1;
  }
  r->draw_yoffset = next_draw_yoffset;
  r->active_offset = next_draw_offset;
  return 1;
}

#ifdef PLUMOS_FBDEV_ENABLE_PNG
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclobbered"
#endif
static unsigned char plumos_fbdev_component_to_u8(
    uint32_t packed, uint32_t offset, uint32_t length) {
  uint32_t mask;
  uint32_t value;

  if (length == 0) {
    return 0;
  }
  if (length >= 8) {
    return (unsigned char)((packed >> offset) & 0xffU);
  }
  mask = (1U << length) - 1U;
  value = (packed >> offset) & mask;
  return (unsigned char)((value * 255U + mask / 2U) / mask);
}

static int plumos_fbdev_write_screenshot_png(
    const struct plumos_fbdev_renderer *r, const char *path, char *error,
    size_t error_size) {
  const unsigned char *source = NULL;
  size_t stride = 0;
  FILE *output = NULL;
  png_structp png = NULL;
  png_infop info = NULL;
  png_bytep row = NULL;
  uint32_t width;
  uint32_t height;
  uint32_t y;
  int ok = 0;

  if (!r || !path || !path[0] || r->var.xres == 0 || r->var.yres == 0 ||
      r->bytes_per_pixel < 2 || r->bytes_per_pixel > 4) {
    snprintf(error, error_size, "invalid screenshot source");
    return 0;
  }
#ifdef PLUMOS_FBDEV_ENABLE_DRM
  if (r->drm_active) {
    source = r->drm_map[r->drm_front];
    stride = (size_t)r->drm_pitch[r->drm_front];
  } else
#endif
      if (r->shadow) {
    source = r->shadow;
    stride = (size_t)r->fix.line_length;
  } else if (r->mem && r->visible_offset >= 0) {
    source = r->mem + r->visible_offset;
    stride = (size_t)r->fix.line_length;
  }
  if (!source || stride == 0) {
    snprintf(error, error_size, "screenshot framebuffer unavailable");
    return 0;
  }

  width = r->var.xres;
  height = r->var.yres;
  output = fopen(path, "wb");
  if (!output) {
    snprintf(error, error_size, "open screenshot: %s", strerror(errno));
    return 0;
  }
  png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  info = png ? png_create_info_struct(png) : NULL;
  if (!png || !info) {
    snprintf(error, error_size, "create PNG writer");
    goto out;
  }
  if (setjmp(png_jmpbuf(png))) {
    snprintf(error, error_size, "write PNG");
    goto out;
  }
  row = (png_bytep)malloc((size_t)width * 3U);
  if (!row) {
    snprintf(error, error_size, "allocate screenshot row");
    goto out;
  }
  png_init_io(png, output);
  png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  for (y = 0; y < height; y++) {
    const unsigned char *source_row = source + (size_t)y * stride;
    uint32_t x;
    for (x = 0; x < width; x++) {
      const unsigned char *pixel =
          source_row + (size_t)x * (size_t)r->bytes_per_pixel;
      uint32_t packed = 0;
      int byte_index;
      for (byte_index = 0; byte_index < r->bytes_per_pixel; byte_index++) {
        packed |= (uint32_t)pixel[byte_index] << (byte_index * 8);
      }
      row[(size_t)x * 3U + 0U] = plumos_fbdev_component_to_u8(
          packed, r->var.red.offset, r->var.red.length);
      row[(size_t)x * 3U + 1U] = plumos_fbdev_component_to_u8(
          packed, r->var.green.offset, r->var.green.length);
      row[(size_t)x * 3U + 2U] = plumos_fbdev_component_to_u8(
          packed, r->var.blue.offset, r->var.blue.length);
    }
    png_write_row(png, row);
  }
  png_write_end(png, info);
  if (fflush(output) != 0 || fsync(fileno(output)) != 0) {
    snprintf(error, error_size, "flush screenshot: %s", strerror(errno));
    goto out;
  }
  ok = 1;

out:
  free(row);
  if (png || info) {
    png_destroy_write_struct(png ? &png : NULL, info ? &info : NULL);
  }
  if (output && fclose(output) != 0 && ok) {
    snprintf(error, error_size, "close screenshot: %s", strerror(errno));
    ok = 0;
  }
  return ok;
}

static int plumos_fbdev_path_has_png_ext(const char *path) {
  const char *ext;

  if (!path || !path[0]) {
    return 0;
  }
  ext = strrchr(path, '.');
  return ext && strlen(ext) == 4 && tolower((unsigned char)ext[1]) == 'p' &&
         tolower((unsigned char)ext[2]) == 'n' &&
         tolower((unsigned char)ext[3]) == 'g' && ext[4] == '\0';
}

static int plumos_fbdev_load_png_rgba(const char *path,
                                      unsigned char **pixels_out,
                                      int *width_out, int *height_out) {
  FILE *f;
  png_structp png = NULL;
  png_infop info = NULL;
  png_bytep *rows = NULL;
  png_bytep pixels = NULL;
  png_uint_32 width;
  png_uint_32 height;
  int bit_depth;
  int color_type;
  int interlace_type;
  size_t row_bytes;
  png_uint_32 y;
  int ok = 0;

  if (!path || !pixels_out || !width_out || !height_out ||
      !plumos_fbdev_path_has_png_ext(path)) {
    return 0;
  }
  *pixels_out = NULL;
  *width_out = 0;
  *height_out = 0;
  f = fopen(path, "rb");
  if (!f) {
    return 0;
  }
  png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) {
    fclose(f);
    return 0;
  }
  info = png_create_info_struct(png);
  if (!info) {
    png_destroy_read_struct(&png, NULL, NULL);
    fclose(f);
    return 0;
  }
  if (setjmp(png_jmpbuf(png))) {
    goto done;
  }
  png_init_io(png, f);
  png_read_info(png, info);
  png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type,
               &interlace_type, NULL, NULL);
  if (width == 0 || height == 0 || width > 2048 || height > 2048) {
    goto done;
  }
  if (bit_depth == 16) {
    png_set_strip_16(png);
  }
  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(png);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
    png_set_expand_gray_1_2_4_to_8(png);
  }
  if (png_get_valid(png, info, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png);
  }
  if ((color_type & PNG_COLOR_MASK_ALPHA) == 0) {
    png_set_filler(png, 0xff, PNG_FILLER_AFTER);
  }
  (void)png_set_interlace_handling(png);
  png_read_update_info(png, info);
  row_bytes = png_get_rowbytes(png, info);
  if (row_bytes != (size_t)width * 4u) {
    goto done;
  }
  pixels = (png_bytep)malloc(row_bytes * (size_t)height);
  rows = (png_bytep *)malloc(sizeof(png_bytep) * (size_t)height);
  if (!pixels || !rows) {
    goto done;
  }
  for (y = 0; y < height; y++) {
    rows[y] = pixels + row_bytes * (size_t)y;
  }
  png_read_image(png, rows);
  png_read_end(png, NULL);
  *pixels_out = pixels;
  *width_out = (int)width;
  *height_out = (int)height;
  pixels = NULL;
  ok = 1;

done:
  free(rows);
  free(pixels);
  png_destroy_read_struct(&png, &info, NULL);
  fclose(f);
  return ok;
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static void plumos_fbdev_png_cache_free_slot(
    struct plumos_fbdev_png_cache_slot *slot) {
  if (!slot) {
    return;
  }
  free(slot->pixels);
  memset(slot, 0, sizeof(*slot));
}

static void plumos_fbdev_png_cache_clear(struct plumos_fbdev_renderer *r) {
  size_t i;

  if (!r) {
    return;
  }
  for (i = 0; i < PLUMOS_FBDEV_PNG_CACHE_SLOTS; i++) {
    plumos_fbdev_png_cache_free_slot(&r->png_cache[i]);
  }
  r->png_cache_tick = 0;
}

static const unsigned char *plumos_fbdev_png_cache_get(
    struct plumos_fbdev_renderer *r, const char *path, int *width_out,
    int *height_out) {
  unsigned char *loaded_pixels = NULL;
  int loaded_width = 0;
  int loaded_height = 0;
  size_t i;
  size_t victim = 0;
  unsigned long oldest = 0;

  if (!r || !path || !path[0] || !width_out || !height_out) {
    return NULL;
  }
  *width_out = 0;
  *height_out = 0;
  r->png_cache_tick++;
  if (r->png_cache_tick == 0) {
    r->png_cache_tick = 1;
  }

  for (i = 0; i < PLUMOS_FBDEV_PNG_CACHE_SLOTS; i++) {
    if (r->png_cache[i].pixels && strcmp(r->png_cache[i].path, path) == 0) {
      r->png_cache[i].last_used = r->png_cache_tick;
      *width_out = r->png_cache[i].width;
      *height_out = r->png_cache[i].height;
      return r->png_cache[i].pixels;
    }
  }

  for (i = 0; i < PLUMOS_FBDEV_PNG_CACHE_SLOTS; i++) {
    if (!r->png_cache[i].pixels) {
      victim = i;
      break;
    }
    if (i == 0 || r->png_cache[i].last_used < oldest) {
      oldest = r->png_cache[i].last_used;
      victim = i;
    }
  }

  if (!plumos_fbdev_load_png_rgba(path, &loaded_pixels, &loaded_width,
                                  &loaded_height) ||
      loaded_width <= 0 || loaded_height <= 0) {
    free(loaded_pixels);
    return NULL;
  }

  plumos_fbdev_png_cache_free_slot(&r->png_cache[victim]);
  r->png_cache[victim].pixels = loaded_pixels;
  r->png_cache[victim].width = loaded_width;
  r->png_cache[victim].height = loaded_height;
  r->png_cache[victim].last_used = r->png_cache_tick;
  snprintf(r->png_cache[victim].path, sizeof(r->png_cache[victim].path),
           "%s", path);
  *width_out = loaded_width;
  *height_out = loaded_height;
  return loaded_pixels;
}

static int plumos_fbdev_draw_png_box(struct plumos_fbdev_renderer *r,
                                     const char *path, int x, int y,
                                     int box_w, int box_h, int cover) {
  const unsigned char *pixels = NULL;
  int image_w = 0;
  int image_h = 0;
  int draw_w;
  int draw_h;
  int draw_x;
  int draw_y;
  int start_x;
  int start_y;
  int end_x;
  int end_y;
  int tx;
  int ty;

  if (!r || !path || !path[0] || box_w <= 0 || box_h <= 0 ||
      !(pixels = plumos_fbdev_png_cache_get(r, path, &image_w, &image_h)) ||
      image_w <= 0 || image_h <= 0) {
    return 0;
  }

  draw_w = box_w;
  draw_h = (int)((long)image_h * (long)box_w / (long)image_w);
  if ((!cover && draw_h > box_h) || (cover && draw_h < box_h)) {
    draw_h = box_h;
    draw_w = (int)((long)image_w * (long)box_h / (long)image_h);
  }
  if (draw_w <= 0 || draw_h <= 0) {
    return 0;
  }
  draw_x = x + (box_w - draw_w) / 2;
  draw_y = y + (box_h - draw_h) / 2;
  start_x = draw_x > x ? draw_x : x;
  start_y = draw_y > y ? draw_y : y;
  end_x = draw_x + draw_w < x + box_w ? draw_x + draw_w : x + box_w;
  end_y = draw_y + draw_h < y + box_h ? draw_y + draw_h : y + box_h;
  if (start_x < 0) {
    start_x = 0;
  }
  if (start_y < 0) {
    start_y = 0;
  }
  if (end_x > (int)r->var.xres) {
    end_x = (int)r->var.xres;
  }
  if (end_y > (int)r->var.yres) {
    end_y = (int)r->var.yres;
  }

  for (ty = start_y; ty < end_y; ty++) {
    int sy = (int)((long)(ty - draw_y) * (long)image_h / (long)draw_h);
    if (sy >= image_h) {
      sy = image_h - 1;
    }
    for (tx = start_x; tx < end_x; tx++) {
      int sx = (int)((long)(tx - draw_x) * (long)image_w / (long)draw_w);
      const unsigned char *rgba;
      if (sx >= image_w) {
        sx = image_w - 1;
      }
      rgba = pixels + ((size_t)sy * (size_t)image_w + (size_t)sx) * 4U;
      if (rgba[3] < 128) {
        continue;
      }
      plumos_fbdev_put_pixel(r, tx, ty,
                             plumos_fbdev_pack_color(r, rgba[0], rgba[1], rgba[2]));
    }
  }

  return 1;
}

static int plumos_fbdev_draw_png_contain(struct plumos_fbdev_renderer *r,
                                         const char *path, int x, int y,
                                         int box_w, int box_h) {
  return plumos_fbdev_draw_png_box(r, path, x, y, box_w, box_h, 0);
}

static int plumos_fbdev_draw_png_cover(struct plumos_fbdev_renderer *r,
                                       const char *path, int x, int y,
                                       int box_w, int box_h) {
  return plumos_fbdev_draw_png_box(r, path, x, y, box_w, box_h, 1);
}
#endif

#endif

static const uint8_t *plumos_fbdev_glyph_for(char c) {
  static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
  static const uint8_t box[7] = {31, 17, 21, 17, 21, 17, 31};
  static const uint8_t glyph_0[7] = {14, 17, 19, 21, 25, 17, 14};
  static const uint8_t glyph_1[7] = {4, 12, 4, 4, 4, 4, 14};
  static const uint8_t glyph_2[7] = {14, 17, 1, 2, 4, 8, 31};
  static const uint8_t glyph_3[7] = {30, 1, 1, 14, 1, 1, 30};
  static const uint8_t glyph_4[7] = {2, 6, 10, 18, 31, 2, 2};
  static const uint8_t glyph_5[7] = {31, 16, 30, 1, 1, 17, 14};
  static const uint8_t glyph_6[7] = {6, 8, 16, 30, 17, 17, 14};
  static const uint8_t glyph_7[7] = {31, 1, 2, 4, 8, 8, 8};
  static const uint8_t glyph_8[7] = {14, 17, 17, 14, 17, 17, 14};
  static const uint8_t glyph_9[7] = {14, 17, 17, 15, 1, 2, 12};
  static const uint8_t glyph_a[7] = {14, 17, 17, 31, 17, 17, 17};
  static const uint8_t glyph_b[7] = {30, 17, 17, 30, 17, 17, 30};
  static const uint8_t glyph_c[7] = {14, 17, 16, 16, 16, 17, 14};
  static const uint8_t glyph_d[7] = {30, 17, 17, 17, 17, 17, 30};
  static const uint8_t glyph_e[7] = {31, 16, 16, 30, 16, 16, 31};
  static const uint8_t glyph_f[7] = {31, 16, 16, 30, 16, 16, 16};
  static const uint8_t glyph_g[7] = {14, 17, 16, 23, 17, 17, 15};
  static const uint8_t glyph_h[7] = {17, 17, 17, 31, 17, 17, 17};
  static const uint8_t glyph_i[7] = {14, 4, 4, 4, 4, 4, 14};
  static const uint8_t glyph_j[7] = {1, 1, 1, 1, 17, 17, 14};
  static const uint8_t glyph_k[7] = {17, 18, 20, 24, 20, 18, 17};
  static const uint8_t glyph_l[7] = {16, 16, 16, 16, 16, 16, 31};
  static const uint8_t glyph_m[7] = {17, 27, 21, 21, 17, 17, 17};
  static const uint8_t glyph_n[7] = {17, 25, 21, 19, 17, 17, 17};
  static const uint8_t glyph_o[7] = {14, 17, 17, 17, 17, 17, 14};
  static const uint8_t glyph_p[7] = {30, 17, 17, 30, 16, 16, 16};
  static const uint8_t glyph_q[7] = {14, 17, 17, 17, 21, 18, 13};
  static const uint8_t glyph_r[7] = {30, 17, 17, 30, 20, 18, 17};
  static const uint8_t glyph_s[7] = {15, 16, 16, 14, 1, 1, 30};
  static const uint8_t glyph_t[7] = {31, 4, 4, 4, 4, 4, 4};
  static const uint8_t glyph_u[7] = {17, 17, 17, 17, 17, 17, 14};
  static const uint8_t glyph_v[7] = {17, 17, 17, 17, 17, 10, 4};
  static const uint8_t glyph_w[7] = {17, 17, 17, 21, 21, 21, 10};
  static const uint8_t glyph_x[7] = {17, 17, 10, 4, 10, 17, 17};
  static const uint8_t glyph_y[7] = {17, 17, 10, 4, 4, 4, 4};
  static const uint8_t glyph_z[7] = {31, 1, 2, 4, 8, 16, 31};
  static const uint8_t dash[7] = {0, 0, 0, 31, 0, 0, 0};
  static const uint8_t colon[7] = {0, 4, 4, 0, 4, 4, 0};
  static const uint8_t slash[7] = {1, 1, 2, 4, 8, 16, 16};
  static const uint8_t dot[7] = {0, 0, 0, 0, 0, 12, 12};
  static const uint8_t comma[7] = {0, 0, 0, 0, 0, 4, 8};
  static const uint8_t at[7] = {14, 17, 23, 21, 23, 16, 14};
  static const uint8_t hash[7] = {10, 31, 10, 10, 31, 10, 0};
  static const uint8_t percent[7] = {17, 2, 4, 8, 17, 0, 0};
  static const uint8_t tilde[7] = {0, 0, 8, 21, 2, 0, 0};
  static const uint8_t eq[7] = {0, 0, 31, 0, 31, 0, 0};
  static const uint8_t gt[7] = {16, 8, 4, 2, 4, 8, 16};
  static const uint8_t lt[7] = {1, 2, 4, 8, 4, 2, 1};
  static const uint8_t under[7] = {0, 0, 0, 0, 0, 0, 31};
  static const uint8_t pipe[7] = {4, 4, 4, 4, 4, 4, 4};
  static const uint8_t plus[7] = {0, 4, 4, 31, 4, 4, 0};
  static const uint8_t star[7] = {0, 21, 14, 31, 14, 21, 0};
  static const uint8_t bang[7] = {4, 4, 4, 4, 4, 0, 4};
  static const uint8_t quest[7] = {14, 17, 1, 2, 4, 0, 4};
  static const uint8_t lpar[7] = {2, 4, 8, 8, 8, 4, 2};
  static const uint8_t rpar[7] = {8, 4, 2, 2, 2, 4, 8};
  static const uint8_t lbr[7] = {14, 8, 8, 8, 8, 8, 14};
  static const uint8_t rbr[7] = {14, 2, 2, 2, 2, 2, 14};

  if (c >= 'a' && c <= 'z') {
    c = (char)(c - 'a' + 'A');
  }
  switch (c) {
  case ' ': return blank;
  case '0': return glyph_0;
  case '1': return glyph_1;
  case '2': return glyph_2;
  case '3': return glyph_3;
  case '4': return glyph_4;
  case '5': return glyph_5;
  case '6': return glyph_6;
  case '7': return glyph_7;
  case '8': return glyph_8;
  case '9': return glyph_9;
  case 'A': return glyph_a;
  case 'B': return glyph_b;
  case 'C': return glyph_c;
  case 'D': return glyph_d;
  case 'E': return glyph_e;
  case 'F': return glyph_f;
  case 'G': return glyph_g;
  case 'H': return glyph_h;
  case 'I': return glyph_i;
  case 'J': return glyph_j;
  case 'K': return glyph_k;
  case 'L': return glyph_l;
  case 'M': return glyph_m;
  case 'N': return glyph_n;
  case 'O': return glyph_o;
  case 'P': return glyph_p;
  case 'Q': return glyph_q;
  case 'R': return glyph_r;
  case 'S': return glyph_s;
  case 'T': return glyph_t;
  case 'U': return glyph_u;
  case 'V': return glyph_v;
  case 'W': return glyph_w;
  case 'X': return glyph_x;
  case 'Y': return glyph_y;
  case 'Z': return glyph_z;
  case '-': return dash;
  case ':': return colon;
  case '/':
  case '\\': return slash;
  case '.': return dot;
  case ',': return comma;
  case '@': return at;
  case '#': return hash;
  case '%': return percent;
  case '~': return tilde;
  case '=': return eq;
  case '>': return gt;
  case '<': return lt;
  case '_': return under;
  case '|': return pipe;
  case '+': return plus;
  case '*': return star;
  case '!': return bang;
  case '?': return quest;
  case '(':
  case '{': return lpar;
  case ')':
  case '}': return rpar;
  case '[': return lbr;
  case ']': return rbr;
  default: return box;
  }
}

#ifndef PLUMOS_FBDEV_RENDERER_GLYPHS_ONLY
static void plumos_fbdev_draw_char(struct plumos_fbdev_renderer *r, int x, int y,
                                   char c, int scale, uint32_t color) {
  const uint8_t *g = plumos_fbdev_glyph_for(c);
  int row;
  int col;
  for (row = 0; row < 7; row++) {
    for (col = 0; col < 5; col++) {
      if (g[row] & (1U << (4 - col))) {
        plumos_fbdev_fill_rect(r, x + col * scale, y + row * scale, scale, scale,
                               color);
      }
    }
  }
}

static void plumos_fbdev_draw_char_clipped(struct plumos_fbdev_renderer *r,
                                           int x, int y, char c, int scale,
                                           int min_x, int max_x,
                                           uint32_t color) {
  const uint8_t *g = plumos_fbdev_glyph_for(c);
  int row;
  int col;
  int yy;
  int xx;

  if (scale <= 0 || max_x <= min_x) {
    return;
  }
  for (row = 0; row < 7; row++) {
    for (col = 0; col < 5; col++) {
      if (!(g[row] & (1U << (4 - col)))) {
        continue;
      }
      for (yy = 0; yy < scale; yy++) {
        for (xx = 0; xx < scale; xx++) {
          int draw_x = x + col * scale + xx;
          int draw_y = y + row * scale + yy;
          if (draw_x >= min_x && draw_x < max_x) {
            plumos_fbdev_put_pixel(r, draw_x, draw_y, color);
          }
        }
      }
    }
  }
}

static const char *plumos_fbdev_utf8_next(const char *s,
                                          unsigned int *codepoint) {
  const unsigned char *p = (const unsigned char *)s;
  unsigned int cp;

  if (!p || !*p) {
    if (codepoint) {
      *codepoint = 0;
    }
    return s;
  }
  if (p[0] < 0x80) {
    if (codepoint) {
      *codepoint = p[0];
    }
    return s + 1;
  }
  if ((p[0] & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
    cp = ((unsigned int)(p[0] & 0x1f) << 6) |
         (unsigned int)(p[1] & 0x3f);
    if (cp >= 0x80) {
      if (codepoint) {
        *codepoint = cp;
      }
      return s + 2;
    }
  } else if ((p[0] & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 &&
             (p[2] & 0xc0) == 0x80) {
    cp = ((unsigned int)(p[0] & 0x0f) << 12) |
         ((unsigned int)(p[1] & 0x3f) << 6) |
         (unsigned int)(p[2] & 0x3f);
    if (cp >= 0x800 && !(cp >= 0xd800 && cp <= 0xdfff)) {
      if (codepoint) {
        *codepoint = cp;
      }
      return s + 3;
    }
  } else if ((p[0] & 0xf8) == 0xf0 && (p[1] & 0xc0) == 0x80 &&
             (p[2] & 0xc0) == 0x80 && (p[3] & 0xc0) == 0x80) {
    cp = ((unsigned int)(p[0] & 0x07) << 18) |
         ((unsigned int)(p[1] & 0x3f) << 12) |
         ((unsigned int)(p[2] & 0x3f) << 6) |
         (unsigned int)(p[3] & 0x3f);
    if (cp >= 0x10000 && cp <= 0x10ffff) {
      if (codepoint) {
        *codepoint = cp;
      }
      return s + 4;
    }
  }
  if (codepoint) {
    *codepoint = '?';
  }
  return s + 1;
}

static int plumos_fbdev_utf8_append_codepoint(char *out, size_t out_size,
                                              size_t *pos,
                                              unsigned int codepoint) {
  if (!out || !pos || *pos >= out_size) {
    return 0;
  }
  if (codepoint <= 0x7f) {
    if (*pos + 1 >= out_size) {
      return 0;
    }
    out[(*pos)++] = (char)codepoint;
  } else if (codepoint <= 0x7ff) {
    if (*pos + 2 >= out_size) {
      return 0;
    }
    out[(*pos)++] = (char)(0xc0 | (codepoint >> 6));
    out[(*pos)++] = (char)(0x80 | (codepoint & 0x3f));
  } else if (codepoint <= 0xffff) {
    if (*pos + 3 >= out_size) {
      return 0;
    }
    out[(*pos)++] = (char)(0xe0 | (codepoint >> 12));
    out[(*pos)++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
    out[(*pos)++] = (char)(0x80 | (codepoint & 0x3f));
  } else if (codepoint <= 0x10ffff) {
    if (*pos + 4 >= out_size) {
      return 0;
    }
    out[(*pos)++] = (char)(0xf0 | (codepoint >> 18));
    out[(*pos)++] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
    out[(*pos)++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
    out[(*pos)++] = (char)(0x80 | (codepoint & 0x3f));
  } else {
    return 0;
  }
  out[*pos] = '\0';
  return 1;
}

static unsigned int plumos_fbdev_compose_kana_mark(unsigned int base,
                                                   unsigned int mark) {
  if (mark == 0x3099) {
    switch (base) {
    case 0x3046:
      return 0x3094;
    case 0x30A6:
      return 0x30F4;
    case 0x30EF:
      return 0x30F7;
    case 0x30F0:
      return 0x30F8;
    case 0x30F1:
      return 0x30F9;
    case 0x30F2:
      return 0x30FA;
    default:
      break;
    }
    if ((base >= 0x304B && base <= 0x3069 &&
         (base == 0x304B || base == 0x304D || base == 0x304F ||
          base == 0x3051 || base == 0x3053 || base == 0x3055 ||
          base == 0x3057 || base == 0x3059 || base == 0x305B ||
          base == 0x305D || base == 0x305F || base == 0x3061 ||
          base == 0x3064 || base == 0x3066 || base == 0x3068)) ||
        (base >= 0x30AB && base <= 0x30C8 &&
         (base == 0x30AB || base == 0x30AD || base == 0x30AF ||
          base == 0x30B1 || base == 0x30B3 || base == 0x30B5 ||
          base == 0x30B7 || base == 0x30B9 || base == 0x30BB ||
          base == 0x30BD || base == 0x30BF || base == 0x30C1 ||
          base == 0x30C4 || base == 0x30C6 || base == 0x30C8))) {
      return base + 1;
    }
    if ((base >= 0x306F && base <= 0x307B &&
         (base == 0x306F || base == 0x3072 || base == 0x3075 ||
          base == 0x3078 || base == 0x307B)) ||
        (base >= 0x30CF && base <= 0x30DB &&
         (base == 0x30CF || base == 0x30D2 || base == 0x30D5 ||
          base == 0x30D8 || base == 0x30DB))) {
      return base + 1;
    }
  } else if (mark == 0x309A) {
    if ((base >= 0x306F && base <= 0x307B &&
         (base == 0x306F || base == 0x3072 || base == 0x3075 ||
          base == 0x3078 || base == 0x307B)) ||
        (base >= 0x30CF && base <= 0x30DB &&
         (base == 0x30CF || base == 0x30D2 || base == 0x30D5 ||
          base == 0x30D8 || base == 0x30DB))) {
      return base + 2;
    }
  }
  return 0;
}

static void plumos_fbdev_normalize_kana_marks(const char *in, char *out,
                                              size_t out_size) {
  const char *p;
  size_t n = 0;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!in) {
    return;
  }
  p = in;
  while (*p) {
    unsigned int cp;
    unsigned int next_cp = 0;
    unsigned int composed = 0;
    const char *next = plumos_fbdev_utf8_next(p, &cp);
    const char *after_next = next;

    if (*next) {
      after_next = plumos_fbdev_utf8_next(next, &next_cp);
      composed = plumos_fbdev_compose_kana_mark(cp, next_cp);
    }
    if (composed) {
      if (!plumos_fbdev_utf8_append_codepoint(out, out_size, &n, composed)) {
        break;
      }
      p = after_next;
    } else {
      if (!plumos_fbdev_utf8_append_codepoint(out, out_size, &n, cp)) {
        break;
      }
      p = next;
    }
  }
}

static int plumos_fbdev_unicode_is_combining(unsigned int codepoint) {
  return (codepoint >= 0x0300 && codepoint <= 0x036f) ||
         (codepoint >= 0x1ab0 && codepoint <= 0x1aff) ||
         (codepoint >= 0x1dc0 && codepoint <= 0x1dff) ||
         (codepoint >= 0x20d0 && codepoint <= 0x20ff) ||
         (codepoint >= 0xfe20 && codepoint <= 0xfe2f);
}

static int plumos_fbdev_unicode_is_wide(unsigned int codepoint) {
  return (codepoint >= 0x1100 && codepoint <= 0x115f) ||
         (codepoint >= 0x2329 && codepoint <= 0x232a) ||
         (codepoint >= 0x2e80 && codepoint <= 0xa4cf) ||
         (codepoint >= 0xac00 && codepoint <= 0xd7a3) ||
         (codepoint >= 0xf900 && codepoint <= 0xfaff) ||
         (codepoint >= 0xfe10 && codepoint <= 0xfe19) ||
         (codepoint >= 0xfe30 && codepoint <= 0xfe6f) ||
         (codepoint >= 0xff00 && codepoint <= 0xff60) ||
         (codepoint >= 0xffe0 && codepoint <= 0xffe6) ||
         (codepoint >= 0x1f200 && codepoint <= 0x1f251) ||
         (codepoint >= 0x20000 && codepoint <= 0x3fffd);
}

static int plumos_fbdev_utf8_cell_width(unsigned int codepoint) {
  if (plumos_fbdev_unicode_is_combining(codepoint)) {
    return 0;
  }
  return plumos_fbdev_unicode_is_wide(codepoint) ? 2 : 1;
}

#ifdef PLUMOS_FBDEV_ENABLE_FREETYPE
static int plumos_fbdev_renderer_init_freetype(
    struct plumos_fbdev_renderer *r, char *error, size_t error_size) {
  FT_Error ft_error;

  if (!r) {
    return 0;
  }
  if (r->ft_library) {
    return 1;
  }
  ft_error = FT_Init_FreeType(&r->ft_library);
  if (ft_error) {
    snprintf(error, error_size, "FT_Init_FreeType failed: %d", (int)ft_error);
    return 0;
  }
  return 1;
}

static void plumos_fbdev_ft_glyph_cache_clear(struct plumos_fbdev_renderer *r) {
  size_t i;

  if (!r) {
    return;
  }
  for (i = 0; i < PLUMOS_FBDEV_FT_GLYPH_CACHE_SIZE; i++) {
    free(r->ft_glyph_cache[i].alpha);
    r->ft_glyph_cache[i].alpha = NULL;
    r->ft_glyph_cache[i].valid = 0;
    r->ft_glyph_cache[i].used_at = 0;
  }
  r->ft_glyph_cache_tick = 0;
}

static int plumos_fbdev_renderer_load_font(struct plumos_fbdev_renderer *r,
                                           const char *font_path,
                                           char *error,
                                           size_t error_size) {
  FT_Error ft_error;

  if (!r || !font_path || !font_path[0]) {
    snprintf(error, error_size, "font path is empty");
    return 0;
  }
  if (!plumos_fbdev_renderer_init_freetype(r, error, error_size)) {
    return 0;
  }
  if (r->ft_face) {
    FT_Done_Face(r->ft_face);
    r->ft_face = NULL;
    r->ft_ready = 0;
  }
  ft_error = FT_New_Face(r->ft_library, font_path, 0, &r->ft_face);
  if (ft_error) {
    snprintf(error, error_size, "FT_New_Face failed: %d", (int)ft_error);
    return 0;
  }
  r->ft_ready = 1;
  r->ft_pixel_size = 0;
  memset(r->ft_advance_cache, 0, sizeof(r->ft_advance_cache));
  plumos_fbdev_ft_glyph_cache_clear(r);
  return 1;
}

static int plumos_fbdev_renderer_load_fallback_font(
    struct plumos_fbdev_renderer *r, const char *font_path, char *error,
    size_t error_size) {
  FT_Error ft_error;

  if (!r || !font_path || !font_path[0]) {
    snprintf(error, error_size, "fallback font path is empty");
    return 0;
  }
  if (!plumos_fbdev_renderer_init_freetype(r, error, error_size)) {
    return 0;
  }
  if (r->ft_fallback_face) {
    FT_Done_Face(r->ft_fallback_face);
    r->ft_fallback_face = NULL;
    r->ft_fallback_ready = 0;
  }
  ft_error = FT_New_Face(r->ft_library, font_path, 0, &r->ft_fallback_face);
  if (ft_error) {
    snprintf(error, error_size, "FT_New_Face fallback failed: %d",
             (int)ft_error);
    return 0;
  }
  r->ft_fallback_ready = 1;
  r->ft_fallback_pixel_size = 0;
  memset(r->ft_advance_cache, 0, sizeof(r->ft_advance_cache));
  plumos_fbdev_ft_glyph_cache_clear(r);
  return 1;
}

static int plumos_fbdev_ft_set_face_size(FT_Face face, int *pixel_size_state,
                                         int scale) {
  int pixel_size;

  if (!face || !pixel_size_state) {
    return 0;
  }
  pixel_size = scale > 0 ? 9 * scale : 9;
  if (pixel_size < 12) {
    pixel_size = 12;
  }
  if (*pixel_size_state == pixel_size) {
    return 1;
  }
  if (FT_Set_Pixel_Sizes(face, 0, (FT_UInt)pixel_size) != 0) {
    return 0;
  }
  *pixel_size_state = pixel_size;
  return 1;
}

static FT_Face plumos_fbdev_ft_select_face(struct plumos_fbdev_renderer *r,
                                           unsigned int codepoint,
                                           int **pixel_size_state) {
  if (pixel_size_state) {
    *pixel_size_state = NULL;
  }
  if (!r) {
    return NULL;
  }
  if (r->ft_ready && r->ft_face &&
      FT_Get_Char_Index(r->ft_face, (FT_ULong)codepoint) != 0) {
    if (pixel_size_state) {
      *pixel_size_state = &r->ft_pixel_size;
    }
    return r->ft_face;
  }
  if (r->ft_fallback_ready && r->ft_fallback_face &&
      FT_Get_Char_Index(r->ft_fallback_face, (FT_ULong)codepoint) != 0) {
    if (pixel_size_state) {
      *pixel_size_state = &r->ft_fallback_pixel_size;
    }
    return r->ft_fallback_face;
  }
  return NULL;
}

static int plumos_fbdev_ft_advance(struct plumos_fbdev_renderer *r,
                                   unsigned int codepoint, int scale) {
  int fallback;
  int advance;
  size_t cache_index;
  struct plumos_fbdev_ft_advance_cache_entry *cached;
  FT_Face face;
  int *pixel_size_state = NULL;

  fallback = 8 * scale;
  if (fallback < 8) {
    fallback = 8;
  }
  if (!r || !r->ft_ready || !r->ft_face) {
    return fallback;
  }
  cache_index = (((size_t)codepoint * 131u) + (size_t)scale) %
                PLUMOS_FBDEV_FT_ADVANCE_CACHE_SIZE;
  cached = &r->ft_advance_cache[cache_index];
  if (cached->valid && cached->codepoint == codepoint &&
      cached->scale == scale) {
    return cached->advance;
  }
  face = plumos_fbdev_ft_select_face(r, codepoint, &pixel_size_state);
  if (!face || !pixel_size_state ||
      !plumos_fbdev_ft_set_face_size(face, pixel_size_state, scale)) {
    return fallback;
  }
  if (FT_Load_Char(face, (FT_ULong)codepoint, FT_LOAD_DEFAULT) != 0) {
    advance = fallback;
  } else {
    advance = (int)((face->glyph->advance.x + 32) >> 6);
    if (advance <= 0) {
      advance = fallback;
    }
  }
  cached->codepoint = codepoint;
  cached->scale = scale;
  cached->advance = advance;
  cached->valid = 1;
  return advance;
}

static const struct plumos_fbdev_ft_glyph_cache_entry *
plumos_fbdev_ft_glyph_cache_get(struct plumos_fbdev_renderer *r,
                                unsigned int codepoint, int scale) {
  FT_Face face;
  FT_GlyphSlot slot;
  FT_Bitmap *bitmap;
  int *pixel_size_state = NULL;
  int face_slot;
  int pixel_size;
  size_t i;
  size_t slot_index = 0;
  unsigned long oldest = 0;
  struct plumos_fbdev_ft_glyph_cache_entry *cached;

  face = plumos_fbdev_ft_select_face(r, codepoint, &pixel_size_state);
  if (!face || !pixel_size_state ||
      !plumos_fbdev_ft_set_face_size(face, pixel_size_state, scale)) {
    return NULL;
  }
  face_slot = (face == r->ft_fallback_face) ? 1 : 0;
  for (i = 0; i < PLUMOS_FBDEV_FT_GLYPH_CACHE_SIZE; i++) {
    cached = &r->ft_glyph_cache[i];
    if (cached->valid && cached->codepoint == codepoint &&
        cached->scale == scale && cached->face_slot == face_slot) {
      cached->used_at = ++r->ft_glyph_cache_tick;
      return cached;
    }
  }
  if (FT_Load_Char(face, (FT_ULong)codepoint,
                   FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT) != 0) {
    return NULL;
  }
  slot = face->glyph;
  bitmap = &slot->bitmap;
  pixel_size = *pixel_size_state > 0 ? *pixel_size_state : 9 * scale;

  for (i = 0; i < PLUMOS_FBDEV_FT_GLYPH_CACHE_SIZE; i++) {
    cached = &r->ft_glyph_cache[i];
    if (!cached->valid) {
      slot_index = i;
      break;
    }
    if (i == 0 || cached->used_at < oldest) {
      oldest = cached->used_at;
      slot_index = i;
    }
  }
  cached = &r->ft_glyph_cache[slot_index];
  free(cached->alpha);
  memset(cached, 0, sizeof(*cached));
  cached->codepoint = codepoint;
  cached->scale = scale;
  cached->face_slot = face_slot;
  cached->pixel_size = pixel_size;
  cached->bitmap_left = slot->bitmap_left;
  cached->bitmap_top = slot->bitmap_top;
  cached->width = (int)bitmap->width;
  cached->rows = (int)bitmap->rows;
  if (cached->width > 0 && cached->rows > 0) {
    cached->alpha = (unsigned char *)malloc(
        (size_t)cached->width * (size_t)cached->rows);
    if (!cached->alpha) {
      return NULL;
    }
    for (i = 0; i < (size_t)cached->rows; i++) {
      const unsigned char *row_data;
      int col;
      if (bitmap->pitch < 0) {
        row_data = bitmap->buffer +
                   ((int)bitmap->rows - 1 - (int)i) * (-bitmap->pitch);
      } else {
        row_data = bitmap->buffer + (int)i * bitmap->pitch;
      }
      for (col = 0; col < cached->width; col++) {
        unsigned char value;
        if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
          value = (row_data[col >> 3] & (0x80u >> (col & 7))) ? 255 : 0;
        } else {
          value = row_data[col];
        }
        cached->alpha[(size_t)i * (size_t)cached->width + (size_t)col] =
            value;
      }
    }
  }
  cached->valid = 1;
  cached->used_at = ++r->ft_glyph_cache_tick;
  return cached;
}

static int plumos_fbdev_draw_freetype_codepoint_clipped(
    struct plumos_fbdev_renderer *r, unsigned int codepoint, int x, int y,
    int scale, int min_x, int max_x, uint32_t color) {
  const struct plumos_fbdev_ft_glyph_cache_entry *glyph;
  int baseline;
  int row;

  glyph = plumos_fbdev_ft_glyph_cache_get(r, codepoint, scale);
  if (!glyph) {
    return 0;
  }
  if (glyph->width <= 0 || glyph->rows <= 0 || !glyph->alpha) {
    return 1;
  }
  baseline = y + glyph->pixel_size - 2;
  for (row = 0; row < glyph->rows; row++) {
    const unsigned char *row_data =
        glyph->alpha + (size_t)row * (size_t)glyph->width;
    int col;
    int draw_y = baseline - glyph->bitmap_top + row;

    if (draw_y < 0 || draw_y >= (int)r->var.yres) {
      continue;
    }
    for (col = 0; col < glyph->width; col++) {
      unsigned char value = row_data[col];
      int draw_x = x + glyph->bitmap_left + col;
      if (value >= 80 && draw_x >= min_x && draw_x < max_x) {
        plumos_fbdev_put_pixel(r, draw_x, draw_y, color);
      }
    }
  }
  return 1;
}
#endif

static int plumos_fbdev_codepoint_advance(struct plumos_fbdev_renderer *r,
                                          unsigned int codepoint, int scale,
                                          int prefer_freetype) {
  int ascii_step = 6 * scale;

  if (ascii_step < 6) {
    ascii_step = 6;
  }
#ifdef PLUMOS_FBDEV_ENABLE_FREETYPE
  if (r && r->ft_ready && r->ft_face && (prefer_freetype || codepoint > 126)) {
    return plumos_fbdev_ft_advance(r, codepoint, scale);
  }
#else
  (void)r;
  (void)prefer_freetype;
#endif
  if (codepoint > 126) {
    return ascii_step * plumos_fbdev_utf8_cell_width(codepoint);
  }
  return ascii_step;
}

static int plumos_fbdev_text_width_font(struct plumos_fbdev_renderer *r,
                                        const char *text, int scale,
                                        int prefer_freetype) {
  char normalized[PLUMOS_FBDEV_RENDER_LINE_MAX];
  const char *p;
  int width = 0;

  if (!text || scale <= 0) {
    return 0;
  }
  plumos_fbdev_normalize_kana_marks(text, normalized, sizeof(normalized));
  p = normalized[0] ? normalized : text;
  while (*p) {
    unsigned int cp;
    const char *next = plumos_fbdev_utf8_next(p, &cp);
    if (cp == '\t') {
      cp = ' ';
    }
    if (cp >= 32) {
      width += plumos_fbdev_codepoint_advance(r, cp, scale,
                                              prefer_freetype);
    }
    p = next;
  }
  return width;
}

static int plumos_fbdev_text_width_for(struct plumos_fbdev_renderer *r,
                                       const char *text, int scale) {
  return plumos_fbdev_text_width_font(r, text, scale, 0);
}

static void plumos_fbdev_draw_text_clipped(struct plumos_fbdev_renderer *r,
                                           int x, int y, const char *text,
                                           int scale, int prefer_freetype,
                                           int min_x, int max_x,
                                           uint32_t color) {
  char normalized[PLUMOS_FBDEV_RENDER_LINE_MAX];
  const char *p;
  int pen_x = 0;

  if (!r || !text || scale <= 0 || max_x <= min_x) {
    return;
  }
  if (max_x > (int)r->var.xres - 8) {
    max_x = (int)r->var.xres - 8;
  }
  plumos_fbdev_normalize_kana_marks(text, normalized, sizeof(normalized));
  p = normalized[0] ? normalized : text;
  while (*p) {
    unsigned int cp;
    int advance;
    int glyph_x;
    const char *next = plumos_fbdev_utf8_next(p, &cp);

    if (cp == '\t') {
      cp = ' ';
    }
    if (cp < 32) {
      p = next;
      continue;
    }
    advance = plumos_fbdev_codepoint_advance(r, cp, scale, prefer_freetype);
    glyph_x = x + pen_x;
    if (glyph_x >= max_x) {
      break;
    }
    if (glyph_x + advance > min_x) {
      if (cp <= 126 && !prefer_freetype) {
        if (glyph_x >= min_x && glyph_x + 5 * scale <= max_x) {
          plumos_fbdev_draw_char(r, glyph_x, y, (char)cp, scale, color);
        } else {
          plumos_fbdev_draw_char_clipped(r, glyph_x, y, (char)cp, scale,
                                         min_x, max_x, color);
        }
      } else {
#ifdef PLUMOS_FBDEV_ENABLE_FREETYPE
        if (!plumos_fbdev_draw_freetype_codepoint_clipped(
                r, cp, glyph_x, y, scale, min_x, max_x, color)) {
          plumos_fbdev_draw_char_clipped(r, glyph_x, y,
                                         cp <= 126 ? (char)cp : '?', scale,
                                         min_x, max_x, color);
        }
#else
        plumos_fbdev_draw_char_clipped(r, glyph_x, y,
                                       cp <= 126 ? (char)cp : '?', scale,
                                       min_x, max_x, color);
#endif
      }
    }
    pen_x += advance;
    p = next;
  }
}

static void plumos_fbdev_draw_text_font(struct plumos_fbdev_renderer *r,
                                        int x, int y, const char *text,
                                        int scale, int prefer_freetype,
                                        uint32_t color, int max_width) {
  plumos_fbdev_draw_text_clipped(r, x, y, text, scale, prefer_freetype, x,
                                 max_width, color);
}

static long long plumos_fbdev_time_ms(void) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    return (long long)time(NULL) * 1000LL;
  }
  return (long long)tv.tv_sec * 1000LL + (long long)(tv.tv_usec / 1000);
}

static int plumos_fbdev_marquee_offset(struct plumos_fbdev_renderer *r,
                                       int text_width, int available_width,
                                       int step_px) {
  int overflow;
  int scroll_ms;
  int cycle_ms;
  int phase_ms;
  long long elapsed_ms;
  const int hold_ms = 1000;
  const int pixels_per_second = 80;

  (void)step_px;
  overflow = text_width - available_width;
  if (overflow <= 0) {
    return 0;
  }
  elapsed_ms = plumos_fbdev_time_ms() - (r ? r->marquee_focus_ms : 0);
  if (elapsed_ms < hold_ms) {
    return 0;
  }
  scroll_ms = (overflow * 1000 + pixels_per_second - 1) / pixels_per_second;
  cycle_ms = scroll_ms + hold_ms;
  phase_ms = (int)((elapsed_ms - hold_ms) % (long long)cycle_ms);
  if (phase_ms >= scroll_ms) {
    return overflow;
  }
  return (phase_ms * pixels_per_second) / 1000;
}

static void plumos_fbdev_draw_text(struct plumos_fbdev_renderer *r, int x, int y,
                                   const char *text, int scale, uint32_t color,
                                   int max_width) {
  plumos_fbdev_draw_text_font(r, x, y, text, scale, 0, color, max_width);
}

struct plumos_fbdev_palette {
  uint32_t background;
  uint32_t foreground;
  uint32_t muted;
  uint32_t accent;
  uint32_t panel;
  uint32_t panel_inner;
  uint32_t media_panel;
  uint32_t selection_background;
  uint32_t selection_foreground;
  uint32_t danger;
  uint32_t line;
};

struct plumos_fbdev_entry {
  int selected;
  char title[160];
  char detail[224];
  char media[224];
};

struct plumos_fbdev_motion {
  char top_layout[32];
  char transition_axis[32];
  char transition_easing[32];
};

struct plumos_fbdev_assets {
  char background[PLUMOS_FBDEV_RENDER_LINE_MAX];
  char gallery_background[PLUMOS_FBDEV_RENDER_LINE_MAX];
  char placeholder[PLUMOS_FBDEV_RENDER_LINE_MAX];
};

static const char *plumos_fbdev_ltrim(const char *s) {
  while (s && *s && isspace((unsigned char)*s)) {
    s++;
  }
  return s ? s : "";
}

static void plumos_fbdev_trim_right(char *s) {
  size_t len;
  if (!s) {
    return;
  }
  len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len - 1])) {
    s[--len] = '\0';
  }
}

static int plumos_fbdev_text_width(const char *text, int scale) {
  int width = 0;
  const char *p;
  if (scale <= 0) {
    scale = 1;
  }
  for (p = text; p && *p;) {
    unsigned int cp;
    const char *next = plumos_fbdev_utf8_next(p, &cp);
    if (cp == '\t') {
      cp = ' ';
    }
    if (cp >= 32) {
      width += 6 * scale * plumos_fbdev_utf8_cell_width(cp);
    }
    p = next;
  }
  return width;
}

static void plumos_fbdev_copy_range(char *out, size_t out_size,
                                    const char *start, const char *end) {
  size_t len;
  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!start) {
    return;
  }
  if (!end) {
    end = start + strlen(start);
  }
  while (start < end && isspace((unsigned char)*start)) {
    start++;
  }
  while (end > start && isspace((unsigned char)*(end - 1))) {
    end--;
  }
  len = (size_t)(end - start);
  if (len >= out_size) {
    len = out_size - 1;
  }
  memcpy(out, start, len);
  out[len] = '\0';
}

static void plumos_fbdev_copy_text(char *out, size_t out_size, const char *text) {
  plumos_fbdev_copy_range(out, out_size, text, text ? text + strlen(text) : NULL);
}

static int plumos_fbdev_split_setting_control(const char *in, char *label,
                                              size_t label_size, char *control,
                                              size_t control_size) {
  const char *choice;
  char *marker;

  if (!in || !label || label_size == 0 || !control || control_size == 0) {
    return 0;
  }
  label[0] = '\0';
  control[0] = '\0';

  if ((strncmp(in, "[x] ", 4) == 0 || strncmp(in, "[ ] ", 4) == 0) && in[4]) {
    plumos_fbdev_copy_range(control, control_size, in, in + 3);
    plumos_fbdev_copy_text(label, label_size, in + 4);
    return label[0] != '\0' && control[0] != '\0';
  }

  choice = strstr(in, " < ");
  if (!choice || !strstr(choice + 1, " >")) {
    return 0;
  }
  plumos_fbdev_copy_range(label, label_size, in, choice);
  plumos_fbdev_copy_text(control, control_size, choice + 1);
  marker = strstr(control, PLUMOS_FBDEV_SETTING_FLASH_MARKER);
  if (marker) {
    *marker = '\0';
  }
  plumos_fbdev_trim_right(control);
  return label[0] != '\0' && control[0] != '\0';
}

static int plumos_fbdev_hex_digit(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

static int plumos_fbdev_parse_hex_rgb(const char *value,
                                      uint8_t *red, uint8_t *green,
                                      uint8_t *blue) {
  int digits[6];
  int i;

  if (!value || value[0] != '#') {
    return 0;
  }
  for (i = 0; i < 6; i++) {
    digits[i] = plumos_fbdev_hex_digit(value[i + 1]);
    if (digits[i] < 0) {
      return 0;
    }
  }
  if (value[7] != '\0' && !isspace((unsigned char)value[7])) {
    return 0;
  }
  *red = (uint8_t)((digits[0] << 4) | digits[1]);
  *green = (uint8_t)((digits[2] << 4) | digits[3]);
  *blue = (uint8_t)((digits[4] << 4) | digits[5]);
  return 1;
}

static uint32_t plumos_fbdev_color_from_hex(struct plumos_fbdev_renderer *r,
                                            const char *value, uint32_t fallback) {
  uint8_t red;
  uint8_t green;
  uint8_t blue;

  if (!plumos_fbdev_parse_hex_rgb(value, &red, &green, &blue)) {
    return fallback;
  }
  return plumos_fbdev_pack_color(r, red, green, blue);
}

static void plumos_fbdev_palette_init(struct plumos_fbdev_renderer *r,
                                      struct plumos_fbdev_palette *p) {
  p->background = plumos_fbdev_pack_color(r, 5, 8, 10);
  p->foreground = plumos_fbdev_pack_color(r, 232, 242, 238);
  p->muted = plumos_fbdev_pack_color(r, 137, 160, 166);
  p->accent = plumos_fbdev_pack_color(r, 255, 132, 13);
  p->panel = plumos_fbdev_pack_color(r, 22, 31, 34);
  p->panel_inner = plumos_fbdev_pack_color(r, 7, 12, 13);
  p->media_panel = plumos_fbdev_pack_color(r, 25, 38, 42);
  p->selection_background = plumos_fbdev_pack_color(r, 35, 58, 51);
  p->selection_foreground = plumos_fbdev_pack_color(r, 255, 230, 122);
  p->danger = plumos_fbdev_pack_color(r, 255, 50, 36);
  p->line = plumos_fbdev_pack_color(r, 67, 88, 91);
}

static void plumos_fbdev_palette_apply(struct plumos_fbdev_renderer *r,
                                       struct plumos_fbdev_palette *p,
                                       const char *key, const char *value) {
  if (!key || !value) {
    return;
  }
  if (strcmp(key, "background") == 0) {
    p->background = plumos_fbdev_color_from_hex(r, value, p->background);
  } else if (strcmp(key, "foreground") == 0) {
    p->foreground = plumos_fbdev_color_from_hex(r, value, p->foreground);
  } else if (strcmp(key, "muted") == 0) {
    p->muted = plumos_fbdev_color_from_hex(r, value, p->muted);
  } else if (strcmp(key, "accent") == 0) {
    p->accent = plumos_fbdev_color_from_hex(r, value, p->accent);
  } else if (strcmp(key, "panel") == 0) {
    p->panel = plumos_fbdev_color_from_hex(r, value, p->panel);
  } else if (strcmp(key, "panel_inner") == 0) {
    p->panel_inner = plumos_fbdev_color_from_hex(r, value, p->panel_inner);
  } else if (strcmp(key, "media_panel") == 0) {
    p->media_panel = plumos_fbdev_color_from_hex(r, value, p->media_panel);
  } else if (strcmp(key, "selection_background") == 0) {
    p->selection_background =
        plumos_fbdev_color_from_hex(r, value, p->selection_background);
  } else if (strcmp(key, "selection_foreground") == 0) {
    p->selection_foreground =
        plumos_fbdev_color_from_hex(r, value, p->selection_foreground);
  } else if (strcmp(key, "danger") == 0) {
    p->danger = plumos_fbdev_color_from_hex(r, value, p->danger);
  }
}

static void plumos_fbdev_load_palette(struct plumos_fbdev_renderer *r,
                                      struct plumos_fbdev_palette *p,
                                      char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX],
                                      size_t line_count) {
  size_t i;

  plumos_fbdev_palette_init(r, p);
  for (i = 0; i < line_count; i++) {
    const char *line = plumos_fbdev_ltrim(lines[i]);
    const char *key;
    const char *value;
    const char *tab;
    char key_buf[64];
    char value_buf[64];

    if (strncmp(line, "graphic_theme_color\t", 20) != 0) {
      continue;
    }
    key = line + 20;
    tab = strchr(key, '\t');
    if (!tab) {
      continue;
    }
    value = tab + 1;
    plumos_fbdev_copy_range(key_buf, sizeof(key_buf), key, tab);
    plumos_fbdev_copy_text(value_buf, sizeof(value_buf), value);
    plumos_fbdev_palette_apply(r, p, key_buf, value_buf);
  }
}

static void plumos_fbdev_load_motion(struct plumos_fbdev_motion *motion,
                                     char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX],
                                     size_t line_count) {
  size_t i;

  if (!motion) {
    return;
  }
  memset(motion, 0, sizeof(*motion));
  plumos_fbdev_copy_text(motion->top_layout, sizeof(motion->top_layout),
                         "tile_grid");
  plumos_fbdev_copy_text(motion->transition_axis,
                         sizeof(motion->transition_axis), "vertical");
  plumos_fbdev_copy_text(motion->transition_easing,
                         sizeof(motion->transition_easing), "smoothstep");
  for (i = 0; i < line_count; i++) {
    const char *line = plumos_fbdev_ltrim(lines[i]);
    const char *key;
    const char *value;
    const char *tab;
    char key_buf[64];
    char value_buf[64];

    if (strncmp(line, "graphic_theme_motion\t", 21) != 0) {
      continue;
    }
    key = line + 21;
    tab = strchr(key, '\t');
    if (!tab) {
      continue;
    }
    value = tab + 1;
    plumos_fbdev_copy_range(key_buf, sizeof(key_buf), key, tab);
    plumos_fbdev_copy_text(value_buf, sizeof(value_buf), value);
    if (strcmp(key_buf, "top_layout") == 0 &&
        (strcmp(value_buf, "tile_grid") == 0 ||
         strcmp(value_buf, "tile_strip") == 0)) {
      plumos_fbdev_copy_text(motion->top_layout, sizeof(motion->top_layout),
                             value_buf);
    } else if (strcmp(key_buf, "transition_axis") == 0 &&
               (strcmp(value_buf, "horizontal") == 0 ||
                strcmp(value_buf, "vertical") == 0)) {
      plumos_fbdev_copy_text(motion->transition_axis,
                             sizeof(motion->transition_axis), value_buf);
    } else if (strcmp(key_buf, "transition_easing") == 0 && value_buf[0]) {
      plumos_fbdev_copy_text(motion->transition_easing,
                             sizeof(motion->transition_easing), value_buf);
    }
  }
}

static void plumos_fbdev_load_assets(struct plumos_fbdev_assets *assets,
                                     char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX],
                                     size_t line_count) {
  size_t i;

  if (!assets) {
    return;
  }
  memset(assets, 0, sizeof(*assets));
  for (i = 0; i < line_count; i++) {
    const char *line = plumos_fbdev_ltrim(lines[i]);
    const char *key;
    const char *value;
    const char *tab;
    char key_buf[64];
    char value_buf[PLUMOS_FBDEV_RENDER_LINE_MAX];

    if (strncmp(line, "graphic_theme_asset\t", 20) != 0) {
      continue;
    }
    key = line + 20;
    tab = strchr(key, '\t');
    if (!tab) {
      continue;
    }
    value = tab + 1;
    plumos_fbdev_copy_range(key_buf, sizeof(key_buf), key, tab);
    plumos_fbdev_copy_text(value_buf, sizeof(value_buf), value);
    if (strcmp(key_buf, "background") == 0) {
      plumos_fbdev_copy_text(assets->background, sizeof(assets->background),
                             value_buf);
    } else if (strcmp(key_buf, "gallery_background") == 0) {
      plumos_fbdev_copy_text(assets->gallery_background,
                             sizeof(assets->gallery_background), value_buf);
    } else if (strcmp(key_buf, "placeholder") == 0) {
      plumos_fbdev_copy_text(assets->placeholder, sizeof(assets->placeholder),
                             value_buf);
    }
  }
}

static void plumos_fbdev_draw_text_center(struct plumos_fbdev_renderer *r, int x,
                                          int y, int w, const char *text,
                                          int scale, uint32_t color) {
  int width = plumos_fbdev_text_width_for(r, text, scale);
  int draw_x = x + (w - width) / 2;
  if (draw_x < x) {
    draw_x = x;
  }
  plumos_fbdev_draw_text(r, draw_x, y, text, scale, color, x + w);
}

static void plumos_fbdev_time_label(char *out, size_t out_size) {
  time_t now;
  struct tm tm_now;

  if (!out || out_size == 0) {
    return;
  }
  now = time(NULL);
  if (now == (time_t)-1 || !localtime_r(&now, &tm_now)) {
    plumos_fbdev_copy_text(out, out_size, "--:--");
    return;
  }
  snprintf(out, out_size, "%02d:%02d", tm_now.tm_hour, tm_now.tm_min);
}

static int plumos_fbdev_read_first_line(const char *path, char *out,
                                        size_t out_size);

static void plumos_fbdev_wifi_label(char *out, size_t out_size) {
  FILE *f;
  char line[256];
  char value[32];
  int linked = 0;

  if (!out || out_size == 0) {
    return;
  }
  if (plumos_fbdev_read_first_line("/sys/class/net/wlan0/carrier", value,
                                   sizeof(value)) &&
      strcmp(value, "1") == 0) {
    linked = 1;
  }
  if (!linked &&
      plumos_fbdev_read_first_line("/sys/class/net/wlan0/operstate", value,
                                   sizeof(value)) &&
      strcmp(value, "up") == 0) {
    linked = 1;
  }
  f = fopen("/proc/net/wireless", "r");
  if (f) {
    while (fgets(line, sizeof(line), f)) {
      unsigned int status = 0;
      float quality = 0.0f;
      if (sscanf(line, " wlan0: %x %f", &status, &quality) == 2 &&
          quality > 0.0f) {
        linked = 1;
        break;
      }
    }
    fclose(f);
  }
  plumos_fbdev_copy_text(out, out_size, linked ? "WIFI" : "NO WIFI");
}

static int plumos_fbdev_read_first_line(const char *path, char *out,
                                        size_t out_size) {
  FILE *f;
  char *newline;

  if (!path || !out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  f = fopen(path, "r");
  if (!f) {
    return 0;
  }
  if (!fgets(out, (int)out_size, f)) {
    fclose(f);
    out[0] = '\0';
    return 0;
  }
  fclose(f);
  newline = strchr(out, '\n');
  if (newline) {
    *newline = '\0';
  }
  return out[0] != '\0';
}

static int plumos_fbdev_find_battery_path(char *out, size_t out_size) {
  static const char *const candidates[] = {
      "/sys/class/power_supply/battery",
      "/sys/class/power_supply/axp2202-battery",
  };
  DIR *dir;
  struct dirent *entry;
  char path[256];
  char type[32];
  char capacity[32];
  size_t i;

  if (!out || out_size == 0) {
    return 0;
  }
  out[0] = '\0';
  for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    snprintf(path, sizeof(path), "%s/type", candidates[i]);
    if (!plumos_fbdev_read_first_line(path, type, sizeof(type)) ||
        strcmp(type, "Battery") != 0) {
      continue;
    }
    snprintf(path, sizeof(path), "%s/capacity", candidates[i]);
    if (!plumos_fbdev_read_first_line(path, capacity, sizeof(capacity))) {
      continue;
    }
    plumos_fbdev_copy_text(out, out_size, candidates[i]);
    return 1;
  }

  dir = opendir("/sys/class/power_supply");
  if (!dir) {
    return 0;
  }
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.') {
      continue;
    }
    snprintf(path, sizeof(path), "/sys/class/power_supply/%.128s/type",
             entry->d_name);
    if (!plumos_fbdev_read_first_line(path, type, sizeof(type)) ||
        strcmp(type, "Battery") != 0) {
      continue;
    }
    snprintf(path, sizeof(path), "/sys/class/power_supply/%.128s/capacity",
             entry->d_name);
    if (!plumos_fbdev_read_first_line(path, capacity, sizeof(capacity))) {
      continue;
    }
    snprintf(out, out_size, "/sys/class/power_supply/%.128s", entry->d_name);
    closedir(dir);
    return 1;
  }
  closedir(dir);
  return 0;
}

static void plumos_fbdev_battery_label(char *out, size_t out_size) {
  char battery_path[256];
  char path[320];
  char capacity[32];
  char status[32];
  char *end;
  long percent;
  const char *prefix = "BAT";

  if (!out || out_size == 0) {
    return;
  }
  if (!plumos_fbdev_find_battery_path(battery_path, sizeof(battery_path))) {
    plumos_fbdev_copy_text(out, out_size, "BAT --");
    return;
  }
  snprintf(path, sizeof(path), "%s/capacity", battery_path);
  if (!plumos_fbdev_read_first_line(path, capacity, sizeof(capacity))) {
    plumos_fbdev_copy_text(out, out_size, "BAT --");
    return;
  }
  percent = strtol(capacity, &end, 10);
  if (end == capacity) {
    plumos_fbdev_copy_text(out, out_size, "BAT --");
    return;
  }
  if (percent < 0) {
    percent = 0;
  } else if (percent > 100) {
    percent = 100;
  }
  snprintf(path, sizeof(path), "%s/status", battery_path);
  if (plumos_fbdev_read_first_line(path, status, sizeof(status)) &&
      (strcmp(status, "Charging") == 0 || strcmp(status, "Full") == 0)) {
    prefix = "CHG";
  }
  snprintf(out, out_size, "%s %ld", prefix, percent);
}

static void plumos_fbdev_draw_graphic_top_bar_overlay(
    struct plumos_fbdev_renderer *r, const struct plumos_fbdev_palette *p,
    const char *title) {
  char time_label[16];
  char wifi_label[16];
  char battery_label[24];
  char right[80];
  int w = (int)r->var.xres;
  int right_width;
  int title_max_x;

  plumos_fbdev_time_label(time_label, sizeof(time_label));
  plumos_fbdev_wifi_label(wifi_label, sizeof(wifi_label));
  plumos_fbdev_battery_label(battery_label, sizeof(battery_label));
  snprintf(right, sizeof(right), "%s  %s  %s", time_label, wifi_label,
           battery_label);
  right_width = plumos_fbdev_text_width(right, 2);
  title_max_x = w - 28 - right_width;
  if (title_max_x < 96) {
    title_max_x = w - 16;
  }

  plumos_fbdev_fill_rect(r, 0, 0, w, 40, p->panel_inner);
  plumos_fbdev_fill_rect(r, 0, 40, w, 2, p->panel);
  plumos_fbdev_fill_rect(r, 0, 0, 5, (int)r->var.yres, p->accent);
  plumos_fbdev_draw_text(r, 16, 12,
                         title && title[0] ? title : "PLUMOS MF", 2,
                         p->foreground, title_max_x);
  plumos_fbdev_draw_text(r, w - 14 - right_width, 12, right, 2, p->muted,
                         w - 12);
}

static void plumos_fbdev_draw_graphic_top_bar(
    struct plumos_fbdev_renderer *r, const struct plumos_fbdev_palette *p,
    const char *title) {
  plumos_fbdev_fill_rect(r, 0, 0, (int)r->var.xres, (int)r->var.yres,
                         p->background);
  plumos_fbdev_draw_graphic_top_bar_overlay(r, p, title);
}

static void plumos_fbdev_draw_tty_top_bar(struct plumos_fbdev_renderer *r) {
  char time_label[16];
  char wifi_label[16];
  char battery_label[24];
  char right[80];
  int w = (int)r->var.xres;
  int right_width;
  uint32_t bg = plumos_fbdev_pack_color(r, 0, 5, 4);
  uint32_t border = plumos_fbdev_pack_color(r, 31, 82, 56);
  uint32_t prompt = plumos_fbdev_pack_color(r, 158, 255, 199);
  uint32_t muted = plumos_fbdev_pack_color(r, 179, 235, 219);

  plumos_fbdev_time_label(time_label, sizeof(time_label));
  plumos_fbdev_wifi_label(wifi_label, sizeof(wifi_label));
  plumos_fbdev_battery_label(battery_label, sizeof(battery_label));
  snprintf(right, sizeof(right), "%s  %s  %s", time_label, wifi_label,
           battery_label);
  right_width = plumos_fbdev_text_width(right, 2);

  plumos_fbdev_fill_rect(r, 0, 0, w, 34, bg);
  plumos_fbdev_fill_rect(r, 0, 34, w, 2, border);
  plumos_fbdev_draw_text(r, 14, 10, "PLUMOS MF", 2, prompt, w - 8);
  plumos_fbdev_draw_text(r, w - 14 - right_width, 10, right, 2, muted, w - 8);
}

static const char *plumos_fbdev_find_value(
    char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX], size_t line_count,
    const char *prefix) {
  size_t i;
  size_t prefix_len = strlen(prefix);

  for (i = 0; i < line_count; i++) {
    const char *line = plumos_fbdev_ltrim(lines[i]);
    if (strncmp(line, prefix, prefix_len) == 0) {
      return line + prefix_len;
    }
  }
  return NULL;
}

static void plumos_fbdev_screen_title(char *out, size_t out_size,
                                      char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX],
                                      size_t line_count) {
  const char *line;
  const char *title;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (line_count == 0) {
    plumos_fbdev_copy_text(out, out_size, "plumOS");
    return;
  }
  line = plumos_fbdev_ltrim(lines[0]);
  title = strstr(line, " - ");
  if (title) {
    title += 3;
  } else {
    title = line;
  }
  plumos_fbdev_copy_text(out, out_size, title);
  if (!out[0]) {
    plumos_fbdev_copy_text(out, out_size, "plumOS");
  }
}

static int plumos_fbdev_title_is_settings_family(const char *title) {
  return title && (strstr(title, "START") || strstr(title, "Apps") ||
                   strstr(title, "APPS") || strstr(title, "Settings") ||
                   strstr(title, "SETTINGS") || strstr(title, "HELP") ||
                   strstr(title, "Thumbnail Results") ||
                   strstr(title, "Scraping") ||
                   strstr(title, "Network") || strstr(title, "NETWORK"));
}

static int plumos_fbdev_title_is_rom_list(const char *title) {
  return title && (strstr(title, "ROMS") || strstr(title, "FAVORITES") ||
                   strstr(title, "RECENT"));
}

static int plumos_fbdev_has_prefixed_line(
    char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX], size_t line_count,
    const char *prefix) {
  size_t i;
  size_t prefix_len = prefix ? strlen(prefix) : 0;

  if (!prefix || prefix_len == 0) {
    return 0;
  }
  for (i = 0; i < line_count; i++) {
    const char *line = plumos_fbdev_ltrim(lines[i]);
    if (strncmp(line, prefix, prefix_len) == 0) {
      return 1;
    }
  }
  return 0;
}

static int plumos_fbdev_entry_head(const char *line, int *selected,
                                   char *number, size_t number_size,
                                   const char **rest, int *favorite) {
  const char *p = line;
  size_t n = 0;

  if (selected) {
    *selected = 0;
  }
  if (number && number_size > 0) {
    number[0] = '\0';
  }
  if (rest) {
    *rest = "";
  }
  if (favorite) {
    *favorite = 0;
  }
  if (!line || !rest) {
    return 0;
  }
  if (*p == '>') {
    if (selected) {
      *selected = 1;
    }
    p++;
  }
  while (*p == ' ') {
    p++;
  }
  if (*p == '*') {
    if (favorite) {
      *favorite = 1;
    }
    p++;
    while (*p == ' ') {
      p++;
    }
  }
  while (isdigit((unsigned char)*p) && number && n + 1 < number_size) {
    number[n++] = *p++;
  }
  if (number && number_size > 0) {
    number[n] = '\0';
  }
  while (isdigit((unsigned char)*p)) {
    p++;
  }
  while (*p == ' ') {
    p++;
  }
  *rest = p;
  return number && number[0] && *p;
}

static void plumos_fbdev_compact_menu_entry(const char *line, char *out,
                                            size_t out_size, int *selected) {
  char number[16];
  const char *rest;
  int favorite = 0;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!plumos_fbdev_entry_head(line, selected, number, sizeof(number), &rest,
                               &favorite)) {
    plumos_fbdev_copy_text(out, out_size, line);
    return;
  }
  if (favorite) {
    snprintf(out, out_size, "* %s", rest);
  } else {
    plumos_fbdev_copy_text(out, out_size, rest);
  }
}

static int plumos_fbdev_parse_graphic_entry(const char *line, const char *prefix,
                                            struct plumos_fbdev_entry *entry) {
  const char *p;
  const char *next;
  size_t prefix_len;

  if (!line || !prefix || !entry) {
    return 0;
  }
  prefix_len = strlen(prefix);
  if (strncmp(line, prefix, prefix_len) != 0 || line[prefix_len] != '\t') {
    return 0;
  }
  memset(entry, 0, sizeof(*entry));
  p = line + prefix_len + 1;
  entry->selected = (*p == '1');
  next = strchr(p, '\t');
  if (!next) {
    return 0;
  }
  p = next + 1;
  next = strchr(p, '\t');
  plumos_fbdev_copy_range(entry->title, sizeof(entry->title), p, next);
  if (next) {
    p = next + 1;
    next = strchr(p, '\t');
    plumos_fbdev_copy_range(entry->detail, sizeof(entry->detail), p, next);
  }
  if (next) {
    p = next + 1;
    next = strchr(p, '\t');
    plumos_fbdev_copy_range(entry->media, sizeof(entry->media), p, next);
  }
  if (!entry->title[0]) {
    plumos_fbdev_copy_text(entry->title, sizeof(entry->title), "Untitled");
  }
  return 1;
}

static size_t plumos_fbdev_collect_graphic_entries(
    char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX], size_t line_count,
    struct plumos_fbdev_entry *entries, size_t max_entries) {
  size_t i;
  size_t count = 0;

  if (!entries || max_entries == 0) {
    return 0;
  }
  for (i = 0; i < line_count && count < max_entries; i++) {
    const char *line = plumos_fbdev_ltrim(lines[i]);
    if (plumos_fbdev_parse_graphic_entry(line, "graphic_entry", &entries[count])) {
      count++;
    }
  }
  return count;
}

static size_t plumos_fbdev_collect_graphic_prev_entries(
    char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX], size_t line_count,
    struct plumos_fbdev_entry *entries, size_t max_entries) {
  size_t i;
  size_t count = 0;

  if (!entries || max_entries == 0) {
    return 0;
  }
  for (i = 0; i < line_count && count < max_entries; i++) {
    const char *line = plumos_fbdev_ltrim(lines[i]);
    if (plumos_fbdev_parse_graphic_entry(line, "graphic_prev_entry",
                                         &entries[count])) {
      count++;
    }
  }
  return count;
}

static size_t plumos_fbdev_selected_index(
    const struct plumos_fbdev_entry *entries, size_t count) {
  size_t i;

  if (!entries || count == 0) {
    return 0;
  }
  for (i = 0; i < count; i++) {
    if (entries[i].selected) {
      return i;
    }
  }
  return 0;
}

static const struct plumos_fbdev_entry *plumos_fbdev_selected_entry(
    const struct plumos_fbdev_entry *entries, size_t count) {
  return entries && count ? &entries[plumos_fbdev_selected_index(entries, count)]
                          : NULL;
}

static int plumos_fbdev_render_centered_progress(
    struct plumos_fbdev_renderer *r, const struct plumos_fbdev_palette *p,
    const char *line1, const char *line2, const char *line3,
    const char *line4) {
  int w = (int)r->var.xres;
  int h = (int)r->var.yres;
  int y1 = h / 2 - 112;
  int y2 = y1 + 64;
  int y3 = y2 + 76;
  int y4 = y3 + 34;
  uint32_t blue = plumos_fbdev_pack_color(r, 56, 148, 255);
  uint32_t yellow = plumos_fbdev_pack_color(r, 255, 219, 71);
  uint32_t text_color = plumos_fbdev_pack_color(r, 198, 240, 230);

  plumos_fbdev_fill_rect(r, 0, 0, w, h, p->background);
  plumos_fbdev_draw_tty_top_bar(r);
  plumos_fbdev_fill_rect(r, 0, 0, 7, h, blue);
  plumos_fbdev_draw_text_center(r, 0, y1, w, line1, 4, blue);
  plumos_fbdev_draw_text_center(r, 0, y2, w, line2, 4, yellow);
  plumos_fbdev_draw_text_center(r, 0, y3, w, line3, 2, text_color);
  plumos_fbdev_draw_text_center(r, 0, y4, w, line4, 2, text_color);
  return 1;
}

static int plumos_fbdev_render_top_refresh_running(
    struct plumos_fbdev_renderer *r,
    const struct plumos_fbdev_palette *p) {
  return plumos_fbdev_render_centered_progress(
      r, p, "REFRESH TOP", "PLEASE WAIT", "SCANNING SYSTEMS",
      "RELOADING TOP LIST");
}

static int plumos_fbdev_render_power_action_running(
    struct plumos_fbdev_renderer *r,
    char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX], size_t line_count,
    const struct plumos_fbdev_palette *p) {
  const char *title =
      plumos_fbdev_find_value(lines, line_count, "power_action_title=");
  const char *wait =
      plumos_fbdev_find_value(lines, line_count, "power_action_wait=");
  const char *saving =
      plumos_fbdev_find_value(lines, line_count, "power_action_saving=");
  const char *no_remove =
      plumos_fbdev_find_value(lines, line_count, "power_action_no_remove=");
  const char *action =
      plumos_fbdev_find_value(lines, line_count, "power_action=");

  if (!title || !title[0]) {
    title = action && strcmp(action, "reboot") == 0 ? "RESTARTING"
                                                     : "SHUTTING DOWN";
  }
  if (!wait || !wait[0]) {
    wait = "PLEASE WAIT";
  }
  if (!saving || !saving[0]) {
    saving = "SAVING DATA SAFELY";
  }
  if (!no_remove || !no_remove[0]) {
    no_remove = "DO NOT REMOVE THE SD CARD";
  }
  return plumos_fbdev_render_centered_progress(r, p, title, wait, saving,
                                                no_remove);
}

static void plumos_fbdev_entry_badge(char *out, size_t out_size,
                                     const char *title) {
  size_t pos = 0;
  const char *p;

  if (!out || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (!title || !title[0]) {
    plumos_fbdev_copy_text(out, out_size, "OS");
    return;
  }
  if (title[0] == '*') {
    plumos_fbdev_copy_text(out, out_size, "*");
    return;
  }
  if (strcasecmp(title, "favorites") == 0) {
    plumos_fbdev_copy_text(out, out_size, "FAV");
    return;
  }
  if (strcasecmp(title, "recent") == 0) {
    plumos_fbdev_copy_text(out, out_size, "REC");
    return;
  }
  for (p = title; *p && pos + 1 < out_size && pos < 3; p++) {
    if (isalnum((unsigned char)*p)) {
      out[pos++] = (char)toupper((unsigned char)*p);
    }
  }
  out[pos] = '\0';
  if (!out[0]) {
    plumos_fbdev_copy_text(out, out_size, "OS");
  }
}

static void plumos_fbdev_graphic_top_layout_metrics(
    struct plumos_fbdev_renderer *r, const struct plumos_fbdev_motion *motion,
    int *columns_out, int *rows_out, int *grid_x_out, int *grid_y_out,
    int *tile_size_out, int *gap_out) {
  int strip = motion && strcmp(motion->top_layout, "tile_strip") == 0;
  int columns = strip ? 2 : 3;
  int rows = strip ? 1 : 2;
  int horizontal_margin = strip ? 32 : 22;
  int grid_y = strip ? 116 : 70;
  int bottom_margin = strip ? 42 : 18;
  int gap = strip ? 16 : 10;
  int width_limited;
  int height_limited;
  int tile_size;
  int grid_width;
  int grid_x;

  width_limited = ((int)r->var.xres - horizontal_margin * 2 -
                   gap * (columns - 1)) /
                  columns;
  height_limited = ((int)r->var.yres - grid_y - bottom_margin -
                    gap * (rows - 1)) /
                   rows;
  tile_size = width_limited < height_limited ? width_limited : height_limited;
  if (strip && tile_size > 260) {
    tile_size = 260;
  }
  if (tile_size < 72) {
    tile_size = 72;
  }
  grid_width = tile_size * columns + gap * (columns - 1);
  grid_x = ((int)r->var.xres - grid_width) / 2;
  if (grid_x < 8) {
    grid_x = 8;
  }
  if (columns_out) {
    *columns_out = columns;
  }
  if (rows_out) {
    *rows_out = rows;
  }
  if (grid_x_out) {
    *grid_x_out = grid_x;
  }
  if (grid_y_out) {
    *grid_y_out = grid_y;
  }
  if (tile_size_out) {
    *tile_size_out = tile_size;
  }
  if (gap_out) {
    *gap_out = gap;
  }
}

static void plumos_fbdev_draw_top_tile(struct plumos_fbdev_renderer *r,
                                       const struct plumos_fbdev_palette *p,
                                       const struct plumos_fbdev_entry *entry,
                                       int x, int y, int w, int h) {
  uint32_t fill = entry->selected ? p->selection_background : p->panel_inner;
  uint32_t outline = entry->selected ? p->accent : p->panel;
  uint32_t title_color = entry->selected ? p->selection_foreground : p->foreground;
  int media_x = x + 14;
  int media_y = y + 14;
  int title_y = y + h - 58;
  int detail_y = y + h - 30;
  int media_w = w - 28;
  int media_h = title_y - media_y - 10;
  int title_scale = 2;
  int logo_drawn = 0;
  char badge[8];

  plumos_fbdev_entry_badge(badge, sizeof(badge), entry->title);
  if (media_h < 24) {
    media_h = 24;
  }
  plumos_fbdev_fill_rect(r, x, y, w, h, outline);
  plumos_fbdev_fill_rect(r, x + (entry->selected ? 4 : 2),
                         y + (entry->selected ? 4 : 2),
                         w - (entry->selected ? 8 : 4),
                         h - (entry->selected ? 8 : 4), fill);
  plumos_fbdev_fill_rect(r, media_x, media_y, media_w, media_h,
                         entry->selected ? p->selection_background
                                         : p->media_panel);
#ifdef PLUMOS_FBDEV_ENABLE_PNG
  if (entry->media[0]) {
    logo_drawn = plumos_fbdev_draw_png_contain(r, entry->media, media_x + 4,
                                               media_y + 4, media_w - 8,
                                               media_h - 8);
  }
#endif
  if (!logo_drawn) {
    plumos_fbdev_draw_text_center(
        r, media_x, media_y + (media_h - 28) / 2, media_w, badge, 4,
        entry->selected ? p->selection_foreground : p->foreground);
  }
  plumos_fbdev_draw_text(r, x + 12, title_y, entry->title, title_scale,
                         title_color, x + w - 12);
  if (entry->detail[0]) {
    plumos_fbdev_draw_text(r, x + 12, detail_y, entry->detail, 2, p->muted,
                           x + w - 12);
  }
}

static double plumos_fbdev_ease_progress(double progress, const char *easing);

static void plumos_fbdev_draw_top_entries(
    struct plumos_fbdev_renderer *r, const struct plumos_fbdev_palette *p,
    const struct plumos_fbdev_motion *motion,
    const struct plumos_fbdev_entry *entries, size_t count, int x_offset,
    int y_offset) {
  size_t i;
  int cols;
  int rows;
  int grid_x;
  int grid_y;
  int tile_size;
  int gap;

  if (!entries || count == 0) {
    return;
  }
  plumos_fbdev_graphic_top_layout_metrics(r, motion, &cols, &rows, &grid_x,
                                          &grid_y, &tile_size, &gap);
  for (i = 0; i < count && i < (size_t)(cols * rows); i++) {
    int col = (int)i % cols;
    int row = (int)i / cols;
    int x = grid_x + x_offset + col * (tile_size + gap);
    int y = grid_y + y_offset + row * (tile_size + gap);
    plumos_fbdev_draw_top_tile(r, p, &entries[i], x, y, tile_size, tile_size);
  }
}

static int plumos_fbdev_render_top(struct plumos_fbdev_renderer *r,
                                   char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX],
                                   size_t line_count,
                                   const struct plumos_fbdev_palette *p) {
  struct plumos_fbdev_entry entries[12];
  struct plumos_fbdev_entry prev_entries[12];
  struct plumos_fbdev_motion motion;
  size_t count;
  size_t prev_count;
  const char *transition;
  const char *transition_progress_text;
  const char *transition_direction_text;
  double transition_progress = 1.0;
  int transition_direction = 1;
  int current_x_offset = 0;
  int current_y_offset = 0;
  int prev_x_offset = 0;
  int prev_y_offset = 0;
  int slide_active;

  count = plumos_fbdev_collect_graphic_entries(lines, line_count, entries,
                                               sizeof(entries) / sizeof(entries[0]));
  if (count == 0) {
    memset(&entries[0], 0, sizeof(entries[0]));
    entries[0].selected = 1;
    plumos_fbdev_copy_text(entries[0].title, sizeof(entries[0].title), "No Systems");
    count = 1;
  }
  plumos_fbdev_load_motion(&motion, lines, line_count);
  prev_count = plumos_fbdev_collect_graphic_prev_entries(
      lines, line_count, prev_entries,
      sizeof(prev_entries) / sizeof(prev_entries[0]));
  transition = plumos_fbdev_find_value(lines, line_count, "graphic_transition=");
  transition_progress_text = plumos_fbdev_find_value(
      lines, line_count, "graphic_transition_progress=");
  transition_direction_text = plumos_fbdev_find_value(
      lines, line_count, "graphic_transition_direction=");
  if (transition_progress_text && transition_progress_text[0]) {
    transition_progress = strtod(transition_progress_text, NULL);
  }
  if (transition_direction_text && transition_direction_text[0]) {
    transition_direction = (int)strtol(transition_direction_text, NULL, 10);
  }
  if (transition_progress < 0.0) {
    transition_progress = 0.0;
  } else if (transition_progress > 1.0) {
    transition_progress = 1.0;
  }
  slide_active = transition && strcmp(transition, "slide") == 0 &&
                 prev_count > 0 && transition_progress < 1.0;

  plumos_fbdev_fill_rect(r, 0, 0, (int)r->var.xres, (int)r->var.yres,
                         p->background);
  if (slide_active) {
    int horizontal = strcmp(motion.transition_axis, "horizontal") == 0;
    int distance = horizontal ? (int)r->var.xres : (int)r->var.yres;
    double eased_progress = plumos_fbdev_ease_progress(
        transition_progress, motion.transition_easing);
    int direction = transition_direction < 0 ? -1 : 1;
    int prev_offset = -(direction * (int)((double)distance * eased_progress));
    int current_offset =
        direction * (int)((double)distance * (1.0 - eased_progress));

    if (horizontal) {
      prev_x_offset = prev_offset;
      current_x_offset = current_offset;
    } else {
      prev_y_offset = prev_offset;
      current_y_offset = current_offset;
    }
    plumos_fbdev_draw_top_entries(r, p, &motion, prev_entries, prev_count,
                                  prev_x_offset, prev_y_offset);
  }
  plumos_fbdev_draw_top_entries(r, p, &motion, entries, count,
                                current_x_offset, current_y_offset);
  plumos_fbdev_draw_graphic_top_bar_overlay(r, p, "PLUMOS MF");
  return 1;
}

static void plumos_fbdev_draw_rom_preview(struct plumos_fbdev_renderer *r,
                                          const struct plumos_fbdev_palette *p,
                                          const struct plumos_fbdev_entry *entry,
                                          int x, int y, int w, int h) {
  int media_x = x + 16;
  int media_y = y + 18;
  int media_w = w - 32;
  int media_h = 156;
  int thumbnail_drawn = 0;
  char badge[8];
  int text_x = x + 16;
  int text_right_x = x + w - 16;
  int text_available_w = text_right_x - text_x;
  int title_scroll_px = 0;
  int detail_scroll_px = 0;

  plumos_fbdev_fill_rect(r, x, y, w, h, p->panel);
  plumos_fbdev_fill_rect(r, x + 3, y + 3, w - 6, h - 6, p->panel_inner);
  plumos_fbdev_fill_rect(r, media_x, media_y, media_w, media_h,
                         p->media_panel);
  if (entry && entry->media[0]) {
#ifdef PLUMOS_FBDEV_ENABLE_PNG
    thumbnail_drawn =
        plumos_fbdev_draw_png_contain(r, entry->media, media_x, media_y,
                                      media_w, media_h);
#endif
  }
  if (entry) {
    if (!thumbnail_drawn) {
      plumos_fbdev_entry_badge(badge, sizeof(badge), entry->title);
      plumos_fbdev_draw_text_center(r, media_x, media_y + 58, media_w, badge, 4,
                                    p->foreground);
    }
    if (text_available_w > 0) {
      int title_width =
          plumos_fbdev_text_width_font(r, entry->title, 2, 1);
      int detail_width =
          plumos_fbdev_text_width_font(r, entry->detail, 2, 1);
      if (title_width > text_available_w) {
        title_scroll_px =
            plumos_fbdev_marquee_offset(r, title_width, text_available_w, 12);
      }
      if (detail_width > text_available_w) {
        detail_scroll_px =
            plumos_fbdev_marquee_offset(r, detail_width, text_available_w, 12);
      }
    }
    plumos_fbdev_draw_text_clipped(r, text_x - title_scroll_px, y + 200,
                                   entry->title, 2, 1, text_x, text_right_x,
                                   p->foreground);
    if (entry->detail[0]) {
      plumos_fbdev_draw_text_clipped(r, text_x - detail_scroll_px, y + 232,
                                     entry->detail, 2, 1, text_x,
                                     text_right_x, p->muted);
    }
  } else {
    plumos_fbdev_draw_text_center(r, media_x, media_y + 58, media_w, "NO ART",
                                  3, p->muted);
  }
}

static int plumos_fbdev_render_roms(struct plumos_fbdev_renderer *r,
                                    char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX],
                                    size_t line_count,
                                    const struct plumos_fbdev_palette *p) {
  struct plumos_fbdev_entry entries[14];
  const struct plumos_fbdev_entry *selected;
  const char *system_value;
  char title[160];
  size_t count;
  size_t i;
  int w = (int)r->var.xres;
  int h = (int)r->var.yres;
  int list_x = 18;
  int list_y = 70;
  int list_w = w > 680 ? 360 : w / 2 - 28;
  int row_h = 38;
  int preview_x = list_x + list_w + 16;
  int preview_y = 72;
  int preview_w = w - preview_x - 16;
  int preview_h = h - preview_y - 16;

  if (list_w < 190) {
    list_w = 190;
  }
  if (preview_w < 160) {
    preview_w = 160;
  }

  system_value = plumos_fbdev_find_value(lines, line_count, "graphic_system=");
  if (system_value && system_value[0]) {
    plumos_fbdev_copy_text(title, sizeof(title), system_value);
  } else {
    plumos_fbdev_screen_title(title, sizeof(title), lines, line_count);
  }

  count = plumos_fbdev_collect_graphic_entries(lines, line_count, entries,
                                               sizeof(entries) / sizeof(entries[0]));
  if (count == 0) {
    memset(&entries[0], 0, sizeof(entries[0]));
    entries[0].selected = 1;
    plumos_fbdev_copy_text(entries[0].title, sizeof(entries[0].title), "No Entries");
    count = 1;
  }
  selected = plumos_fbdev_selected_entry(entries, count);

  plumos_fbdev_draw_graphic_top_bar(r, p, title);

  for (i = 0; i < count; i++) {
    int y = list_y + (int)i * row_h;
    int name_x = list_x + 24;
    int name_right_x = list_x + list_w - 10;
    int scroll_px = 0;
    uint32_t fg = entries[i].selected ? p->selection_foreground : p->foreground;
    if (y + row_h > h - 20) {
      break;
    }
    if (entries[i].selected) {
      plumos_fbdev_fill_rect(r, list_x - 6, y - 7, list_w, row_h - 4,
                             p->selection_background);
      if (name_right_x > name_x) {
        int title_width =
            plumos_fbdev_text_width_font(r, entries[i].title, 2, 1);
        int available_width = name_right_x - name_x;
        if (title_width > available_width) {
          scroll_px =
              plumos_fbdev_marquee_offset(r, title_width, available_width, 12);
        }
      }
    }
    plumos_fbdev_draw_text(r, list_x, y + 3, entries[i].selected ? ">" : " ",
                           2, fg, name_right_x);
    plumos_fbdev_draw_text_clipped(r, name_x - scroll_px, y,
                                   entries[i].title, 2, 1, name_x,
                                   name_right_x, fg);
  }

  plumos_fbdev_draw_rom_preview(r, p, selected, preview_x, preview_y,
                                preview_w, preview_h);
  return 1;
}

struct plumos_fbdev_gallery_slot {
  int x;
  int y;
  int w;
  int h;
};

static int plumos_fbdev_min_int(int a, int b) {
  return a < b ? a : b;
}

static int plumos_fbdev_lerp_int(int from, int to, double progress) {
  double value;

  if (progress < 0.0) {
    progress = 0.0;
  } else if (progress > 1.0) {
    progress = 1.0;
  }
  value = (double)from + (double)(to - from) * progress;
  return (int)(value + (value >= 0.0 ? 0.5 : -0.5));
}

static double plumos_fbdev_ease_progress(double progress, const char *easing) {
  if (progress < 0.0) {
    progress = 0.0;
  } else if (progress > 1.0) {
    progress = 1.0;
  }
  if (easing && strcmp(easing, "linear") == 0) {
    return progress;
  }
  return progress * progress * (3.0 - 2.0 * progress);
}

static void plumos_fbdev_gallery_slot(
    const struct plumos_fbdev_renderer *r, long rel,
    struct plumos_fbdev_gallery_slot *slot) {
  int screen_w = r ? (int)r->var.xres : 640;
  int screen_h = r ? (int)r->var.yres : 480;
  int center_w = plumos_fbdev_min_int(360, screen_w - 170);
  int footer_y = screen_h - 84;
  int center_y = 84;
  int center_h;
  int max_center_h;
  int side_w;
  int side_h;
  int center_x;
  int side_y;
  int visible_side = 76;
  int offscreen_gap = 18;
  int side_step;

  if (!slot) {
    return;
  }
  if (center_w < 180) {
    center_w = screen_w > 96 ? screen_w - 96 : screen_w;
  }
  center_h = center_w * 3 / 4;
  max_center_h = footer_y - center_y - 24;
  if (max_center_h < 120) {
    max_center_h = 120;
  }
  if (center_h > max_center_h) {
    center_h = max_center_h;
    center_w = center_h * 4 / 3;
  }
  side_w = center_w * 2 / 3;
  side_h = center_h * 2 / 3;
  center_x = (screen_w - center_w) / 2;
  side_y = center_y + (center_h - side_h) / 2;
  if (visible_side > side_w / 2) {
    visible_side = side_w / 2;
  }
  side_step = side_w + offscreen_gap;

  slot->w = rel == 0 ? center_w : side_w;
  slot->h = rel == 0 ? center_h : side_h;
  slot->y = rel == 0 ? center_y : side_y;
  if (rel == 0) {
    slot->x = center_x;
  } else if (rel == -1) {
    slot->x = -side_w + visible_side;
  } else if (rel == 1) {
    slot->x = screen_w - visible_side;
  } else if (rel <= -2) {
    slot->x = -side_w - offscreen_gap + (int)(rel + 2) * side_step;
  } else {
    slot->x = screen_w + offscreen_gap + (int)(rel - 2) * side_step;
  }
}

static void plumos_fbdev_draw_gallery_background(
    struct plumos_fbdev_renderer *r, const struct plumos_fbdev_palette *p,
    const struct plumos_fbdev_assets *assets) {
  int row;
  int w = (int)r->var.xres;
  int h = (int)r->var.yres;
  int wall_top = 40;
  int shelf_y = h - 145;
  uint32_t mortar = plumos_fbdev_pack_color(r, 38, 20, 11);
  uint32_t brick = plumos_fbdev_pack_color(r, 33, 18, 10);
  uint32_t shelf = plumos_fbdev_pack_color(r, 89, 46, 20);
  uint32_t shelf_shadow = plumos_fbdev_pack_color(r, 43, 20, 9);

#ifdef PLUMOS_FBDEV_ENABLE_PNG
  if (assets && assets->gallery_background[0] &&
      plumos_fbdev_draw_png_cover(r, assets->gallery_background, 0, 0, w, h)) {
    return;
  }
  if (assets && assets->background[0] &&
      plumos_fbdev_draw_png_cover(r, assets->background, 0, 0, w, h)) {
    return;
  }
#else
  (void)assets;
#endif

  plumos_fbdev_fill_rect(r, 0, 0, w, h, p->background);
  for (row = 0; row < 11; row++) {
    int y = wall_top + row * 27;
    int offset = (row % 2) ? 38 : 0;
    int x;

    plumos_fbdev_fill_rect(r, 0, y, w, 2, mortar);
    for (x = -offset; x < w; x += 76) {
      plumos_fbdev_fill_rect(r, x, y, 2, 27, brick);
    }
  }
  plumos_fbdev_fill_rect(r, 0, shelf_y, w, 20, shelf);
  plumos_fbdev_fill_rect(r, 0, shelf_y + 20, w, 26, shelf_shadow);
  plumos_fbdev_fill_rect(r, 0, shelf_y + 45, w, h - shelf_y - 45,
                         p->panel_inner);
}

static void plumos_fbdev_draw_gallery_top_bar(
    struct plumos_fbdev_renderer *r, const struct plumos_fbdev_palette *p,
    const char *title) {
  char time_label[16];
  char wifi_label[16];
  char battery_label[24];
  char right[80];
  int w = (int)r->var.xres;
  int right_width;
  int title_max_x;

  plumos_fbdev_time_label(time_label, sizeof(time_label));
  plumos_fbdev_wifi_label(wifi_label, sizeof(wifi_label));
  plumos_fbdev_battery_label(battery_label, sizeof(battery_label));
  snprintf(right, sizeof(right), "%s  %s  %s", time_label, wifi_label,
           battery_label);
  right_width = plumos_fbdev_text_width(right, 2);
  title_max_x = w - 28 - right_width;
  if (title_max_x < 96) {
    title_max_x = w - 16;
  }

  plumos_fbdev_fill_rect(r, 0, 0, w, 40, p->panel_inner);
  plumos_fbdev_fill_rect(r, 0, 40, w, 2, p->panel);
  plumos_fbdev_draw_text(r, 16, 12,
                         title && title[0] ? title : "GALLERY", 2,
                         p->foreground, title_max_x);
  plumos_fbdev_draw_text(r, w - 14 - right_width, 12, right, 2, p->muted,
                         w - 12);
}

static void plumos_fbdev_draw_gallery_card(
    struct plumos_fbdev_renderer *r, const struct plumos_fbdev_palette *p,
    const struct plumos_fbdev_assets *assets,
    const struct plumos_fbdev_entry *entry, int x, int y, int w, int h,
    int selected) {
  char badge[8];
  int scale = selected ? 5 : 4;
  int initials_width;
  int thumbnail_drawn = 0;
  uint32_t outline = selected ? p->accent : p->panel;
  uint32_t fill = selected ? p->selection_background : p->media_panel;

  if (!entry || w <= 0 || h <= 0) {
    return;
  }
  plumos_fbdev_fill_rect(r, x - 4, y - 4, w + 8, h + 8, outline);
  plumos_fbdev_fill_rect(r, x, y, w, h, fill);
#ifdef PLUMOS_FBDEV_ENABLE_PNG
  if (entry->media[0]) {
    thumbnail_drawn = plumos_fbdev_draw_png_contain(r, entry->media, x, y, w, h);
  }
  if (!thumbnail_drawn && assets && assets->placeholder[0]) {
    thumbnail_drawn =
        plumos_fbdev_draw_png_contain(r, assets->placeholder, x, y, w, h);
  }
#else
  (void)assets;
#endif
  if (!thumbnail_drawn) {
    plumos_fbdev_entry_badge(badge, sizeof(badge), entry->title);
    initials_width = plumos_fbdev_text_width(badge, scale);
    plumos_fbdev_draw_text(r, x + (w - initials_width) / 2,
                           y + h * 45 / 100, badge, scale,
                           selected ? p->selection_foreground : p->foreground,
                           x + w - 8);
  }
}

static void plumos_fbdev_draw_gallery_entries(
    struct plumos_fbdev_renderer *r, const struct plumos_fbdev_palette *p,
    const struct plumos_fbdev_assets *assets,
    const struct plumos_fbdev_entry *entries, size_t entry_count,
    int x_offset) {
  size_t selected_index;
  size_t i;
  int pass;

  if (!entries || entry_count == 0) {
    return;
  }
  selected_index = plumos_fbdev_selected_index(entries, entry_count);
  for (pass = 0; pass < 2; pass++) {
    for (i = 0; i < entry_count; i++) {
      long rel = (long)i - (long)selected_index;
      struct plumos_fbdev_gallery_slot slot;

      if (rel < -2 || rel > 2) {
        continue;
      }
      if ((pass == 0 && rel == 0) || (pass == 1 && rel != 0)) {
        continue;
      }
      plumos_fbdev_gallery_slot(r, rel, &slot);
      plumos_fbdev_draw_gallery_card(r, p, assets, &entries[i],
                                     slot.x + x_offset, slot.y, slot.w, slot.h,
                                     rel == 0);
    }
  }
}

static void plumos_fbdev_draw_gallery_transition_entries(
    struct plumos_fbdev_renderer *r, const struct plumos_fbdev_palette *p,
    const struct plumos_fbdev_assets *assets,
    const struct plumos_fbdev_entry *entries, size_t entry_count,
    const struct plumos_fbdev_entry *prev_entries, size_t prev_entry_count,
    int transition_direction, double transition_progress) {
  size_t selected_index;
  size_t i;
  int pass;

  (void)entries;
  (void)entry_count;
  if (!prev_entries || prev_entry_count == 0) {
    plumos_fbdev_draw_gallery_entries(r, p, assets, entries, entry_count, 0);
    return;
  }
  if (transition_progress < 0.0) {
    transition_progress = 0.0;
  } else if (transition_progress > 1.0) {
    transition_progress = 1.0;
  }
  selected_index = plumos_fbdev_selected_index(prev_entries, prev_entry_count);
  for (pass = 0; pass < 2; pass++) {
    for (i = 0; i < prev_entry_count; i++) {
      long rel = (long)i - (long)selected_index;
      long target_rel = rel - (transition_direction < 0 ? -1 : 1);
      int target_selected = target_rel == 0;
      struct plumos_fbdev_gallery_slot from_slot;
      struct plumos_fbdev_gallery_slot to_slot;
      int x;
      int y;
      int w;
      int h;

      if (rel < -2 || rel > 2) {
        continue;
      }
      if ((pass == 0 && target_selected) ||
          (pass == 1 && !target_selected)) {
        continue;
      }
      plumos_fbdev_gallery_slot(r, rel, &from_slot);
      plumos_fbdev_gallery_slot(r, target_rel, &to_slot);
      x = plumos_fbdev_lerp_int(from_slot.x, to_slot.x, transition_progress);
      y = plumos_fbdev_lerp_int(from_slot.y, to_slot.y, transition_progress);
      w = plumos_fbdev_lerp_int(from_slot.w, to_slot.w, transition_progress);
      h = plumos_fbdev_lerp_int(from_slot.h, to_slot.h, transition_progress);
      plumos_fbdev_draw_gallery_card(r, p, assets, &prev_entries[i], x, y, w, h,
                                     target_selected);
    }
  }
}

static void plumos_fbdev_draw_gallery_footer(
    struct plumos_fbdev_renderer *r, const struct plumos_fbdev_palette *p,
    const struct plumos_fbdev_entry *selected) {
  int w = (int)r->var.xres;
  int h = (int)r->var.yres;
  int footer_y = h - 84;
  int text_left = 86;
  int text_right = w - 86;
  const char *title =
      selected && selected->title[0] ? selected->title : "NO ENTRY";
  int title_width = plumos_fbdev_text_width_font(r, title, 3, 1);
  int available_width;
  int scroll_px = 0;
  int title_x;

  if (text_right <= text_left) {
    text_left = 24;
    text_right = w - 24;
  }
  available_width = text_right - text_left;
  if (title_width > available_width) {
    scroll_px =
        plumos_fbdev_marquee_offset(r, title_width, available_width, 12);
    title_x = text_left - scroll_px;
  } else {
    title_x = (w - title_width) / 2;
  }
  plumos_fbdev_fill_rect(r, 0, footer_y, w, 84, p->panel_inner);
  plumos_fbdev_fill_rect(r, 0, footer_y, w, 2, p->accent);
  plumos_fbdev_draw_text_clipped(r, title_x, footer_y + 23, title, 3, 1,
                                 text_left, text_right,
                                 p->selection_foreground);
  plumos_fbdev_draw_text(r, 24, footer_y + 27, "<", 3, p->muted, w - 24);
  plumos_fbdev_draw_text(r, w - 42, footer_y + 27, ">", 3, p->muted, w - 16);
}

static int plumos_fbdev_render_gallery(
    struct plumos_fbdev_renderer *r,
    char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX], size_t line_count,
    const struct plumos_fbdev_palette *p) {
  struct plumos_fbdev_entry entries[16];
  struct plumos_fbdev_entry prev_entries[16];
  struct plumos_fbdev_assets assets;
  struct plumos_fbdev_motion motion;
  const struct plumos_fbdev_entry *selected;
  const char *system_value;
  const char *transition;
  const char *transition_progress_text;
  const char *transition_direction_text;
  char title[160];
  size_t entry_count;
  size_t prev_entry_count;
  double transition_progress = 1.0;
  int transition_direction = 1;
  int slide_active;

  system_value = plumos_fbdev_find_value(lines, line_count, "graphic_system=");
  if (system_value && system_value[0]) {
    plumos_fbdev_copy_text(title, sizeof(title), system_value);
  } else {
    plumos_fbdev_copy_text(title, sizeof(title), "GALLERY");
  }
  plumos_fbdev_load_assets(&assets, lines, line_count);
  plumos_fbdev_load_motion(&motion, lines, line_count);
  entry_count = plumos_fbdev_collect_graphic_entries(
      lines, line_count, entries, sizeof(entries) / sizeof(entries[0]));
  prev_entry_count = plumos_fbdev_collect_graphic_prev_entries(
      lines, line_count, prev_entries,
      sizeof(prev_entries) / sizeof(prev_entries[0]));
  if (entry_count == 0) {
    memset(&entries[0], 0, sizeof(entries[0]));
    entries[0].selected = 1;
    plumos_fbdev_copy_text(entries[0].title, sizeof(entries[0].title),
                           "No Entries");
    entry_count = 1;
  }
  selected = plumos_fbdev_selected_entry(entries, entry_count);
  transition = plumos_fbdev_find_value(lines, line_count, "graphic_transition=");
  transition_progress_text =
      plumos_fbdev_find_value(lines, line_count,
                              "graphic_transition_progress=");
  transition_direction_text =
      plumos_fbdev_find_value(lines, line_count,
                              "graphic_transition_direction=");
  if (transition_progress_text && transition_progress_text[0]) {
    transition_progress = strtod(transition_progress_text, NULL);
  }
  if (transition_direction_text && transition_direction_text[0]) {
    transition_direction = (int)strtol(transition_direction_text, NULL, 10);
  }
  if (transition_progress < 0.0) {
    transition_progress = 0.0;
  } else if (transition_progress > 1.0) {
    transition_progress = 1.0;
  }
  slide_active = transition && strcmp(transition, "slide") == 0 &&
                 prev_entry_count > 0 && transition_progress < 1.0;

  plumos_fbdev_draw_gallery_background(r, p, &assets);
  if (slide_active) {
    transition_progress =
        plumos_fbdev_ease_progress(transition_progress,
                                   motion.transition_easing);
    plumos_fbdev_draw_gallery_transition_entries(
        r, p, &assets, entries, entry_count, prev_entries, prev_entry_count,
        transition_direction < 0 ? -1 : 1, transition_progress);
  } else {
    plumos_fbdev_draw_gallery_entries(r, p, &assets, entries, entry_count, 0);
  }
  plumos_fbdev_draw_gallery_top_bar(r, p, title);
  plumos_fbdev_draw_gallery_footer(r, p, selected);
  return 1;
}

static int plumos_fbdev_is_hidden_line(const char *line) {
  if (!line || !line[0]) {
    return 1;
  }
  if (strncmp(line, "graphic_", 8) == 0 ||
      strncmp(line, "entries=", 8) == 0 ||
      strncmp(line, "system=", 7) == 0 ||
      strncmp(line, "target=", 7) == 0 ||
      strncmp(line, "profile=", 8) == 0 ||
      strncmp(line, "source=", 7) == 0 ||
      strncmp(line, "source:", 7) == 0 ||
      strncmp(line, "core_settings_screen=", 21) == 0 ||
      strncmp(line, "menu_screen=", 12) == 0 ||
      strncmp(line, "settings_screen=", 16) == 0 ||
      strncmp(line, "scraping_screen=", 16) == 0 ||
      strncmp(line, "thumbnail_results_screen=", 25) == 0 ||
      strncmp(line, "thumbnail_running", 17) == 0 ||
      strncmp(line, "top_refresh_running=", 20) == 0 ||
      strncmp(line, "power_action", 12) == 0 ||
      strncmp(line, "brightness_test=", 16) == 0 ||
      strncmp(line, "wifi_keyboard_cursor=", 21) == 0 ||
      strncmp(line, "wifi_password=", 14) == 0 ||
      strncmp(line, "prompt_path=", 12) == 0 ||
      strncmp(line, "footer", 6) == 0 ||
      strncmp(line, "status:", 7) == 0) {
    return 1;
  }
  if (strstr(line, "LEFT/RIGHT:") || strstr(line, "Q: quit") ||
      (strstr(line, "A:") &&
       (strstr(line, "B:") || strstr(line, "START:") ||
        strstr(line, "SELECT:") || strstr(line, "POWER:")))) {
    return 1;
  }
  return 0;
}

static int plumos_fbdev_find_wifi_keyboard_cursor(
    char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX], size_t line_count,
    int *row_out, int *col_out) {
  const char *value;
  char *endptr;
  long row;
  long col;

  if (row_out) {
    *row_out = -1;
  }
  if (col_out) {
    *col_out = -1;
  }
  value = plumos_fbdev_find_value(lines, line_count,
                                  "wifi_keyboard_cursor=");
  if (!value) {
    return 0;
  }
  row = strtol(value, &endptr, 10);
  if (endptr == value || *endptr != ',') {
    return 0;
  }
  value = endptr + 1;
  col = strtol(value, &endptr, 10);
  if (endptr == value || row < 0 || col < 0) {
    return 0;
  }
  if (row_out) {
    *row_out = (int)row;
  }
  if (col_out) {
    *col_out = (int)col;
  }
  return 1;
}

static void plumos_fbdev_draw_text_token_row(
    struct plumos_fbdev_renderer *r, const char *text, int selected_token,
    int x, int y, int scale, int max_x, uint32_t color) {
  const char *p = text;
  int pen_x = 0;
  int token_index = 0;
  int space_width = plumos_fbdev_text_width_font(r, " ", scale, 1);
  uint32_t selected_fg = plumos_fbdev_pack_color(r, 255, 224, 102);
  uint32_t selected_bg = plumos_fbdev_pack_color(r, 56, 24, 5);

  if (!text || scale <= 0) {
    return;
  }
  while (*p) {
    char token[32];
    size_t token_len = 0;
    size_t consumed;
    int token_width;
    int is_selected;

    while (*p == ' ') {
      pen_x += space_width;
      p++;
    }
    if (!*p || x + pen_x >= max_x) {
      break;
    }
    while (p[token_len] && p[token_len] != ' ' &&
           token_len + 1 < sizeof(token)) {
      token[token_len] = p[token_len];
      token_len++;
    }
    token[token_len] = '\0';
    consumed = token_len;
    while (p[consumed] && p[consumed] != ' ') {
      consumed++;
    }
    p += consumed;
    if (!token[0]) {
      continue;
    }

    token_width = plumos_fbdev_text_width_font(r, token, scale, 1);
    is_selected = token_index == selected_token;
    if (is_selected) {
      plumos_fbdev_fill_rect(r, x + pen_x - 3, y - 4, token_width + 6,
                             scale * 7 + 8, selected_bg);
    }
    plumos_fbdev_draw_text_font(r, x + pen_x, y, token, scale, 1,
                                is_selected ? selected_fg : color, max_x);
    pen_x += token_width;
    token_index++;
  }
}

static int plumos_fbdev_render_generic(struct plumos_fbdev_renderer *r,
                                       char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX],
                                       size_t line_count,
                                       const struct plumos_fbdev_palette *p) {
  char title[128];
  char items[18][PLUMOS_FBDEV_RENDER_LINE_MAX];
  int selected[18];
  size_t item_count = 0;
  size_t i;
  int w = (int)r->var.xres;
  int h = (int)r->var.yres;
  int settings_family;
  int settings_page;
  int core_settings_page;
  int rom_list_page;
  int entry_scale;
  int line_height;
  int cursor_x;
  int name_x;
  int right_x;
  int cell_width;
  int y;
  int wifi_connect_page;
  int wifi_keyboard_row = -1;
  int wifi_keyboard_col = -1;
  uint32_t accent;
  const char *footer1;
  const char *footer2;
  const char *wifi_password;

  memset(selected, 0, sizeof(selected));
  plumos_fbdev_screen_title(title, sizeof(title), lines, line_count);
  rom_list_page = plumos_fbdev_title_is_rom_list(title);
  core_settings_page = plumos_fbdev_has_prefixed_line(lines, line_count,
                                                     "core_settings_screen=1");
  settings_page = core_settings_page ||
                  plumos_fbdev_has_prefixed_line(lines, line_count,
                                                 "settings_screen=1");
  settings_family = plumos_fbdev_title_is_settings_family(title) ||
                    plumos_fbdev_has_prefixed_line(lines, line_count,
                                                   "menu_screen=1") ||
                    settings_page;
  entry_scale = core_settings_page ? 3 : 2;
  line_height = entry_scale * 12;
  cursor_x = settings_family ? 12 : 18;
  name_x = cursor_x + (settings_family ? 18 : 24);
  right_x = w - 14;
  cell_width = 6 * entry_scale;
  accent = settings_family ? plumos_fbdev_pack_color(r, 56, 148, 255)
                           : p->accent;
  footer1 = plumos_fbdev_find_value(lines, line_count, "footer1=");
  footer2 = plumos_fbdev_find_value(lines, line_count, "footer2=");
  wifi_password = plumos_fbdev_find_value(lines, line_count, "wifi_password=");
  wifi_connect_page = strstr(title, "Connect Wi-Fi") != NULL;
  plumos_fbdev_find_wifi_keyboard_cursor(lines, line_count,
                                         &wifi_keyboard_row,
                                         &wifi_keyboard_col);

  for (i = 1; i < line_count && item_count < 18; i++) {
    const char *line = plumos_fbdev_ltrim(lines[i]);
    int is_selected = 0;
    if (plumos_fbdev_is_hidden_line(line)) {
      continue;
    }
    plumos_fbdev_compact_menu_entry(line, items[item_count],
                                    sizeof(items[item_count]), &is_selected);
    selected[item_count] = is_selected;
    item_count++;
  }
  if (item_count == 0) {
    plumos_fbdev_copy_text(items[0], sizeof(items[0]), "Ready");
    selected[0] = 1;
    item_count = 1;
  }

  plumos_fbdev_fill_rect(r, 0, 0, w, h, p->background);
  plumos_fbdev_draw_tty_top_bar(r);
  plumos_fbdev_fill_rect(r, 0, 0, 5, h, accent);
  plumos_fbdev_draw_text_font(r, 14, 48, title, 2, wifi_connect_page,
                              p->muted, w - 8);
  y = settings_family ? 82 : 104;

  for (i = 0; i < item_count; i++) {
    int keyboard_row = wifi_keyboard_row >= 0;
    int keyboard_selected = keyboard_row && (int)i == wifi_keyboard_row;
    uint32_t fg = selected[i] ? p->selection_foreground : p->foreground;
    if (y > h - 34) {
      break;
    }
    if (selected[i] && !keyboard_selected) {
      plumos_fbdev_fill_rect(r, 10, y - 7, w - 20,
                             entry_scale * 7 + 10,
                             p->selection_background);
    }
    plumos_fbdev_draw_text(r, cursor_x, y, selected[i] ? ">" : " ",
                           entry_scale, fg, w - 8);
    if (keyboard_row) {
      plumos_fbdev_draw_text_token_row(
          r, items[i], keyboard_selected ? wifi_keyboard_col : -1,
          name_x, y, entry_scale, w - 8, fg);
    } else if (settings_page) {
      char setting_label[160];
      char setting_control[80];
      int control_width;
      int control_x;

      if (plumos_fbdev_split_setting_control(items[i], setting_label,
                                             sizeof(setting_label),
                                             setting_control,
                                             sizeof(setting_control))) {
        control_width = plumos_fbdev_text_width(setting_control, entry_scale);
        control_x = right_x - control_width;
        if (control_x < name_x + 6 * cell_width) {
          control_x = name_x + 6 * cell_width;
        }
        plumos_fbdev_draw_text(r, name_x, y, setting_label, entry_scale, fg,
                               control_x - cell_width);
        plumos_fbdev_draw_text(r, control_x, y, setting_control, entry_scale, fg,
                               right_x);
      } else {
        plumos_fbdev_draw_text(r, name_x, y, items[i], entry_scale, fg, w - 8);
      }
    } else {
      int scroll_px = 0;
      int prefer_freetype = wifi_connect_page || rom_list_page;
      if (rom_list_page && selected[i] && right_x > name_x) {
        int name_width = plumos_fbdev_text_width_font(
            r, items[i], entry_scale, prefer_freetype);
        int available_width = right_x - name_x;
        if (name_width > available_width) {
          scroll_px = plumos_fbdev_marquee_offset(
              r, name_width, available_width, cell_width / 2);
        }
      }
      plumos_fbdev_draw_text_clipped(
          r, name_x - scroll_px, y, items[i], entry_scale, prefer_freetype,
          name_x, right_x, fg);
    }
    y += line_height;
  }
  if ((footer1 && footer1[0]) || (footer2 && footer2[0]) ||
      (wifi_password && wifi_password[0])) {
    plumos_fbdev_fill_rect(r, 0, h - 74, w, 74, p->panel_inner);
    plumos_fbdev_fill_rect(r, 0, h - 76, w, 2, p->panel);
    if (wifi_password && wifi_password[0]) {
      const char *label = "Password:";
      int label_width = plumos_fbdev_text_width_font(r, label, 2, 1);
      plumos_fbdev_draw_text_font(r, 14, h - 56, label, 2, 1,
                                  p->muted, w - 8);
      plumos_fbdev_draw_text_font(r, 14 + label_width + 12, h - 56,
                                  wifi_password, 2, 1,
                                  p->selection_foreground, w - 8);
    } else if (footer1 && footer1[0]) {
      plumos_fbdev_draw_text_font(r, 14, h - 56, footer1, 2,
                                  wifi_connect_page, p->muted, w - 8);
    }
    if (footer2 && footer2[0]) {
      plumos_fbdev_draw_text_font(r, 14, h - 34, footer2, 2,
                                  wifi_connect_page, p->muted, w - 8);
    }
  }
  return 1;
}

static int plumos_fbdev_renderer_init(struct plumos_fbdev_renderer *r,
                                      const char *fb_path, char *error,
                                      size_t error_size) {
  long map_size;
  long visible_offset;
  long draw_offset;
  const char *double_buffer_env;
  const char *path = fb_path && fb_path[0] ? fb_path : "/dev/fb0";

  if (!r) {
    return 0;
  }
  memset(r, 0, sizeof(*r));
  r->fd = -1;
#ifdef PLUMOS_FBDEV_ENABLE_DRM
  r->drm_fd = -1;
  {
    const char *drm_path = getenv("PLUMOS_DRM_DEVICE");
    if (drm_path && drm_path[0]) {
      return plumos_fbdev_drm_init(r, drm_path, error, error_size);
    }
  }
#endif
  r->fd = open(path, O_RDWR);
  if (r->fd < 0) {
    snprintf(error, error_size, "open %.180s: %.60s", path, strerror(errno));
    return 0;
  }
  if (ioctl(r->fd, FBIOGET_VSCREENINFO, &r->var) != 0 ||
      ioctl(r->fd, FBIOGET_FSCREENINFO, &r->fix) != 0) {
    snprintf(error, error_size, "framebuffer ioctl: %s", strerror(errno));
    close(r->fd);
    r->fd = -1;
    return 0;
  }
  r->bytes_per_pixel = (int)((r->var.bits_per_pixel + 7U) / 8U);
  if (!(r->bytes_per_pixel == 2 || r->bytes_per_pixel == 3 ||
        r->bytes_per_pixel == 4) ||
      r->fix.line_length == 0 || r->var.xres == 0 || r->var.yres == 0) {
    snprintf(error, error_size, "unsupported fb bpp=%u line=%u xres=%u yres=%u",
             r->var.bits_per_pixel, r->fix.line_length, r->var.xres, r->var.yres);
    close(r->fd);
    r->fd = -1;
    return 0;
  }
  r->physical_xres = r->var.xres;
  r->physical_yres = r->var.yres;
  map_size = r->fix.smem_len ? (long)r->fix.smem_len
                             : (long)r->fix.line_length * (long)r->var.yres_virtual;
  r->frame_bytes = (long)r->fix.line_length * (long)r->var.yres;
  r->visible_yoffset = r->var.yoffset;
  visible_offset = (long)r->visible_yoffset * (long)r->fix.line_length +
                   (long)r->var.xoffset * (long)r->bytes_per_pixel;
  r->visible_offset = visible_offset;
  r->active_offset = visible_offset;
  r->draw_yoffset = r->visible_yoffset;
  if (map_size <= 0 || r->visible_offset < 0 ||
      r->visible_offset + r->frame_bytes > map_size) {
    snprintf(error, error_size, "invalid fb active page");
    close(r->fd);
    r->fd = -1;
    return 0;
  }
  r->map_size = (size_t)map_size;
  double_buffer_env = getenv("PLUMOS_FBDEV_DOUBLE_BUFFER");
  if ((!double_buffer_env || strcmp(double_buffer_env, "0") != 0) &&
      r->var.yres_virtual >= r->var.yres * 2U &&
      r->var.yoffset + r->var.yres <= r->var.yres_virtual) {
    r->draw_yoffset = r->var.yoffset < r->var.yres ? r->var.yres : 0;
    draw_offset = plumos_fbdev_yoffset_to_offset(r, r->draw_yoffset);
    if (plumos_fbdev_frame_offset_valid(r, draw_offset) &&
        r->draw_yoffset + r->var.yres <= r->var.yres_virtual) {
      r->active_offset = draw_offset;
      r->double_buffer = 1;
    } else {
      r->draw_yoffset = r->visible_yoffset;
    }
  }
  r->mem = (unsigned char *)mmap(
      NULL, r->map_size, PROT_READ | PROT_WRITE, MAP_SHARED, r->fd, 0);
  if (r->mem == MAP_FAILED) {
    r->mem = NULL;
    snprintf(error, error_size, "mmap framebuffer: %s", strerror(errno));
    close(r->fd);
    r->fd = -1;
    return 0;
  }
  if (!r->double_buffer) {
    r->shadow = (unsigned char *)malloc((size_t)r->frame_bytes);
    if (!r->shadow) {
      snprintf(error, error_size, "allocate framebuffer shadow: %s",
               strerror(errno));
      munmap(r->mem, r->map_size);
      r->mem = NULL;
      close(r->fd);
      r->fd = -1;
      return 0;
    }
    memcpy(r->shadow, r->mem + r->visible_offset, (size_t)r->frame_bytes);
  }
  return 1;
}

static void plumos_fbdev_renderer_set_rotation(struct plumos_fbdev_renderer *r,
                                               const char *rotation) {
  if (!r) {
    return;
  }
  r->rotation = 0;
  if (!rotation) {
    return;
  }
  if (strcmp(rotation, "cw") == 0 || strcmp(rotation, "90") == 0) {
    r->rotation = 1;
  } else if (strcmp(rotation, "180") == 0 ||
             strcmp(rotation, "rotate180") == 0 ||
             strcmp(rotation, "inverted") == 0) {
    r->rotation = 2;
  } else if (strcmp(rotation, "ccw") == 0 || strcmp(rotation, "270") == 0) {
    r->rotation = 3;
  }
}

static int plumos_fbdev_render_lines(struct plumos_fbdev_renderer *r,
                                     char lines[][PLUMOS_FBDEV_RENDER_LINE_MAX],
                                     size_t line_count) {
  struct plumos_fbdev_palette palette;
  const char *mode;
  int ok;

  if (!r || !r->mem) {
    return 0;
  }
  if (r->rotation == 1 || r->rotation == 3) {
    r->var.xres = r->physical_yres;
    r->var.yres = r->physical_xres;
  }
  plumos_fbdev_load_palette(r, &palette, lines, line_count);
  mode = plumos_fbdev_find_value(lines, line_count, "graphic_mode=");
  if (plumos_fbdev_has_prefixed_line(lines, line_count,
                                     "power_action_running=1")) {
    ok = plumos_fbdev_render_power_action_running(r, lines, line_count,
                                                   &palette);
  } else if (plumos_fbdev_has_prefixed_line(lines, line_count,
                                            "top_refresh_running=1")) {
    ok = plumos_fbdev_render_top_refresh_running(r, &palette);
  } else if (mode && strcmp(mode, "top") == 0) {
    ok = plumos_fbdev_render_top(r, lines, line_count, &palette);
  } else if (mode && strcmp(mode, "gallery") == 0) {
    ok = plumos_fbdev_render_gallery(r, lines, line_count, &palette);
  } else if (mode && (strcmp(mode, "roms") == 0 ||
                      strcmp(mode, "favorites") == 0 ||
                      strcmp(mode, "recent") == 0)) {
    ok = plumos_fbdev_render_roms(r, lines, line_count, &palette);
  } else {
    ok = plumos_fbdev_render_generic(r, lines, line_count, &palette);
  }
  r->var.xres = r->physical_xres;
  r->var.yres = r->physical_yres;
  if (!r->shadow
#ifdef PLUMOS_FBDEV_ENABLE_DRM
      && !r->drm_active
#endif
  ) {
    msync(r->mem, r->map_size, MS_ASYNC);
  }
  if (ok) {
    ok = plumos_fbdev_present(r);
  }
  return ok;
}

static void plumos_fbdev_renderer_reset_marquee(struct plumos_fbdev_renderer *r) {
  if (!r) {
    return;
  }
  r->marquee_focus_ms = plumos_fbdev_time_ms();
}

static void plumos_fbdev_renderer_shutdown(struct plumos_fbdev_renderer *r) {
  if (!r) {
    return;
  }
#ifdef PLUMOS_FBDEV_ENABLE_FREETYPE
  plumos_fbdev_ft_glyph_cache_clear(r);
  if (r->ft_fallback_face) {
    FT_Done_Face(r->ft_fallback_face);
    r->ft_fallback_face = NULL;
    r->ft_fallback_ready = 0;
  }
  if (r->ft_face) {
    FT_Done_Face(r->ft_face);
    r->ft_face = NULL;
    r->ft_ready = 0;
  }
  if (r->ft_library) {
    FT_Done_FreeType(r->ft_library);
    r->ft_library = NULL;
  }
#endif
#ifdef PLUMOS_FBDEV_ENABLE_PNG
  plumos_fbdev_png_cache_clear(r);
#endif
#ifdef PLUMOS_FBDEV_ENABLE_DRM
  if (r->drm_active || r->drm_fd >= 0) {
    if (r->drm_saved_crtc && r->drm_saved_crtc->buffer_id) {
      (void)drmModeSetCrtc(
          r->drm_fd, r->drm_saved_crtc->crtc_id,
          r->drm_saved_crtc->buffer_id, r->drm_saved_crtc->x,
          r->drm_saved_crtc->y, &r->drm_connector_id, 1,
          &r->drm_saved_crtc->mode);
    }
    plumos_fbdev_drm_destroy_buffer(r, 1);
    plumos_fbdev_drm_destroy_buffer(r, 0);
    if (r->drm_saved_crtc) {
      drmModeFreeCrtc(r->drm_saved_crtc);
      r->drm_saved_crtc = NULL;
    }
    if (r->drm_fd >= 0) {
      close(r->drm_fd);
      r->drm_fd = -1;
    }
    r->drm_active = 0;
    r->mem = NULL;
    r->map_size = 0;
  }
#endif
  if (r->mem) {
    munmap(r->mem, r->map_size);
    r->mem = NULL;
  }
  free(r->shadow);
  r->shadow = NULL;
  if (r->fd >= 0) {
    close(r->fd);
    r->fd = -1;
  }
}

#endif

#endif
