#include "vc4_cl.h"

void vc4_init_cl(struct vc4_cl *cl)
{
	cl->base = 0;
	cl->next = cl->base;
	cl->size = 0;
}

void vc4_reset_cl(struct vc4_cl *cl)
{
	cl->next = cl->base;
}

void vc4_dump_cl(void *cl, size_t size, size_t cols, const char *name)
{
	kprintf("vc4_dump_cl %s:\n", name);

	uint32_t offset = 0;
	uint8_t *ptr = cl;

	while (offset < size) {
		kprintf("%08x: ", ptr);
		int i;
		for (i = 0; i < cols && offset < size; i++, ptr++, offset++)
			kprintf("%02x ", *(uint8_t *)ptr);
		kprintf("\n");
	}
}
