#include "vc4_cl.h"

void vc4_init_cl(struct vc4_cl *cl, uint32_t paddr, void *vaddr, size_t size)
{
	cl->paddr = paddr;
	cl->vaddr = vaddr;
	cl->next = cl->vaddr;
	cl->size = size;
}

void vc4_reset_cl(struct vc4_cl *cl)
{
	cl->next = cl->vaddr;
}

void cl_dump(struct vc4_cl *cl, size_t cols, const char *name)
{
	kprintf("cl_dump %s:\n", name);
	void *ptr;
	for (ptr = cl->vaddr; ptr < cl->next;) {
		kprintf("%08x: ", ptr);
		int i;
		for (i = 0; i < cols && ptr < cl->next; i++, ptr++)
			kprintf("%02x ", *(uint8_t *)ptr);
		kprintf("\n");
	}
}
