
#ifndef RASPBERRYPI_MAILBOX_H
#define RASPBERRYPI_MAILBOX_H

#include <types.h>

extern uint32_t mbox_read(uint32_t channel);
extern void mbox_write(uint32_t channel, uint32_t data);

#endif /* RASPBERRYPI_MAILBOX_H */
