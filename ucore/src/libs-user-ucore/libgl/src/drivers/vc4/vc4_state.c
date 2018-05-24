#include "vc4_context.h"

static void vc4_set_framebuffer_state(
	struct pipe_context *pctx,
	const struct pipe_framebuffer_state *framebuffer)
{
	struct vc4_context *vc4 = vc4_context(pctx);
	vc4->framebuffer = *framebuffer;
	vc4->dirty |= VC4_DIRTY_FRAMEBUFFER;
}

static void vc4_set_viewport_state(struct pipe_context *pctx,
				   const struct pipe_viewport_state *viewport)
{
	struct vc4_context *vc4 = vc4_context(pctx);
	vc4->viewport = *viewport;
	vc4->dirty |= VC4_DIRTY_VIEWPORT;
}

static void vc4_set_vertex_buffers(struct pipe_context *pctx,
				   const struct pipe_vertex_buffer *vb)
{
	struct vc4_context *vc4 = vc4_context(pctx);
	vc4->vertexbuf = *vb;
	vc4->dirty |= VC4_DIRTY_VTXBUF;
}

void vc4_state_init(struct pipe_context *pctx)
{
	pctx->set_framebuffer_state = vc4_set_framebuffer_state;
	pctx->set_viewport_state = vc4_set_viewport_state;
	pctx->set_vertex_buffers = vc4_set_vertex_buffers;
}
