#include <types.h>
#include <arm.h>
#include <stdio.h>
#include <kio.h>
#include <string.h>
#include <sync.h>
#include <assert.h>
#include <picirq.h>

#include "board.h"
#include "gpio.h"
#include "barrier.h"

#define UART1_BASE          0x20215000

#define AUX_IRQ             (UART1_BASE + 0x00)
#define AUX_ENABLES         (UART1_BASE + 0x04)
#define AUX_MU_IO_REG       (UART1_BASE + 0x40)
#define AUX_MU_IER_REG      (UART1_BASE + 0x44)
#define AUX_MU_IIR_REG      (UART1_BASE + 0x48)
#define AUX_MU_LCR_REG      (UART1_BASE + 0x4C)
#define AUX_MU_MCR_REG      (UART1_BASE + 0x50)
#define AUX_MU_LSR_REG      (UART1_BASE + 0x54)
#define AUX_MU_MSR_REG      (UART1_BASE + 0x58)
#define AUX_MU_SCRATCH      (UART1_BASE + 0x5C)
#define AUX_MU_CNTL_REG     (UART1_BASE + 0x60)
#define AUX_MU_STAT_REG     (UART1_BASE + 0x64)
#define AUX_MU_BAUD_REG     (UART1_BASE + 0x68)

#define AUX_MU_LSR_REG_TX_EMPTY     0x20
#define AUX_MU_LSR_REG_DATA_READY   0x01

#define UART1_IRQ 29

static bool serial_exists = 0;

static int serial_int_handler(int irq, void *data)
{
	extern void dev_stdin_write(char c);
	char c = cons_getc();
	dev_stdin_write(c);
	return 0;
}

void serial_init_early()
{
	if (serial_exists)
		return;

	unsigned int ra;

	outw(AUX_ENABLES, 1);
	outw(AUX_MU_CNTL_REG, 0);
	outw(AUX_MU_LCR_REG, 0x3);	// different from manual
	outw(AUX_MU_MCR_REG, 0);
	outw(AUX_MU_IER_REG, 0x1);	// enable uart rx interrupt (rx and tx on manual is probably reversed)
	outw(AUX_MU_IIR_REG, 0xC6);
	outw(AUX_MU_BAUD_REG, 270);	// see BCM2835 manual section 2.2.1

	ra = inw(GPFSEL1);
	ra &= ~(7 << 12);   // gpio14
	ra |= 2 << 12;      // alt5
	ra &= ~(7 << 15);   // gpio15
	ra |= 2 << 15;      // alt5
	outw(GPFSEL1,ra);

	outw(GPPUD, 0);
	outw(GPPUDCLK0, (1 << 14) | (1 << 15));
	outw(GPPUDCLK0, 0);

	outw(AUX_MU_CNTL_REG, 3);

	serial_exists = 1;
}

void serial_init_mmu()
{
	// make address mapping
	//UART1_BASE is within device address range,
	//therefore ioremap is not necessary for uart

	// init interrupt
	register_irq(UART1_IRQ, serial_int_handler, NULL);
	pic_enable(UART1_IRQ);
}

static void serial_putc_sub(int c)
{
	if (serial_exists) {
		dmb();
		while (!(inb(AUX_MU_LSR_REG) & AUX_MU_LSR_REG_TX_EMPTY)) ;
		dmb();
		outb(AUX_MU_IO_REG, c);
		dmb();
	}
	fb_write(c);
	dmb();
}

/* serial_putc - print character to serial port */
void serial_putc(int c)
{
	if (c == '\b') {
		serial_putc_sub('\b');
		serial_putc_sub(' ');
		serial_putc_sub('\b');
	} else if (c == '\n') {
		serial_putc_sub('\r');
		serial_putc_sub('\n');
	} else {
		serial_putc_sub(c);
	}
}

/* serial_proc_data - get data from serial port */
int serial_proc_data(void)
{
	// return -1 when no char is available at the moment
	//
	int rb = inb(AUX_MU_IIR_REG);
	if ((rb & 1) == 1) {
		return -1;	//no more interrupts
	} else if ((rb & 6) == 4) {
		while (!(inb(AUX_MU_LSR_REG) & AUX_MU_LSR_REG_DATA_READY)) ;
		int c = inb(AUX_MU_IO_REG);
		if (c == 127) {
			c = '\b';
		}
		/*
		   // echo
		   while(!(inb(AUX_MU_LSR_REG)&AUX_MU_LSR_REG_TX_EMPTY)) ;
		   outb(AUX_MU_IO_REG, c);
		 */
		return c;
	} else {
		panic("unexpected AUX_MU_IIR_REG = %02x\n", rb);
	}
}

int serial_check()
{
	return serial_exists;
}

void serial_clear()
{
}
