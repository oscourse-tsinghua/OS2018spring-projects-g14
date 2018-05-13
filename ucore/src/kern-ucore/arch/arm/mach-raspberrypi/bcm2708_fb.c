#include <arm.h>
#include <dev.h>
#include <vfs.h>
#include <inode.h>
#include <error.h>

#include "bcm2708_fb.h"
#include "mailbox_property.h"
#include "barrier.h"
#include "teletext.h"

/* Screen parameters set in fb_init() */
/* Max x/y character cell */
static unsigned int max_x, max_y;

struct fb_info *bcm2708_fb = NULL;

bool fb_check(void)
{
	return bcm2708_fb != NULL;
}

struct fb_info *get_fb_info(void)
{
	return bcm2708_fb;
}

static int bcm2708_fb_probe(struct device *dev)
{
	int ret;
	struct fb_info *fb;

	fb = (struct fb_info *)kmalloc(sizeof(struct fb_info));
	if (fb == NULL) {
		ret = -E_NO_MEM;
		goto free_region;
	}
	memset(fb, 0, sizeof(struct fb_info));

	uint32_t width, height, depth;

	ret = mbox_framebuffer_get_physical_size(&width, &height);
	if (ret != 0) {
		kprintf("BCM2708FB: cannot get physical size!\n");
		goto free_fb;
	}
	ret = mbox_framebuffer_get_depth(&depth);
	if (ret != 0) {
		kprintf("BCM2708FB: cannot get color depth!\n");
		goto free_fb;
	}

	struct fb_alloc_tags fbinfo = {
		.tag1 = { RPI_FIRMWARE_FRAMEBUFFER_SET_PHYSICAL_WIDTH_HEIGHT,
			  8, 0, },
			.xres = width,
			.yres = height,
		.tag2 = { RPI_FIRMWARE_FRAMEBUFFER_SET_VIRTUAL_WIDTH_HEIGHT,
			  8, 0, },
			.xres_virtual = width,
			.yres_virtual = height,
		.tag3 = { RPI_FIRMWARE_FRAMEBUFFER_SET_DEPTH, 4, 0 },
			.bpp = depth,
		.tag4 = { RPI_FIRMWARE_FRAMEBUFFER_SET_VIRTUAL_OFFSET, 8, 0 },
			.xoffset = 0,
			.yoffset = 0,
		.tag5 = { RPI_FIRMWARE_FRAMEBUFFER_ALLOCATE, 8, 0 },
			.base = 0,
			.screen_size = 0,
		.tag6 = { RPI_FIRMWARE_FRAMEBUFFER_GET_PITCH, 4, 0 },
			.pitch = 0,
	};

	ret = mbox_property_list(&fbinfo, sizeof(fbinfo));
	if (ret != 0) {
		kprintf("BCM2708FB: cannot allocate GPU framebuffer.");
		goto free_fb;
	}

	if (fbinfo.base == 0 || fbinfo.screen_size == 0) {
		ret = -E_NO_MEM;
		kprintf("BCM2708FB: mailbox call returned an invalid address/size.\n");
		goto free_fb;
	}
	if (fbinfo.pitch == 0) {
		ret = -E_NO_MEM;
		kprintf("BCM2708FB: mailbox call to set pitch returned an invalid pitch value.\n");
		goto free_fb;
	}

	fb->var.xres = fbinfo.xres;
	fb->var.yres = fbinfo.yres;
	fb->var.xres_virtual = fbinfo.xres_virtual;
	fb->var.yres_virtual = fbinfo.yres_virtual;
	fb->var.xoffset = fbinfo.xoffset;
	fb->var.yoffset = fbinfo.yoffset;
	fb->var.bits_per_pixel = fbinfo.bpp;
	fb->fix.line_length = fbinfo.pitch;
	fb->fix.smem_start = fbinfo.base;
	fb->fix.smem_len = fbinfo.pitch * fbinfo.yres_virtual;
	fb->fb_bus_address = fbinfo.base;
	fb->screen_size = fbinfo.screen_size;
	fb->screen_base =
		(char *)__ucore_ioremap(fbinfo.base, fb->screen_size, 0);
	fb->dev = dev;
	dev->driver_data = fb;
	bcm2708_fb = fb;

	/* Need to set up max_x/max_y before using fb_write */
	max_x = fbinfo.xres / TELETEXT_W;
	max_y = fbinfo.yres / TELETEXT_H;

	kprintf("BCM2708FB Initialized.\n"
		"base=0x%08x, resolution=%dx%d, bpp=%d, pitch=%d, size=%d\n",
		fbinfo.screen_size, fbinfo.xres, fbinfo.yres, fbinfo.bpp,
		fbinfo.pitch, fbinfo.screen_size);

	goto out;

free_fb:
	kfree(fb);
free_region:
	kprintf("BCM2708FB initialization failed.\n");
out:
	return ret;
}

