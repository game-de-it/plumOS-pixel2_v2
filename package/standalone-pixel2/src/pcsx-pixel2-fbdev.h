#ifndef PLUMOS_PCSX_PIXEL2_FBDEV_H
#define PLUMOS_PCSX_PIXEL2_FBDEV_H

#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <SDL.h>

#define PIXEL2_PCSX_MAX_SOURCE_PIXELS (1024U * 1024U)

struct pixel2_pcsx_frame {
	uint16_t *pixels;
	unsigned width;
	unsigned height;
	unsigned pitch;
};

static int pixel2_pcsx_fb_fd = -1;
static uint8_t *pixel2_pcsx_fb_map;
static size_t pixel2_pcsx_fb_size;
static struct fb_fix_screeninfo pixel2_pcsx_fb_fix;
static struct fb_var_screeninfo pixel2_pcsx_fb_var;
static uint32_t pixel2_pcsx_fb_draw_yoffset;
static int pixel2_pcsx_fb_double_buffer;
static pthread_t pixel2_pcsx_fb_thread;
static pthread_mutex_t pixel2_pcsx_fb_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pixel2_pcsx_fb_cond = PTHREAD_COND_INITIALIZER;
static struct pixel2_pcsx_frame pixel2_pcsx_fb_frames[2];
static int pixel2_pcsx_fb_thread_started;
static int pixel2_pcsx_fb_thread_stop;
static int pixel2_pcsx_fb_active = -1;
static int pixel2_pcsx_fb_pending = -1;
static uint32_t pixel2_pcsx_fb_lut[65536];
static uint32_t pixel2_pcsx_sdl_rmask;
static uint32_t pixel2_pcsx_sdl_gmask;
static uint32_t pixel2_pcsx_sdl_bmask;
static int pixel2_pcsx_fb_lut_ready;
static int pixel2_pcsx_fb_first_frame = 1;

static uint32_t pixel2_pcsx_scale_channel(uint8_t value, uint32_t length,
	uint32_t offset)
{
	uint32_t max_value;

	if (length == 0)
		return 0;
	if (length >= 8)
		return (uint32_t)value << offset;
	max_value = (1U << length) - 1U;
	return (((uint32_t)value * max_value + 127U) / 255U) << offset;
}

static uint32_t pixel2_pcsx_pack_color(uint8_t red, uint8_t green, uint8_t blue)
{
	uint32_t color = 0;

	color |= pixel2_pcsx_scale_channel(red, pixel2_pcsx_fb_var.red.length,
		pixel2_pcsx_fb_var.red.offset);
	color |= pixel2_pcsx_scale_channel(green, pixel2_pcsx_fb_var.green.length,
		pixel2_pcsx_fb_var.green.offset);
	color |= pixel2_pcsx_scale_channel(blue, pixel2_pcsx_fb_var.blue.length,
		pixel2_pcsx_fb_var.blue.offset);
	if (pixel2_pcsx_fb_var.transp.length)
		color |= pixel2_pcsx_scale_channel(255,
			pixel2_pcsx_fb_var.transp.length,
			pixel2_pcsx_fb_var.transp.offset);
	return color;
}

static void pixel2_pcsx_fb_update_lut(const SDL_PixelFormat *format)
{
	unsigned value;

	if (!format || format->BitsPerPixel != 16)
		return;
	if (pixel2_pcsx_fb_lut_ready &&
	    pixel2_pcsx_sdl_rmask == format->Rmask &&
	    pixel2_pcsx_sdl_gmask == format->Gmask &&
	    pixel2_pcsx_sdl_bmask == format->Bmask)
		return;
	for (value = 0; value < 65536U; value++) {
		uint8_t red;
		uint8_t green;
		uint8_t blue;

		SDL_GetRGB(value, format, &red, &green, &blue);
		pixel2_pcsx_fb_lut[value] = pixel2_pcsx_pack_color(red, green, blue);
	}
	pixel2_pcsx_sdl_rmask = format->Rmask;
	pixel2_pcsx_sdl_gmask = format->Gmask;
	pixel2_pcsx_sdl_bmask = format->Bmask;
	pixel2_pcsx_fb_lut_ready = 1;
}

