#ifndef VC4_BUFMGR_H
#define VC4_BUFMGR_H

#include <types.h>

struct vc4_bo {
	size_t size;
	uint32_t handle;
	void *map;
};

struct vc4_bo *vc4_bo_alloc(size_t size, size_t align);
struct vc4_bo *vc4_bo_free(struct vc4_bo *bo);
void *vc4_bo_map(struct vc4_bo *bo);

#endif // VC4_BUFMGR_H
