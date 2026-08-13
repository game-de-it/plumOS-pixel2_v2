#ifndef PLUMOS_PICOARCH_PIXEL2_FBDEV_H
#define PLUMOS_PICOARCH_PIXEL2_FBDEV_H

#include <fcntl.h>
#include <errno.h>
#include <linux/fb.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int pixel2_fb_fd = -1;
static uint8_t *pixel2_fb_map;
static size_t pixel2_fb_size;
static struct fb_fix_screeninfo pixel2_fb_fix;
static struct fb_var_screeninfo pixel2_fb_var;
static uint32_t pixel2_fb_draw_yoffset;
static int pixel2_fb_double_buffer;
static pthread_t pixel2_fb_thread;
static pthread_mutex_t pixel2_fb_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pixel2_fb_cond = PTHREAD_COND_INITIALIZER;
static uint16_t *pixel2_fb_frames[2];
static int pixel2_fb_thread_started;
static int pixel2_fb_thread_stop;
static int pixel2_fb_active = -1;
static int pixel2_fb_pending = -1;

static void pixel2_fb_present_pixels(const uint16_t *pixels)
{
	unsigned dst_y;
	unsigned dst_width = pixel2_fb_var.xres;
	unsigned dst_height = pixel2_fb_var.yres;
	uint8_t *draw_base;

	if (!pixel2_fb_map || !pixels)
		return;
	draw_base = pixel2_fb_map +
		(size_t)pixel2_fb_draw_yoffset * pixel2_fb_fix.line_length;
	/* Pixel2 exposes the landscape LCD as a physical 480x640 framebuffer.
	 * Present the logical 640x480 PicoArch surface counter-clockwise, matching
	 * the frontend, RetroArch, and PCSX-ReARMed contract. */
	for (dst_y = 0; dst_y < dst_height; dst_y++) {
		unsigned logical_x = dst_height - 1U - dst_y;
		uint32_t *dst = (uint32_t *)(draw_base +
			(size_t)dst_y * pixel2_fb_fix.line_length +
			(size_t)pixel2_fb_var.xoffset * sizeof(*dst));
		unsigned dst_x;

		for (dst_x = 0; dst_x < dst_width; dst_x++) {
			uint16_t p = pixels[(size_t)dst_x * SCREEN_WIDTH + logical_x];
			uint32_t r = (p >> 11) & 0x1f;
			uint32_t g = (p >> 5) & 0x3f;
			uint32_t b = p & 0x1f;

			dst[dst_x] = 0xff000000u | ((r << 3 | r >> 2) << 16) |
			         ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
		}
	}
	if (pixel2_fb_double_buffer) {
		struct fb_var_screeninfo next = pixel2_fb_var;
		next.xoffset = 0;
		next.yoffset = pixel2_fb_draw_yoffset;
		/* The emulation/audio clock runs at the core's native rate. Waiting for
		 * the LCD here would drop one a 60 Hz frame roughly once
		 * per second, so the presenter must remain nonblocking. */
		next.activate = FB_ACTIVATE_NOW;
		if (ioctl(pixel2_fb_fd, FBIOPAN_DISPLAY, &next) == 0) {
			pixel2_fb_var = next;
			pixel2_fb_draw_yoffset = next.yoffset == 0 ? next.yres : 0;
		} else {
			pixel2_fb_double_buffer = 0;
			pixel2_fb_draw_yoffset = pixel2_fb_var.yoffset;
		}
	}
}

static void *pixel2_fb_present_thread(void *unused)
{
	(void)unused;
	for (;;) {
		int frame;

		pthread_mutex_lock(&pixel2_fb_mutex);
		while (pixel2_fb_pending < 0 && !pixel2_fb_thread_stop)
			pthread_cond_wait(&pixel2_fb_cond, &pixel2_fb_mutex);
		if (pixel2_fb_thread_stop) {
			pthread_mutex_unlock(&pixel2_fb_mutex);
			break;
		}
		frame = pixel2_fb_pending;
		pixel2_fb_pending = -1;
		pixel2_fb_active = frame;
		pthread_mutex_unlock(&pixel2_fb_mutex);

		pixel2_fb_present_pixels(pixel2_fb_frames[frame]);

		pthread_mutex_lock(&pixel2_fb_mutex);
		pixel2_fb_active = -1;
		pthread_mutex_unlock(&pixel2_fb_mutex);
	}
	return NULL;
}

