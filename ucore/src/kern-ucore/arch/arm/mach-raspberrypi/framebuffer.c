#include <arm.h>

#include "framebuffer.h"
#include "mailbox_property.h"
#include "barrier.h"
#include "teletext.h"

/* Screen parameters set in fb_init() */
/* Max x/y character cell */
static unsigned int max_x, max_y;

static struct fb_info __raspberrypi_fb;
struct fb_info *raspberrypi_fb = 0;

bool raspberrypi_fb_exist = 0;

/* Framebuffer initialisation failed. Can't display an error, so flashing
 * the OK LED will have to do
 */
static void fb_fail(unsigned int num)
{
	while (1) {
	}
	//output(num);
}

void fb_init(void)
{
	struct fb_info *fb = &__raspberrypi_fb;
	memset(fb, 0, sizeof(struct fb_info));

	uint32_t width, height, depth;

	if (mbox_framebuffer_get_physical_size(&width, &height) != 0) {
		kprintf("Framebuffer: cannot get physical size!\n");
		goto fail;
	}
	if (mbox_framebuffer_get_depth(&depth) != 0) {
		kprintf("Framebuffer: cannot get color depth!\n");
		goto fail;
	}

	struct fb_alloc_tags fb_data = {
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

	if (mbox_property_list(&fb_data, sizeof(fb_data)) != 0) {
		kprintf("Framebuffer: cannot allocate GPU framebuffer.");
		goto fail;
	}

	if (fb_data.base == 0 || fb_data.screen_size == 0) {
		kprintf("Framebuffer: mailbox call returned an invalid address/size.\n");
		goto fail;
	}
	if (fb_data.pitch == 0) {
		kprintf("Framebuffer: mailbox call to set pitch returned an invalid pitch value.\n");
		goto fail;
	}

	fb->width = fb_data.xres;
	fb->height = fb_data.yres;
	fb->width_virtual = fb_data.xres_virtual;
	fb->height_virtual = fb_data.yres_virtual;
	fb->xoffset = fb_data.xoffset;
	fb->yoffset = fb_data.yoffset;
	fb->bits_per_pixel = fb_data.bpp;
	fb->pitch = fb_data.pitch;
	fb->bus_addr = fb_data.base;
	fb->screen_size = fb_data.screen_size;
	fb->screen_base = __ucore_ioremap(fb->bus_addr, fb->screen_size, 0);

	raspberrypi_fb = fb;
	raspberrypi_fb_exist = 1;

	/* Need to set up max_x/max_y before using fb_write */
	max_x = fb->width / TELETEXT_W;
	max_y = fb->height / TELETEXT_H;

	kprintf("Framebuffer initialised.\n"
		"base=0x%08x, resolution=%dx%d, bpp=%d, pitch=%d, size=%d\n",
		fb->screen_base, fb->width, fb->height, fb->bits_per_pixel, fb->pitch,
		fb->screen_size);

	return;

fail:
	kprintf("Framebuffer initialization failed.\n");
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
	register unsigned int rowbytes = TELETEXT_H * fb->pitch;

	consx = 0;
	if (consy < (max_y - 1)) {
		consy++;
	} else {
		/* Copy a screen's worth of data (minus 1 character row) from the
		 * second row to the first
		 */
		unsigned int source = fb->screen_base + rowbytes;
		memmove((void *)fb->screen_base, (void *)source,
			(max_y - 1) * rowbytes);
		/* Clear last line on screen */
		memset((void *)(fb->screen_base + (max_y - 1) * rowbytes), 0,
		       rowbytes);
	}
}

/* Write null-terminated text to the console
 * Supports control characters (see framebuffer.h) for colour and newline
 */
void fb_write(unsigned char ch) //char *text)
{
	if (raspberrypi_fb_exist == 0)
		return;

	struct fb_info *fb = raspberrypi_fb;

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
			uint32_t bytes_per_pixel = fb->bits_per_pixel >> 3;
			uint32_t addr = (row + consy * TELETEXT_H) * fb->pitch +
					consx * TELETEXT_W * bytes_per_pixel;
			uint8_t *ptr = (uint8_t *)fb->screen_base + addr;
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
