#ifndef PIPE_CONTEXT_H
#define PIPE_CONTEXT_H

#include "p_defines.h"

struct pipe_framebuffer_state;
struct pipe_viewport_state;
struct pipe_clear_state;

struct pipe_clear_state {
	unsigned buffers;
	union pipe_color_union color;
	double depth;
	unsigned stencil;
};

struct pipe_context {
	struct pipe_clear_state clear_state;

	void (*destroy)(struct pipe_context *ctx);
	void (*draw_vbo)(struct pipe_context *ctx);
	void (*clear)(struct pipe_context *ctx, unsigned buffers,
		      const union pipe_color_union *color, double depth,
		      unsigned stencil);
	void (*flush)(struct pipe_context *ctx);

	int (*create_fs_state)(struct pipe_context *);

	void (*set_framebuffer_state)(struct pipe_context *,
				      const struct pipe_framebuffer_state *);

	void (*set_viewport_state)(struct pipe_context *,
				   const struct pipe_viewport_state *);
};

struct pipe_context *pipe_context_create(int fd);

#endif // PIPE_CONTEXT_H
