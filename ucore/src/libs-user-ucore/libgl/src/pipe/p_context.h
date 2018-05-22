#ifndef PIPE_CONTEXT_H
#define PIPE_CONTEXT_H

#include "p_state.h"

struct pipe_context {
	void (*destroy)(struct pipe_context *ctx);
	void (*draw_vbo)(struct pipe_context *ctx);
	void (*clear)(struct pipe_context *ctx, uint32_t color);
	void (*flush)(struct pipe_context *ctx);

	int (*create_fs_state)(struct pipe_context *);

	void (*set_framebuffer_state)(struct pipe_context *,
				      const struct pipe_framebuffer_state *);

	void (*set_viewport_state)(struct pipe_context *,
				   const struct pipe_viewport_state *);
};

struct pipe_context *pipe_context_create(int fd);

#endif // PIPE_CONTEXT_H
