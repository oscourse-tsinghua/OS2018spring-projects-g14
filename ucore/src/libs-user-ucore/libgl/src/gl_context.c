#include <malloc.h>

#include "gl_context.h"
#include "pipe/p_context.h"

void gl_default_state(struct gl_context *ctx)
{
	// current_color
	{
		gl_current_color(ctx, 1.0f, 1.0f, 1.0f, 1.0f);
	}

	// clear_state
	{
		struct pipe_clear_state *clear_state = &ctx->clear_state;
		clear_state->buffers = 0;
		clear_state->depth = 1.0;
		clear_state->stencil = 0;
		memset(clear_state->color, 0, sizeof(union pipe_color_union));
	}

	// depth_stencil_alpha
	{
		struct pipe_depth_stencil_alpha_state *depth_stencil =
			&ctx->depth_stencil;
		depth_stencil->depth.enabled = 0;
		depth_stencil->depth.writemask = 1;
		depth_stencil->depth.func = PIPE_FUNC_LESS;
		ctx->pipe->set_depth_stencil_alpha_state(ctx->pipe,
							 depth_stencil);
	}

	// viewport
	{
		struct pipe_framebuffer_state *framebuffer = &ctx->framebuffer;
		struct pipe_viewport_state *viewport = &ctx->viewport;
		float half_width = 0.5f * framebuffer->width;
		float half_height = 0.5f * framebuffer->height;
		float n = 0;
		float f = 1;

		viewport->scale[0] = half_width;
		viewport->translate[0] = half_width;
		viewport->scale[1] = half_height;
		viewport->translate[1] = half_height;
		viewport->scale[2] = 0.5 * (f - n);
		viewport->translate[2] = 0.5 * (n + f);

		ctx->pipe->set_viewport_state(ctx->pipe, viewport);
	}
}

void gl_current_color(struct gl_context *ctx, GLfloat red, GLfloat green,
		      GLfloat blue, GLfloat alpha)
{
	ctx->current_color =
		(union pipe_color_union){ red, green, blue, alpha };
}

void gl_record_error(struct gl_context *ctx, GLenum error)
{
	ctx->last_error = error;
}

struct gl_context *gl_context_create(int fd)
{
	struct gl_context *ctx;

	ctx = (struct gl_context *)malloc(sizeof(struct gl_context));
	if (ctx == NULL)
		return NULL;

	memset(ctx, 0, sizeof(struct gl_context));

	ctx->pipe = pipe_context_create(fd);
	if (ctx->pipe == NULL)
		return NULL;

	return ctx;
}

void gl_context_destroy(struct gl_context *ctx)
{
	if (ctx == NULL)
		return;

	ctx->pipe->destroy(ctx->pipe);
	free(ctx);
}