static void pixel2_pcsx_fb_present_pixels(const struct pixel2_pcsx_frame *frame)
{
	unsigned dst_y;
	unsigned dst_width = pixel2_pcsx_fb_var.xres;
	unsigned dst_height = pixel2_pcsx_fb_var.yres;
	uint8_t *draw_base;

	if (!pixel2_pcsx_fb_map || !frame || !frame->pixels ||
	    !frame->width || !frame->height || !pixel2_pcsx_fb_lut_ready)
		return;
	draw_base = pixel2_pcsx_fb_map +
		(size_t)pixel2_pcsx_fb_draw_yoffset * pixel2_pcsx_fb_fix.line_length;

	/*
	 * Pixel2 exposes its landscape 640x480 LCD as a 480x640 framebuffer.
	 * This is the same counter-clockwise logical rotation used by the plumOS
	 * frontend and RetroArch.  Scaling in logical coordinates also gives PSX
	 * video its intended 4:3 display aspect instead of its raw pixel aspect.
	 */
	for (dst_y = 0; dst_y < dst_height; dst_y++) {
		unsigned logical_x = dst_height - 1U - dst_y;
		unsigned src_x = logical_x * frame->width / dst_height;
		uint32_t *dst = (uint32_t *)(draw_base +
			(size_t)dst_y * pixel2_pcsx_fb_fix.line_length +
			(size_t)pixel2_pcsx_fb_var.xoffset * sizeof(*dst));
		unsigned dst_x;

		for (dst_x = 0; dst_x < dst_width; dst_x++) {
			unsigned src_y = dst_x * frame->height / dst_width;
			uint16_t pixel = frame->pixels[(size_t)src_y * frame->pitch + src_x];
			dst[dst_x] = pixel2_pcsx_fb_lut[pixel];
		}
	}

	if (pixel2_pcsx_fb_double_buffer) {
		struct fb_var_screeninfo next = pixel2_pcsx_fb_var;

		next.xoffset = 0;
		next.yoffset = pixel2_pcsx_fb_draw_yoffset;
#ifdef FB_ACTIVATE_VBL
		next.activate = FB_ACTIVATE_VBL;
#else
		next.activate = FB_ACTIVATE_NOW;
#endif
		if (ioctl(pixel2_pcsx_fb_fd, FBIOPAN_DISPLAY, &next) == 0) {
			pixel2_pcsx_fb_var = next;
			pixel2_pcsx_fb_draw_yoffset = next.yoffset == 0 ? next.yres : 0;
		} else {
			fprintf(stderr,
				"Pixel2 PCSX framebuffer pan failed: %s; using front page\n",
				strerror(errno));
			pixel2_pcsx_fb_double_buffer = 0;
			pixel2_pcsx_fb_draw_yoffset = pixel2_pcsx_fb_var.yoffset;
		}
	}
}

static void *pixel2_pcsx_fb_present_thread(void *unused)
{
	(void)unused;
	for (;;) {
		int frame;

		pthread_mutex_lock(&pixel2_pcsx_fb_mutex);
		while (pixel2_pcsx_fb_pending < 0 && !pixel2_pcsx_fb_thread_stop)
			pthread_cond_wait(&pixel2_pcsx_fb_cond, &pixel2_pcsx_fb_mutex);
		if (pixel2_pcsx_fb_thread_stop) {
			pthread_mutex_unlock(&pixel2_pcsx_fb_mutex);
			break;
		}
		frame = pixel2_pcsx_fb_pending;
		pixel2_pcsx_fb_pending = -1;
		pixel2_pcsx_fb_active = frame;
		pthread_mutex_unlock(&pixel2_pcsx_fb_mutex);

		pixel2_pcsx_fb_present_pixels(&pixel2_pcsx_fb_frames[frame]);

		pthread_mutex_lock(&pixel2_pcsx_fb_mutex);
		pixel2_pcsx_fb_active = -1;
		pthread_mutex_unlock(&pixel2_pcsx_fb_mutex);
	}
	return NULL;
}

static int pixel2_pcsx_fb_init(void)
{
	size_t frame_bytes;
	int index;

	pixel2_pcsx_fb_fd = open("/dev/fb0", O_RDWR);
	if (pixel2_pcsx_fb_fd < 0 ||
	    ioctl(pixel2_pcsx_fb_fd, FBIOGET_FSCREENINFO, &pixel2_pcsx_fb_fix) < 0 ||
	    ioctl(pixel2_pcsx_fb_fd, FBIOGET_VSCREENINFO, &pixel2_pcsx_fb_var) < 0) {
		fprintf(stderr, "Pixel2 PCSX framebuffer query failed: %s\n",
			strerror(errno));
		return -1;
	}
	if (pixel2_pcsx_fb_var.xres != 480 || pixel2_pcsx_fb_var.yres != 640 ||
	    pixel2_pcsx_fb_var.bits_per_pixel != 32 ||
	    pixel2_pcsx_fb_fix.line_length < 480U * 4U) {
		fprintf(stderr,
			"Pixel2 PCSX framebuffer contract mismatch: %ux%u bpp=%u stride=%u\n",
			pixel2_pcsx_fb_var.xres, pixel2_pcsx_fb_var.yres,
			pixel2_pcsx_fb_var.bits_per_pixel,
			pixel2_pcsx_fb_fix.line_length);
		return -1;
	}
	pixel2_pcsx_fb_size = pixel2_pcsx_fb_fix.smem_len
		? pixel2_pcsx_fb_fix.smem_len
		: (size_t)pixel2_pcsx_fb_fix.line_length *
		  pixel2_pcsx_fb_var.yres_virtual;
	if (pixel2_pcsx_fb_size < (size_t)pixel2_pcsx_fb_fix.line_length *
	    (pixel2_pcsx_fb_var.yoffset + pixel2_pcsx_fb_var.yres))
		return -1;
	pixel2_pcsx_fb_map = mmap(NULL, pixel2_pcsx_fb_size,
		PROT_READ | PROT_WRITE, MAP_SHARED, pixel2_pcsx_fb_fd, 0);
	if (pixel2_pcsx_fb_map == MAP_FAILED) {
		pixel2_pcsx_fb_map = NULL;
		fprintf(stderr, "Pixel2 PCSX framebuffer mmap failed: %s\n",
			strerror(errno));
		return -1;
	}
	pixel2_pcsx_fb_double_buffer =
		pixel2_pcsx_fb_var.yres_virtual >= pixel2_pcsx_fb_var.yres * 2U;
	pixel2_pcsx_fb_draw_yoffset = pixel2_pcsx_fb_double_buffer &&
		pixel2_pcsx_fb_var.yoffset < pixel2_pcsx_fb_var.yres
		? pixel2_pcsx_fb_var.yres : pixel2_pcsx_fb_var.yoffset;
	frame_bytes = PIXEL2_PCSX_MAX_SOURCE_PIXELS * sizeof(uint16_t);
	for (index = 0; index < 2; index++) {
		pixel2_pcsx_fb_frames[index].pixels = malloc(frame_bytes);
		if (!pixel2_pcsx_fb_frames[index].pixels)
			return -1;
	}
	if (pthread_create(&pixel2_pcsx_fb_thread, NULL,
	    pixel2_pcsx_fb_present_thread, NULL) != 0)
		return -1;
	pixel2_pcsx_fb_thread_started = 1;
	fprintf(stderr,
		"Pixel2 PCSX framebuffer presenter: physical=%ux%u logical=640x480 ccw double_buffer=%d\n",
		pixel2_pcsx_fb_var.xres, pixel2_pcsx_fb_var.yres,
		pixel2_pcsx_fb_double_buffer);
	return 0;
}