/* Current console text cursor position (ie. where the next character will
 * be written
*/
static int consx = 0;
static int consy = 0;

/* Current fg/bg colour */
static unsigned fgcolour = 0xffffffff;
static unsigned bgcolour = 0x0;

/* A small stack to allow temporary colour changes in text */
static unsigned int colour_stack[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

static unsigned int colour_sp = 8;

/* Move to a new line, and, if at the bottom of the screen, scroll the
 * framebuffer 1 character row upwards, discarding the top row
 */
static void newline(struct fb_info *fb)
{
	/* Number of bytes in a character row */
	register unsigned int rowbytes = TELETEXT_H * fb->fix.line_length;

	consx = 0;
	if (consy < (max_y - 1)) {
		consy++;
	} else {
		/* Copy a screen's worth of data (minus 1 character row) from the
		 * second row to the first
		 */
		char *source = fb->screen_base + rowbytes;
		memmove(fb->screen_base, source, (max_y - 1) * rowbytes);
		/* Clear last line on screen */
		memset(fb->screen_base + (max_y - 1) * rowbytes, 0, rowbytes);
	}
}

/* Write null-terminated text to the console
 * Supports control characters (see framebuffer.h) for colour and newline
 */
void fb_putc(unsigned char ch) //char *text)
{
	if (fb_check() == 0)
		return;

	struct fb_info *fb = get_fb_info();

	if (ch == 13) {
		//do nothing
	} else if (ch == 10) {
		newline(fb);
	} else if (ch >= 32 && ch <= 127) {
		ch -= 32;

		/* Plot character onto screen
		 *
		 * TELETEXT_H and TELETEXT_W are the size of the block the
		 * character occupies. The character itself is one pixel
		 * smaller in each direction, and is located in the upper left
		 * of the block
		 */
		int row, col;
		for (row = 0; row < TELETEXT_H; row++) {
			uint32_t bytes_per_pixel = fb->var.bits_per_pixel >> 3;
			uint32_t addr = (row + consy * TELETEXT_H) * fb->fix.line_length +
					consx * TELETEXT_W * bytes_per_pixel;
			char *ptr = fb->screen_base + addr;
			for (col = TELETEXT_W - 1; col >= 0; col--) {
				if (teletext[ch][row] & (1 << col))
					memcpy(ptr, &fgcolour, bytes_per_pixel);
				ptr += bytes_per_pixel;
			}
		}

		if (++consx >= max_x) {
			newline(fb);
		}
	}
}

static int fb_open(struct device *dev, uint32_t open_flags)
{
	return 0;
}

static int fb_close(struct device *dev)
{
	return 0;
}

static int fb_io(struct device *dev, struct iobuf *iob, bool write)
{
	return -E_INVAL;
}

static int fb_ioctl(struct device *dev, int op, void *data)
{
	return -E_INVAL;
}

static int fb_device_init(struct device *dev)
{
	memset(dev, 0, sizeof(*dev));
	int ret;
	if ((ret = bcm2708_fb_probe(dev)) != 0) {
		return ret;
	}

	dev->d_blocks = 0;
	dev->d_blocksize = 1;
	dev->d_open = fb_open;
	dev->d_close = fb_close;
	dev->d_io = fb_io;
	dev->d_ioctl = fb_ioctl;

	return ret;
}

void dev_init_fb()
{
	struct inode *node;
	if ((node = dev_create_inode()) == NULL) {
		panic("fb0: dev_create_node.\n");
	}

	int ret;
	if ((ret = fb_device_init(vop_info(node, device))) != 0) {
		panic("fb0: fb_device_init: %e.\n", ret);
	}
	if ((ret = vfs_add_dev("fb0", node, 0)) != 0) {
		panic("fb0: vfs_add_dev: %e.\n", ret);
	}
}
