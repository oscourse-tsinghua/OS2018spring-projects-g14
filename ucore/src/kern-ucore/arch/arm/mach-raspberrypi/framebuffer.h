#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

extern void fb_init(void);
extern void fb_write(unsigned char ch);
extern void fb_init_mmu(void);

struct fb_info {
	unsigned int paddr; /* Physical address */
	unsigned int screen_base; /* Virtual address */
	unsigned long screen_size; /* Amount of ioremapped VRAM or 0 */
};

extern struct fb_info *raspberrypi_fb;

#endif /* FRAMEBUFFER_H */
