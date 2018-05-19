#include <vmm.h>
#include <error.h>

#include "vc4_drv.h"
#include "vc4_drm.h"
#include "mailbox_property.h"

struct vc4_bo *vc4_bo_create(struct device *dev, size_t size)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct vc4_bo *bo;

	// bo = (struct vc4_bo *)kmalloc(sizeof(struct vc4_bo));
	// if (!bo)
	// 	return NULL;

	if (!size)
		return NULL;

	size = ROUNDUP(size, PGSIZE);

	uint32_t handle =
		mbox_mem_alloc(size, PGSIZE, MEM_FLAG_COHERENT | MEM_FLAG_ZERO);
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

	kprintf("vc4_bo_create: %08x %08x %08x %08x\n", bo->size, bo->handle,
		bo->paddr, bo->vaddr);

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

	bo = vc4_bo_create(dev, args->size);
	if (bo == NULL)
		return -E_NOMEM;

	args->handle = bo->handle;

	return 0;
}

static int vc4_mmap(struct device *dev, struct vma_struct *vma, uintptr_t paddr)
{
	uintptr_t start = paddr;
	start &= ~(PGSIZE - 1);
	void *r = (void *)remap_pfn_range(vma->vm_start, start >> PGSHIFT,
					  vma->vm_end - vma->vm_start);
	if (!r) {
		return -E_NOMEM;
	}
	vma->vm_start = (uintptr_t)r;
	return 0;
}

int vc4_mmap_bo_ioctl(struct device *dev, void *data)
{
	struct drm_vc4_mmap_bo *args = data;
	struct vc4_bo *bo;
	int ret = 0;

	bo = vc4_lookup_bo(dev, args->handle);
	if (bo == NULL) {
		return -E_INVAL;
	}

	struct vma_struct *vma = NULL;
	vma = (struct vma_struct *)kmalloc(sizeof(struct vma_struct));
	if (!vma) {
		return -E_NOMEM;
	}

	uint32_t len = ROUNDUP(bo->size, PGSIZE);
	vma->vm_start = 0;
	vma->vm_end = vma->vm_start + len;

	ret = vc4_mmap(dev, vma, bo->paddr);
	args->offset = vma->vm_start;
	kfree(vma);

	return ret;
}
