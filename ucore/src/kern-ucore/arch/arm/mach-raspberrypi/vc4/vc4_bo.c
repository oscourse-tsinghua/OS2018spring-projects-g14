#include "vc4_drv.h"
#include "mailbox_property.h"

struct vc4_bo *vc4_bo_create(size_t size, size_t align)
{
	struct vc4_bo *bo;

	bo = (struct vc4_bo *)kmalloc(sizeof(struct vc4_bo));
	if (!bo)
		return NULL;

	if (!align)
		align = 1;

	uint32_t handle =
		mbox_mem_alloc(size, align, MEM_FLAG_COHERENT | MEM_FLAG_ZERO);
	if (!handle) {
		kprintf("VC4: unable to allocate memory with size %08x\n",
			size);
		return NULL;
	}

	uint32_t bus_addr = mbox_mem_lock(handle);
	if (!bus_addr) {
		kprintf("VC4: unable to lock memory at handle %08x\n", handle);
		return NULL;
	}

	__boot_map_iomem(bus_addr, size, bus_addr);

	bo->size = size;
	bo->handle = handle;
	bo->paddr = bus_addr;
	bo->vaddr = (void *)bus_addr;

	kprintf("vc4_bo_create: %08x %08x %08x %08x %08x\n", bo->size, align,
		bo->handle, bo->paddr, bo->vaddr);

	return bo;
}

void vc4_bo_destroy(struct vc4_bo *bo)
{
	__ucore_iounmap(bo->vaddr, bo->size);
	mbox_mem_unlock(bo->handle);
	mbox_mem_free(bo->handle);
	kfree(bo);
}
