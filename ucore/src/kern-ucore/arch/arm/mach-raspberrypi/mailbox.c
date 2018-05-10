/*
 * Access system mailboxes
 */

#include "mailbox.h"
#include "barrier.h"

#define MAILBOX_BASE		0x2000b880

/* Mailboxes */
#define ARM_0_MAIL0		(MAILBOX_BASE + 0x00)
#define ARM_0_MAIL1		(MAILBOX_BASE + 0x20)

/*
 * Mailbox registers. We basically only support mailbox 0 & 1. We
 * deliver to the VC in mailbox 1, it delivers to us in mailbox 0. See
 * BCM2835-ARM-Peripherals.pdf section 1.3 for an explanation about
 * the placement of memory barriers.
 */
#define MAIL0_RD		(ARM_0_MAIL0 + 0x00)
#define MAIL0_POL		(ARM_0_MAIL0 + 0x10)
#define MAIL0_STA		(ARM_0_MAIL0 + 0x18)
#define MAIL0_CNF		(ARM_0_MAIL0 + 0x1C)
#define MAIL1_WRT		(ARM_0_MAIL1 + 0x00)
#define MAIL1_STA		(ARM_0_MAIL1 + 0x18)

/* Bit 31 set in status register if the write mailbox is full */
/* Bit 30 set in status register if the read mailbox is empty */
#define MAILBOX_FULL		(1 << 31)
#define MAILBOX_EMPTY		(1 << 30)

#define MBOX_MSG(chan, data28)		(((data28) & ~0xf) | ((chan) & 0xf))
#define MBOX_CHAN(msg)			((msg) & 0xf)
#define MBOX_DATA28(msg)		((msg) & ~0xf)

uint32_t mbox_read(uint32_t channel)
{
	uint32_t count = 0;
	uint32_t data;

	/* Loop until something is received from channel
	 * If nothing recieved, it eventually give up and returns 0xffffffff
	 */
	while (1) {
		while (inw(MAIL0_STA) & MAILBOX_EMPTY) {
			/* Need to check if this is the right thing to do */
			flushcache();

			/* This is an arbritarily large number */
			if (count++ > (1 << 25)) {
				return 0xffffffff;
			}
		}
		/* Read the data
		 * Data memory barriers as we've switched peripheral
		 */
		dmb();
		data = inw(MAIL0_RD);
		dmb();

		if (MBOX_CHAN(data) == channel)
			return MBOX_DATA28(data);
	}
}

void mbox_write(uint32_t channel, uint32_t data)
{
	/* Wait for mailbox to be not full */
	while (inw(MAIL1_STA) & MAILBOX_FULL) {
		/* Need to check if this is the right thing to do */
		flushcache();
	}

	dmb();
	outw(MAIL1_WRT, MBOX_MSG(channel, data));
}
