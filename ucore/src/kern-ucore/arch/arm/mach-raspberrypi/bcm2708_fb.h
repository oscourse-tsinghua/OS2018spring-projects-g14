#ifndef BCM2708_FB_H
#define BCM2708_FB_H

#include <fb.h>

extern void dev_init_fb(void);

extern bool fb_check(void);
extern struct fb_info *get_fb_info(void);
extern void fb_putc(unsigned char ch);

struct fb_info {
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	struct device *dev;
	char *screen_base; /* Virtual address */
	unsigned long screen_size; /* Amount of ioremapped VRAM or 0 */
	unsigned long fb_bus_address; /* Physical address */
};

#endif /* BCM2708_FB_H */
