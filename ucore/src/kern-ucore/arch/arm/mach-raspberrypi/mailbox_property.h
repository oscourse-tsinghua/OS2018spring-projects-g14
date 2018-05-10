#ifndef RASPBERRYPI_MAILBOX_PROPERTY_H
#define RASPBERRYPI_MAILBOX_PROPERTY_H

#include <arm.h>

#define MBOX_CHAN_FB			1
#define MBOX_CHAN_PROPERTY		8

enum rpi_firmware_property_status {
	RPI_FIRMWARE_STATUS_REQUEST = 0,
	RPI_FIRMWARE_STATUS_SUCCESS = 0x80000000,
	RPI_FIRMWARE_STATUS_ERROR =   0x80000001,
};

struct rpi_firmware_property_tag_header {
	uint32_t tag;
	uint32_t buf_size;
	uint32_t req_resp_size;
};

enum rpi_firmware_property_tag {
	RPI_FIRMWARE_PROPERTY_END =                           0,
	RPI_FIRMWARE_GET_FIRMWARE_REVISION =                  0x00000001,

	RPI_FIRMWARE_SET_CURSOR_INFO =                        0x00008010,
	RPI_FIRMWARE_SET_CURSOR_STATE =                       0x00008011,

	RPI_FIRMWARE_GET_BOARD_MODEL =                        0x00010001,
	RPI_FIRMWARE_GET_BOARD_REVISION =                     0x00010002,
	RPI_FIRMWARE_GET_BOARD_MAC_ADDRESS =                  0x00010003,
	RPI_FIRMWARE_GET_BOARD_SERIAL =                       0x00010004,
	RPI_FIRMWARE_GET_ARM_MEMORY =                         0x00010005,
	RPI_FIRMWARE_GET_VC_MEMORY =                          0x00010006,
	RPI_FIRMWARE_GET_CLOCKS =                             0x00010007,
	RPI_FIRMWARE_GET_POWER_STATE =                        0x00020001,
	RPI_FIRMWARE_GET_TIMING =                             0x00020002,
	RPI_FIRMWARE_SET_POWER_STATE =                        0x00028001,
	RPI_FIRMWARE_GET_CLOCK_STATE =                        0x00030001,
	RPI_FIRMWARE_GET_CLOCK_RATE =                         0x00030002,
	RPI_FIRMWARE_GET_VOLTAGE =                            0x00030003,
	RPI_FIRMWARE_GET_MAX_CLOCK_RATE =                     0x00030004,
	RPI_FIRMWARE_GET_MAX_VOLTAGE =                        0x00030005,
	RPI_FIRMWARE_GET_TEMPERATURE =                        0x00030006,
	RPI_FIRMWARE_GET_MIN_CLOCK_RATE =                     0x00030007,
	RPI_FIRMWARE_GET_MIN_VOLTAGE =                        0x00030008,
	RPI_FIRMWARE_GET_TURBO =                              0x00030009,
	RPI_FIRMWARE_GET_MAX_TEMPERATURE =                    0x0003000a,
	RPI_FIRMWARE_GET_STC =                                0x0003000b,
	RPI_FIRMWARE_ALLOCATE_MEMORY =                        0x0003000c,
	RPI_FIRMWARE_LOCK_MEMORY =                            0x0003000d,
	RPI_FIRMWARE_UNLOCK_MEMORY =                          0x0003000e,
	RPI_FIRMWARE_RELEASE_MEMORY =                         0x0003000f,
	RPI_FIRMWARE_EXECUTE_CODE =                           0x00030010,
	RPI_FIRMWARE_EXECUTE_QPU =                            0x00030011,
	RPI_FIRMWARE_SET_ENABLE_QPU =                         0x00030012,
	RPI_FIRMWARE_GET_DISPMANX_RESOURCE_MEM_HANDLE =       0x00030014,
	RPI_FIRMWARE_GET_EDID_BLOCK =                         0x00030020,
	RPI_FIRMWARE_GET_CUSTOMER_OTP =                       0x00030021,
	RPI_FIRMWARE_GET_DOMAIN_STATE =                       0x00030030,
	RPI_FIRMWARE_SET_CLOCK_STATE =                        0x00038001,
	RPI_FIRMWARE_SET_CLOCK_RATE =                         0x00038002,
	RPI_FIRMWARE_SET_VOLTAGE =                            0x00038003,
	RPI_FIRMWARE_SET_TURBO =                              0x00038009,
	RPI_FIRMWARE_SET_CUSTOMER_OTP =                       0x00038021,
	RPI_FIRMWARE_SET_DOMAIN_STATE =                       0x00038030,
	RPI_FIRMWARE_GET_GPIO_STATE =                         0x00030041,
	RPI_FIRMWARE_SET_GPIO_STATE =                         0x00038041,
	RPI_FIRMWARE_SET_SDHOST_CLOCK =                       0x00038042,
	RPI_FIRMWARE_GET_GPIO_CONFIG =                        0x00030043,
	RPI_FIRMWARE_SET_GPIO_CONFIG =                        0x00038043,
	RPI_FIRMWARE_GET_PERIPH_REG =                         0x00030045,
	RPI_FIRMWARE_SET_PERIPH_REG =                         0x00038045,

