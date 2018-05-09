#include <arm.h>

#include "framebuffer.h"
#include "mailbox_property.h"
#include "barrier.h"
#include "teletext.h"

/* Use some free memory in the area below the kernel/stack */
#define BUFFER_ADDRESS 0x1000

/* Screen parameters set in fb_init() */
static unsigned int fb_x, fb_y, pitch;
/* Max x/y character cell */
static unsigned int max_x, max_y;

static struct fb_info __raspberrypi_fb;
struct fb_info *raspberrypi_fb;

/* Framebuffer initialisation failed. Can't display an error, so flashing
 * the OK LED will have to do
 */
static void fb_fail(unsigned int num)
{
	while (1) {
	}
	//output(num);
}

void fb_init_mmu(void)
{
	struct fb_info *fb = raspberrypi_fb;
	if (fb->paddr) {
		fb->screen_base =
			__ucore_ioremap(fb->paddr, fb->screen_size, 0);
		kprintf("GPU addr: %x\n", fb->paddr);
		kprintf("ARM addr: %x\n", fb->screen_base);
	}
}

void fb_init(void)
{
	struct fb_info *fb = &__raspberrypi_fb;
	memset(fb, 0, sizeof(struct fb_info));

	unsigned int var;
	unsigned int count;
	volatile unsigned int mailbuffer[32] __attribute__((aligned(16)));

	/* Get the display size */
	mailbuffer[0] = 8 * 4; // Total size
	mailbuffer[1] = 0; // Request
	mailbuffer[2] = 0x40003; // Display size
	mailbuffer[3] = 8; // Buffer size
	mailbuffer[4] = 0; // Request size
	mailbuffer[5] = 0; // Space for horizontal resolution
	mailbuffer[6] = 0; // Space for vertical resolution
	mailbuffer[7] = 0; // End tag

	writemailbox(8, (unsigned int)mailbuffer);

	var = readmailbox(8);

	/* Valid response in data structure */
	if (mailbuffer[1] != 0x80000000)
		kprintf("Framebuffer: mailbox call to get resolution failed\n");
	/* Mailbox call to get screen resolution failed */

	fb_x = mailbuffer[5];
	fb_y = mailbuffer[6];

	if (fb_x == 0 || fb_y == 0)
		kprintf("Framebuffer: mailbox call returned bad resolution\n");

	/* Set up screen */

	mailbuffer[0] = 22 * 4; // Buffer size
	mailbuffer[1] = 0; // Request

	mailbuffer[2] = 0x00048003; // Tag id (set physical size)
	mailbuffer[3] = 8; // Value buffer size (bytes)
	mailbuffer[4] = 8; // Req. + value length (bytes)
	mailbuffer[5] = fb_x; // Horizontal resolution
	mailbuffer[6] = fb_y; // Vertical resolution

	mailbuffer[7] = 0x00048004; // Tag id (set virtual size)
	mailbuffer[8] = 8; // Value buffer size (bytes)
	mailbuffer[9] = 8; // Req. + value length (bytes)
	mailbuffer[10] = fb_x; // Horizontal resolution
	mailbuffer[11] = fb_y; // Vertical resolution

	mailbuffer[12] = 0x00048005; // Tag id (set depth)
	mailbuffer[13] = 4; // Value buffer size (bytes)
	mailbuffer[14] = 4; // Req. + value length (bytes)
	mailbuffer[15] = 32; // 16 bpp

	mailbuffer[16] = 0x00040001; // Tag id (allocate framebuffer)
	mailbuffer[17] = 8; // Value buffer size (bytes)
	mailbuffer[18] = 4; // Req. + value length (bytes)
	mailbuffer[19] = 16; // Alignment = 16
	mailbuffer[20] = 0; // Space for response

	mailbuffer[21] = 0; // Terminating tag

	writemailbox(8, (unsigned int)mailbuffer);

	var = readmailbox(8);

	/* Valid response in data structure */
	if (mailbuffer[1] != 0x80000000) {
		kprintf("Framebuffer: mailbox call to set up framebuffer failed\n");
		return;
	}

	count = 2; /* First tag */
	while ((var = mailbuffer[count])) {
		if (var == 0x40001)
			break;

		/* Skip to next tag
		 * Advance count by 1 (tag) + 2 (buffer size/value size)
		 *                          + specified buffer size
		 */
		count += 3 + (mailbuffer[count + 1] >> 2);

		if (count > 21) {
			kprintf("Framebuffer: mailbox call to set up framebuffer "
				"returned an invalid list of response tabs\n");
			return;
		}
	}

	/* 8 bytes, plus MSB set to indicate a response */
	if (mailbuffer[count + 2] != 0x80000008) {
		kprintf("Framebuffer: mailbox call returned an invalid response for the framebuffer tag\n");
		return;
	}

	/* Framebuffer address/size in response */
	fb->paddr = mailbuffer[count + 3];
	fb->screen_size = mailbuffer[count + 4];

	if (fb->paddr == 0 || fb->screen_size == 0) {
		kprintf("Framebuffer: mailbox call returned an invalid address/size\n");
		return;
	}

	/* Get the framebuffer pitch (bytes per line) */
	mailbuffer[0] = 7 * 4; // Total size
	mailbuffer[1] = 0; // Request
	mailbuffer[2] = 0x40008; // Display size
	mailbuffer[3] = 4; // Buffer size
	mailbuffer[4] = 0; // Request size
	mailbuffer[5] = 0; // Space for pitch
	mailbuffer[6] = 0; // End tag

	writemailbox(8, (unsigned int)mailbuffer);

	var = readmailbox(8);

	/* 4 bytes, plus MSB set to indicate a response */
	if (mailbuffer[4] != 0x80000004) {
		kprintf("Framebuffer: mailbox call to set pitch returned an invalid response\n");
		return;
	}

	pitch = mailbuffer[5];
	if (pitch == 0) {
		kprintf("Framebuffer: mailbox call to set pitch returned an invalid pitch value\n");
		return;
	}

	/* Need to set up max_x/max_y before using fb_write */
	max_x = fb_x / TELETEXT_W;
	max_y = fb_y / TELETEXT_H;

	raspberrypi_fb = fb;

	kprintf("pitch: %d\n", pitch);

	kprintf("Framebuffer initialised. Address = 0x%08x, size = 0x%08x, resolution = %dx%d\n",
		fb->paddr, fb->screen_size, fb_x, fb_y);
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
	register unsigned int rowbytes = TELETEXT_H * pitch;

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
	struct fb_info *fb = raspberrypi_fb;
	if (!fb->screen_base)
		return;

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
			unsigned int addr = (row + consy * TELETEXT_H) * pitch +
					    consx * TELETEXT_W * 4;
			volatile unsigned int *ptr =
				(unsigned int *)(fb->screen_base + addr);
			for (col = TELETEXT_W - 1; col >= 0; col--) {
				if (teletext[ch][row] & (1 << col))
					*ptr = fgcolour;
				// else
				// 	*ptr = bgcolour;
				ptr++;
			}
		}

		if (++consx >= max_x) {
			newline(fb);
		}
	}
}
