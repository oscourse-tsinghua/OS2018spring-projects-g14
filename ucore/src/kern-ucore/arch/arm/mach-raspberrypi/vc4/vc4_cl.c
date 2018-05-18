#include "vc4_cl.h"
#include "vc4_context.h"

void vc4_init_cl(struct vc4_cl *cl)
{
	cl->base = 0;
	cl->next = cl->base;
	cl->size = 0;
}

void cl_ensure_space(struct vc4_cl *cl, uint32_t space)
{
	uint32_t offset = cl_offset(cl);

	if (offset + space <= cl->size)
		return;

	uint32_t size = cl->size + space;
	if (cl->size * 2 > size)
		size = cl->size * 2;

	if (cl->base) {
		void *temp = cl->base;
		cl->base = (void *)kmalloc(size);
		memcpy(cl->base, temp, cl->size);
		kfree(temp);
	} else {
		cl->base = (void *)kmalloc(size);
	}
	cl->size = size;
	cl->next = cl->base + offset;
}

void vc4_reset_cl(struct vc4_cl *cl)
{
	cl->next = cl->base;
}

uint32_t vc4_gem_hindex(struct vc4_context *vc4, struct vc4_bo *bo)
{
	uint32_t hindex;
	uint32_t *current_handles = (uint32_t *)vc4->bo_handles.base;

	for (hindex = 0; hindex < cl_offset(&vc4->bo_handles) / 4; hindex++) {
		if (current_handles[hindex] == bo->handle)
			return hindex;
	}

	cl_u32(&vc4->bo_handles, bo->handle);
	return hindex;
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