	/* Dispmanx TAGS */
	RPI_FIRMWARE_FRAMEBUFFER_ALLOCATE =                   0x00040001,
	RPI_FIRMWARE_FRAMEBUFFER_BLANK =                      0x00040002,
	RPI_FIRMWARE_FRAMEBUFFER_GET_PHYSICAL_WIDTH_HEIGHT =  0x00040003,
	RPI_FIRMWARE_FRAMEBUFFER_GET_VIRTUAL_WIDTH_HEIGHT =   0x00040004,
	RPI_FIRMWARE_FRAMEBUFFER_GET_DEPTH =                  0x00040005,
	RPI_FIRMWARE_FRAMEBUFFER_GET_PIXEL_ORDER =            0x00040006,
	RPI_FIRMWARE_FRAMEBUFFER_GET_ALPHA_MODE =             0x00040007,
	RPI_FIRMWARE_FRAMEBUFFER_GET_PITCH =                  0x00040008,
	RPI_FIRMWARE_FRAMEBUFFER_GET_VIRTUAL_OFFSET =         0x00040009,
	RPI_FIRMWARE_FRAMEBUFFER_GET_OVERSCAN =               0x0004000a,
	RPI_FIRMWARE_FRAMEBUFFER_GET_PALETTE =                0x0004000b,
	RPI_FIRMWARE_FRAMEBUFFER_GET_TOUCHBUF =               0x0004000f,
	RPI_FIRMWARE_FRAMEBUFFER_GET_GPIOVIRTBUF =            0x00040010,
	RPI_FIRMWARE_FRAMEBUFFER_RELEASE =                    0x00048001,
	RPI_FIRMWARE_FRAMEBUFFER_TEST_PHYSICAL_WIDTH_HEIGHT = 0x00044003,
	RPI_FIRMWARE_FRAMEBUFFER_TEST_VIRTUAL_WIDTH_HEIGHT =  0x00044004,
	RPI_FIRMWARE_FRAMEBUFFER_TEST_DEPTH =                 0x00044005,
	RPI_FIRMWARE_FRAMEBUFFER_TEST_PIXEL_ORDER =           0x00044006,
	RPI_FIRMWARE_FRAMEBUFFER_TEST_ALPHA_MODE =            0x00044007,
	RPI_FIRMWARE_FRAMEBUFFER_TEST_VIRTUAL_OFFSET =        0x00044009,
	RPI_FIRMWARE_FRAMEBUFFER_TEST_OVERSCAN =              0x0004400a,
	RPI_FIRMWARE_FRAMEBUFFER_TEST_PALETTE =               0x0004400b,
	RPI_FIRMWARE_FRAMEBUFFER_TEST_VSYNC =                 0x0004400e,
	RPI_FIRMWARE_FRAMEBUFFER_SET_PHYSICAL_WIDTH_HEIGHT =  0x00048003,
	RPI_FIRMWARE_FRAMEBUFFER_SET_VIRTUAL_WIDTH_HEIGHT =   0x00048004,
	RPI_FIRMWARE_FRAMEBUFFER_SET_DEPTH =                  0x00048005,
	RPI_FIRMWARE_FRAMEBUFFER_SET_PIXEL_ORDER =            0x00048006,
	RPI_FIRMWARE_FRAMEBUFFER_SET_ALPHA_MODE =             0x00048007,
	RPI_FIRMWARE_FRAMEBUFFER_SET_VIRTUAL_OFFSET =         0x00048009,
	RPI_FIRMWARE_FRAMEBUFFER_SET_OVERSCAN =               0x0004800a,
	RPI_FIRMWARE_FRAMEBUFFER_SET_PALETTE =                0x0004800b,
	RPI_FIRMWARE_FRAMEBUFFER_SET_TOUCHBUF =               0x0004801f,
	RPI_FIRMWARE_FRAMEBUFFER_SET_GPIOVIRTBUF =            0x00048020,
	RPI_FIRMWARE_FRAMEBUFFER_SET_VSYNC =                  0x0004800e,
	RPI_FIRMWARE_FRAMEBUFFER_SET_BACKLIGHT =              0x0004800f,

	RPI_FIRMWARE_VCHIQ_INIT =                             0x00048010,

	RPI_FIRMWARE_GET_COMMAND_LINE =                       0x00050001,
	RPI_FIRMWARE_GET_DMA_CHANNELS =                       0x00060001,
};

// Flags for allocate memory.
enum mbox_alloc_mem_flags {
	/* can be resized to 0 at any time. Use for cached data */
	MEM_FLAG_DISCARDABLE = 1 << 0,

	/* normal allocating alias. Don't use from ARM */
	MEM_FLAG_NORMAL = 0 << 2,

	/* 0xC alias uncached */
	MEM_FLAG_DIRECT = 1 << 2,

	/* 0x8 alias. Non-allocating in L2 but coherent */
	MEM_FLAG_COHERENT = 2 << 2,

	/* Allocating in L2 */
	MEM_FLAG_L1_NONALLOCATING = (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT),

	/* initialise buffer to all zeros */
	MEM_FLAG_ZERO = 1 << 4,

	/* don't initialise (default is initialise to all ones */
	MEM_FLAG_NO_INIT = 1 << 5,

	/* Likely to be locked for long periods of time. */
	MEM_FLAG_HINT_PERMALOCK = 1 << 6,
};

int mbox_property_list(void *data, size_t tag_size);
int mbox_property(uint32_t tag, void *tag_data, size_t buf_size);

void *mbox_mapmem(uint32_t base, size_t size);
void mbox_unmapmem(void *addr, size_t size);

uint32_t mbox_mem_alloc(size_t size, size_t align, uint32_t flags);
int mbox_mem_free(uint32_t handle);
uint32_t mbox_mem_lock(uint32_t handle);
int mbox_mem_unlock(uint32_t handle);

int mbox_execute_code(uint32_t code, uint32_t r0, uint32_t r1, uint32_t r2,
		      uint32_t r3, uint32_t r4, uint32_t r5, uint32_t *out_r0);
int mbox_execute_qpu(uint32_t num_qpus, uint32_t control, uint32_t noflush,
		     uint32_t timeout);
int mbox_qpu_enable(uint32_t enable);

#endif // RASPBERRYPI_MAILBOX_PROPERTY_H
