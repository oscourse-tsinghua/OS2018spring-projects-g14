#include <board.h>
#include <picirq.h>
#include <serial.h>
#include <clock.h>

#include "framebuffer.h"
#include "vc4/vc4_gl.h"

static const char *message = "Initializing Raspberry Pi Board...\n";

static void put_string(const char *str)
{
	while (*str)
		serial_putc(*str++);
}

void board_init_early()
{
	// init serial
	serial_init_early();

	put_string(message);

	pic_init();		// init interrupt controller
	// fixed base and irq
	clock_init_arm(0, 0);	// linux put tick_init in kernel_main, so do we~
}

void board_init()
{
	serial_init_mmu();
	fb_init();
	vc4_hello_triangle();
}

/* no nand */
int check_nandflash()
{
	return 0;
}

struct nand_chip *get_nand_chip()
{
	return NULL;
}