static int pixel2_fb_init(void)
{
	pixel2_fb_fd = open("/dev/fb0", O_RDWR);
	if (pixel2_fb_fd < 0 ||
	    ioctl(pixel2_fb_fd, FBIOGET_FSCREENINFO, &pixel2_fb_fix) < 0 ||
	    ioctl(pixel2_fb_fd, FBIOGET_VSCREENINFO, &pixel2_fb_var) < 0) {
		fprintf(stderr, "Pixel2 PicoArch framebuffer query failed: %s\n",
			strerror(errno));
		return -1;
	}
	if (pixel2_fb_var.xres != SCREEN_HEIGHT ||
	    pixel2_fb_var.yres != SCREEN_WIDTH ||
	    pixel2_fb_var.bits_per_pixel != 32 ||
	    pixel2_fb_fix.line_length <
		(pixel2_fb_var.xoffset + pixel2_fb_var.xres) * 4) {
		fprintf(stderr,
			"Pixel2 PicoArch framebuffer contract mismatch: %ux%u bpp=%u stride=%u\n",
			pixel2_fb_var.xres, pixel2_fb_var.yres,
			pixel2_fb_var.bits_per_pixel, pixel2_fb_fix.line_length);
		return -1;
	}
	pixel2_fb_size = pixel2_fb_fix.smem_len
	                 ? pixel2_fb_fix.smem_len
	                 : (size_t)pixel2_fb_fix.line_length * pixel2_fb_var.yres_virtual;
	if (pixel2_fb_size < (size_t)pixel2_fb_fix.line_length *
	                    (pixel2_fb_var.yoffset + pixel2_fb_var.yres)) {
		return -1;
	}
	pixel2_fb_map = mmap(NULL, pixel2_fb_size, PROT_READ | PROT_WRITE,
	                   MAP_SHARED, pixel2_fb_fd, 0);
	if (pixel2_fb_map == MAP_FAILED) {
		pixel2_fb_map = NULL;
		fprintf(stderr, "Pixel2 PicoArch framebuffer mmap failed: %s\n",
			strerror(errno));
		return -1;
	}
	pixel2_fb_double_buffer = pixel2_fb_var.yres_virtual >= pixel2_fb_var.yres * 2;
	pixel2_fb_draw_yoffset = pixel2_fb_double_buffer &&
	                       pixel2_fb_var.yoffset < pixel2_fb_var.yres
	                         ? pixel2_fb_var.yres : 0;
	pixel2_fb_frames[0] = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t));
	pixel2_fb_frames[1] = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t));
	if (!pixel2_fb_frames[0] || !pixel2_fb_frames[1])
		return -1;
	pixel2_fb_thread_stop = 0;
	pixel2_fb_active = -1;
	pixel2_fb_pending = -1;
	if (pthread_create(&pixel2_fb_thread, NULL,
	                   pixel2_fb_present_thread, NULL) != 0)
		return -1;
	pixel2_fb_thread_started = 1;
	fprintf(stderr,
		"Pixel2 PicoArch framebuffer presenter: physical=%ux%u logical=%ux%u ccw double_buffer=%d\n",
		pixel2_fb_var.xres, pixel2_fb_var.yres,
		SCREEN_WIDTH, SCREEN_HEIGHT, pixel2_fb_double_buffer);
	return 0;
}

static void pixel2_fb_present(const SDL_Surface *surface)
{
	unsigned y;
	int frame;

	if (!pixel2_fb_map || !surface || !pixel2_fb_thread_started)
		return;
	pthread_mutex_lock(&pixel2_fb_mutex);
	frame = pixel2_fb_active == 0 ? 1 : 0;
	if (pixel2_fb_active < 0 && pixel2_fb_pending >= 0)
		frame = pixel2_fb_pending;
	for (y = 0; y < SCREEN_HEIGHT; y++) {
		const uint16_t *src = (const uint16_t *)
			((const uint8_t *)surface->pixels + y * surface->pitch);
		memcpy(pixel2_fb_frames[frame] + y * SCREEN_WIDTH, src,
		       SCREEN_WIDTH * sizeof(uint16_t));
	}
	pixel2_fb_pending = frame;
	pthread_cond_signal(&pixel2_fb_cond);
	pthread_mutex_unlock(&pixel2_fb_mutex);
}

static void pixel2_fb_finish(void)
{
	if (pixel2_fb_thread_started) {
		pthread_mutex_lock(&pixel2_fb_mutex);
		pixel2_fb_thread_stop = 1;
		pthread_cond_signal(&pixel2_fb_cond);
		pthread_mutex_unlock(&pixel2_fb_mutex);
		pthread_join(pixel2_fb_thread, NULL);
		pixel2_fb_thread_started = 0;
	}
	if (pixel2_fb_map && pixel2_fb_map != MAP_FAILED) {
		munmap(pixel2_fb_map, pixel2_fb_size);
	}
	if (pixel2_fb_fd >= 0) {
		close(pixel2_fb_fd);
	}
	pixel2_fb_map = NULL;
	pixel2_fb_fd = -1;
	free(pixel2_fb_frames[0]);
	free(pixel2_fb_frames[1]);
	pixel2_fb_frames[0] = NULL;
	pixel2_fb_frames[1] = NULL;
}

#endif
