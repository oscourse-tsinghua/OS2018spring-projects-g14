#ifndef  MACH_BOARD_RASPBERRYPI_H
#define  MACH_BOARD_RASPBERRYPI_H

#ifndef __io_address
#define __io_address(x) (x)
#endif

//IRQ
#define TIMER0_IRQ 0

#define KERNEL_BASE UCONFIG_KERNEL_BASE // 0xc0000000

#define SDRAM0_START UCONFIG_DRAM_START
#define SDRAM0_SIZE  UCONFIG_DRAM_SIZE

#define IO_SPACE_START 0x20000000
#define IO_SPACE_SIZE  0x1000000

#ifndef __ASSEMBLY__

extern void board_init(void);

#endif

#endif
