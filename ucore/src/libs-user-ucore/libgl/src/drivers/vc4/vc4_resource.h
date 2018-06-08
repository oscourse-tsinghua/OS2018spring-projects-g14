#ifndef VC4_RESOURCE_H
#define VC4_RESOURCE_H

#include "pipe/p_state.h"
#include "pipe/p_context.h"

struct vc4_resource {
	struct pipe_resource base;
	struct vc4_bo *bo;
	uint32_t stride;
};

static inline struct vc4_resource *vc4_resource(struct pipe_resource *prsc)
{
	return (struct vc4_resource *)prsc;
}

void vc4_resource_init(struct pipe_context *pctx);

#endif // VC4_RESOURCE_H
