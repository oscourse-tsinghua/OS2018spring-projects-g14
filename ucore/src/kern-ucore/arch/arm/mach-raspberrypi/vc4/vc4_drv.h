#ifndef VC4_DRV_H
#define VC4_DRV_H

#include <types.h>

struct vc4_bo {
	size_t size;
	uint32_t handle;
	uint32_t paddr;
	void *vaddr;
};

struct vc4_bo *vc4_bo_create(size_t size, size_t align);
void vc4_bo_destroy(struct vc4_bo *bo);

#endif // VC4_DRV_H
