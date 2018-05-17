#include <error.h>

#include "vc4_drv.h"
#include "vc4_drm.h"
#include "mailbox_property.h"

struct vc4_bo *vc4_bo_create(struct device *dev, size_t size, size_t align)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct vc4_bo *bo;

	// bo = (struct vc4_bo *)kmalloc(sizeof(struct vc4_bo));
	// if (!bo)
	// 	return NULL;

	if (!size)
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
	if (handle >= VC4_DEV_BO_NENTRY) {
		kprintf("VC4: too many bo handles, VC4_DEV_BO_NENTRY = %d\n",
			VC4_DEV_BO_NENTRY);
		goto free_mem;
	}

	uint32_t bus_addr = mbox_mem_lock(handle);
	if (!bus_addr) {
		kprintf("VC4: unable to lock memory at handle %08x\n", handle);
		goto free_mem;
	}

	__boot_map_iomem(bus_addr, size, bus_addr);

	bo = &vc4->handle_bo_map[handle];
	bo->size = size;
	bo->handle = handle;
	bo->paddr = bus_addr;
	bo->vaddr = (void *)bus_addr;

	kprintf("vc4_bo_create: %08x %08x %08x %08x %08x\n", bo->size, align,
		bo->handle, bo->paddr, bo->vaddr);

	return bo;

free_mem:
	mbox_mem_free(handle);
	return NULL;
}

void vc4_bo_destroy(struct device *dev, struct vc4_bo *bo)
{
	__ucore_iounmap(bo->vaddr, bo->size);
	mbox_mem_unlock(bo->handle);
	mbox_mem_free(bo->handle);
	kfree(bo);
}

struct vc4_bo *vc4_lookup_bo(struct device *dev, uint32_t handle)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct vc4_bo *bo;

	if (handle >= VC4_DEV_BO_NENTRY) {
		return NULL;
	}

	bo = &vc4->handle_bo_map[handle];
	if (bo->handle != handle || !bo->size) {
		return NULL;
	}

	return bo;
}

int vc4_create_bo_ioctl(struct device *dev, void *data)
{
	struct drm_vc4_create_bo *args = data;
	struct vc4_bo *bo = NULL;
	int ret;

	bo = vc4_bo_create(dev, args->size, args->align);
	if (bo == NULL)
		return -E_NOMEM;

	args->handle = bo->handle;

	return 0;
}

int vc4_mmap_bo_ioctl(struct device *dev, void *data)
{
	struct drm_vc4_mmap_bo *args = data;
	struct vc4_bo *bo;

	bo = vc4_lookup_bo(dev, args->handle);
	if (bo == NULL) {
		return -E_INVAL;
	}

	args->offset = (uint32_t)bo->vaddr;

	return 0;
}
