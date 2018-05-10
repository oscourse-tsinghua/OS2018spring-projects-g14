#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <types.h>

extern void fb_init(void);
extern void fb_write(unsigned char ch);

struct fb_info {
	uint32_t width, height;
	uint32_t width_virtual, height_virtual;
	uint32_t xoffset, yoffset;
	uint32_t bits_per_pixel; /* bits per pixel */
	uint32_t pitch; /* bytes per line */
	uint32_t bus_addr; /* Physical address */
	uint32_t screen_base; /* Virtual address */
	uint32_t screen_size; /* Amount of ioremapped VRAM or 0 */
};

extern struct fb_info *raspberrypi_fb;
extern bool raspberrypi_fb_exist;

#endif /* FRAMEBUFFER_H */
