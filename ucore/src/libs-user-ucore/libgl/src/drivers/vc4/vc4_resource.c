#include <malloc.h>

#include "vc4_resource.h"
#include "vc4_context.h"

static void vc4_resource_destroy(struct pipe_context *pctx,
				 struct pipe_resource *prsc)
{
	struct vc4_resource *rsc = vc4_resource(prsc);
	vc4_bo_unreference(rsc->bo);
	free(rsc);
}

static struct pipe_resource *vc4_resource_create(struct pipe_context *pctx,
						 uint32_t stride,
						 size_t array_size)
{
	struct vc4_context *vc4 = vc4_context(pctx);
	struct vc4_resource *rsc;
	struct vc4_bo *bo;

	rsc = (struct vc4_resource *)malloc(sizeof(struct vc4_resource));
	if (rsc == NULL)
		return NULL;

	memset(rsc, 0, sizeof(struct vc4_resource));

	struct pipe_resource *prsc = &rsc->base;
	prsc->array_size = array_size;

	bo = vc4_bo_alloc(vc4, stride * array_size);
	if (bo == NULL) {
		goto fail;
	}

	rsc->bo = bo;
	rsc->stride = stride;
	return prsc;

fail:
	vc4_resource_destroy(pctx, prsc);
	return NULL;
}

static void *vc4_resource_transfer_map(struct pipe_context *pctx,
				       struct pipe_resource *prsc)
{
	struct vc4_resource *rsc = vc4_resource(prsc);
	return vc4_bo_map(rsc->bo);
}

void vc4_resource_init(struct pipe_context *pctx)
{
	pctx->resource_create = vc4_resource_create;
	pctx->resource_destroy = vc4_resource_destroy;
	pctx->resource_transfer_map = vc4_resource_transfer_map;
}
