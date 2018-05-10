#include <error.h>

#include "mailbox.h"
#include "mailbox_property.h"

#define PAGE_SIZE (4 * 1024)
#define MAX_MESSAGE_BUFFER_SIZE (4096)

int mbox_property_list(void *data, size_t tag_size)
{
	size_t size = tag_size + 12;
	uint32_t __attribute__((aligned(16))) buf[MAX_MESSAGE_BUFFER_SIZE / 4];
	if (size >= MAX_MESSAGE_BUFFER_SIZE) {
		kprintf("Mailbox: messages are too long.\n");
		return -E_INVAL;
	}

	buf[0] = size;
	buf[1] = RPI_FIRMWARE_STATUS_REQUEST;
	memcpy(&buf[2], data, tag_size);
	buf[size / 4 - 1] = RPI_FIRMWARE_PROPERTY_END;

	mbox_write(MBOX_CHAN_PROPERTY, (uint32_t)buf);
	mbox_read(MBOX_CHAN_PROPERTY);

	if (buf[1] != RPI_FIRMWARE_STATUS_SUCCESS) {
		kprintf("Mailbox: request 0x%08x returned status 0x%08x.\n",
			buf[2], buf[1]);
		return -E_INVAL;
	}

	memcpy(data, &buf[2], tag_size);
	return 0;
}

int mbox_property(uint32_t tag, void *tag_data, size_t buf_size)
{
	uint8_t data[buf_size + sizeof(struct rpi_firmware_property_tag_header)];
	struct rpi_firmware_property_tag_header *header =
		(struct rpi_firmware_property_tag_header *)data;
	int ret = 0;

	header->tag = tag;
	header->buf_size = buf_size;
	header->req_resp_size = 0;
	memcpy(data + sizeof(struct rpi_firmware_property_tag_header), tag_data,
	       buf_size);

	ret = mbox_property_list(data, sizeof(data));
	memcpy(tag_data, data + sizeof(struct rpi_firmware_property_tag_header),
	       buf_size);

	return ret;
}

void *mbox_mapmem(uint32_t base, size_t size)
{
	// FIXME
	kprintf("mapmem before: %x\n", base);

	uint32_t offset = base % PAGE_SIZE;
	base = base - offset;
	base = __ucore_ioremap(base, size, 0);

	kprintf("mapmem after: %x\n", base);
	return (char *)base + offset;
}

void mbox_unmapmem(void *addr, size_t size)
{
	// FIXME unimplemented
}

uint32_t mbox_mem_alloc(size_t size, size_t align, uint32_t flags)
{
	uint32_t data[3] = { size, align, flags };
	int ret =
		mbox_property(RPI_FIRMWARE_ALLOCATE_MEMORY, data, sizeof(data));
	if (ret != 0)
		return 0;
	return data[0];
}

int mbox_mem_free(uint32_t handle)
{
	uint32_t data[1] = { handle };
	int ret =
		mbox_property(RPI_FIRMWARE_RELEASE_MEMORY, data, sizeof(data));
	if (data[0] != 0)
		ret = -E_INVAL;
	return ret;
}

uint32_t mbox_mem_lock(uint32_t handle)
{
	uint32_t data[1] = { handle };
	int ret = mbox_property(RPI_FIRMWARE_LOCK_MEMORY, data, sizeof(data));
	if (ret != 0)
		return 0;
	return data[0];
}

int mbox_mem_unlock(uint32_t handle)
{
	uint32_t data[1] = { handle };
	int ret = mbox_property(RPI_FIRMWARE_UNLOCK_MEMORY, data, sizeof(data));
	if (data[0] != 0)
		ret = -E_INVAL;
	return ret;
}

int mbox_execute_code(uint32_t code, uint32_t r0, uint32_t r1, uint32_t r2,
		      uint32_t r3, uint32_t r4, uint32_t r5, uint32_t *out_r0)
{
	uint32_t data[7] = { code, r0, r1, r2, r3, r4, r5 };
	int ret = mbox_property(RPI_FIRMWARE_EXECUTE_CODE, data, sizeof(data));
	*out_r0 = data[0];
	return ret;
}

int mbox_qpu_enable(uint32_t enable)
{
	uint32_t data[1] = { enable };
	int ret =
		mbox_property(RPI_FIRMWARE_SET_ENABLE_QPU, data, sizeof(data));
	return ret;
}

int mbox_execute_qpu(uint32_t num_qpus, uint32_t control, uint32_t noflush,
		     uint32_t timeout)
{
	uint32_t data[4] = { num_qpus, control, noflush, timeout };
	int ret = mbox_property(RPI_FIRMWARE_EXECUTE_QPU, data, sizeof(data));
	return ret;
}

int mbox_framebuffer_get_physical_size(uint32_t *width, uint32_t *height)
{
	uint32_t data[2] = { *width, *height };
	int ret = mbox_property(
		RPI_FIRMWARE_FRAMEBUFFER_GET_PHYSICAL_WIDTH_HEIGHT, data,
		sizeof(data));
	*width = data[0];
	*height = data[1];
	return ret;
}

int mbox_framebuffer_get_depth(uint32_t *depth)
{
	uint32_t data[1] = { *depth };
	int ret = mbox_property(RPI_FIRMWARE_FRAMEBUFFER_GET_DEPTH, data,
				sizeof(data));
	*depth = data[0];
	return ret;
}

int mbox_framebuffer_alloc(struct fb_alloc_tags *fb_info)
{
	int ret = mbox_property_list(fb_info, sizeof(struct fb_alloc_tags));
	return ret;
}