static void pixel2_pcsx_fb_present(const SDL_Surface *surface)
{
	SDL_Surface *mutable_surface = (SDL_Surface *)surface;
	struct pixel2_pcsx_frame *dst;
	unsigned width;
	unsigned height;
	unsigned y;
	int frame;
	int locked = 0;

	if (!surface || !surface->pixels || !surface->format ||
	    surface->format->BitsPerPixel != 16 || !pixel2_pcsx_fb_thread_started)
		return;
	width = (unsigned)surface->w;
	height = (unsigned)surface->h;
	if (!width || !height || (size_t)width * height > PIXEL2_PCSX_MAX_SOURCE_PIXELS)
		return;
	pixel2_pcsx_fb_update_lut(surface->format);
	if (SDL_MUSTLOCK(mutable_surface)) {
		if (SDL_LockSurface(mutable_surface) != 0)
			return;
		locked = 1;
	}
	pthread_mutex_lock(&pixel2_pcsx_fb_mutex);
	frame = pixel2_pcsx_fb_active == 0 ? 1 : 0;
	if (pixel2_pcsx_fb_active < 0 && pixel2_pcsx_fb_pending >= 0)
		frame = pixel2_pcsx_fb_pending;
	dst = &pixel2_pcsx_fb_frames[frame];
	for (y = 0; y < height; y++)
		memcpy(dst->pixels + (size_t)y * width,
			(const uint8_t *)surface->pixels + (size_t)y * surface->pitch,
			width * sizeof(uint16_t));
	dst->width = width;
	dst->height = height;
	dst->pitch = width;
	pixel2_pcsx_fb_pending = frame;
	pthread_cond_signal(&pixel2_pcsx_fb_cond);
	pthread_mutex_unlock(&pixel2_pcsx_fb_mutex);
	if (locked)
		SDL_UnlockSurface(mutable_surface);
	if (pixel2_pcsx_fb_first_frame) {
		fprintf(stderr,
			"Pixel2 PCSX first frame: source=%ux%u pitch=%u masks=%08x/%08x/%08x\n",
			width, height, surface->pitch,
			surface->format->Rmask, surface->format->Gmask,
			surface->format->Bmask);
		pixel2_pcsx_fb_first_frame = 0;
	}
}

static void pixel2_pcsx_fb_finish(void)
{
	int index;

	if (pixel2_pcsx_fb_thread_started) {
		pthread_mutex_lock(&pixel2_pcsx_fb_mutex);
		pixel2_pcsx_fb_thread_stop = 1;
		pthread_cond_signal(&pixel2_pcsx_fb_cond);
		pthread_mutex_unlock(&pixel2_pcsx_fb_mutex);
		pthread_join(pixel2_pcsx_fb_thread, NULL);
		pixel2_pcsx_fb_thread_started = 0;
	}
	if (pixel2_pcsx_fb_map)
		munmap(pixel2_pcsx_fb_map, pixel2_pcsx_fb_size);
	if (pixel2_pcsx_fb_fd >= 0)
		close(pixel2_pcsx_fb_fd);
	for (index = 0; index < 2; index++) {
		free(pixel2_pcsx_fb_frames[index].pixels);
		pixel2_pcsx_fb_frames[index].pixels = NULL;
	}
	pixel2_pcsx_fb_map = NULL;
	pixel2_pcsx_fb_fd = -1;
}

#endif
