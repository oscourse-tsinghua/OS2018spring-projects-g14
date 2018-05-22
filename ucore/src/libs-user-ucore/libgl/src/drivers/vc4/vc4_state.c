#include "vc4_context.h"

static void vc4_set_framebuffer_state(
	struct pipe_context *pctx,
	const struct pipe_framebuffer_state *framebuffer)
{
	struct vc4_context *vc4 = vc4_context(pctx);
	vc4->framebuffer = *framebuffer;
}

static void vc4_set_viewport_state(struct pipe_context *pctx,
				   const struct pipe_viewport_state *viewport)
{
	struct vc4_context *vc4 = vc4_context(pctx);
	vc4->viewport = *viewport;
}

void vc4_state_init(struct pipe_context *pctx)
{
	pctx->set_framebuffer_state = vc4_set_framebuffer_state;
	pctx->set_viewport_state = vc4_set_viewport_state;
}
