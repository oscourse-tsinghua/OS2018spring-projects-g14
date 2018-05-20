#ifndef VC4_BUFMGR_H
#define VC4_BUFMGR_H

#include <types.h>

#include "vc4_context.h"

struct vc4_bo {
	size_t size;
	uint32_t handle;
	void *map;
};

struct vc4_bo *vc4_bo_alloc(struct vc4_context *vc4, size_t size);
void vc4_bo_free(struct vc4_context *vc4, struct vc4_bo *bo);
void *vc4_bo_map(struct vc4_context *vc4, struct vc4_bo *bo);

#endif // VC4_BUFMGR_H
