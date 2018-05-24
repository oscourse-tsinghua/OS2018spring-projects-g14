#ifndef PIPE_CONTEXT_H
#define PIPE_CONTEXT_H

#include "p_defines.h"

struct pipe_framebuffer_state;
struct pipe_viewport_state;
struct pipe_draw_info;

struct pipe_clear_state {
	unsigned buffers;
	union pipe_color_union color;
	double depth;
	unsigned stencil;
};

struct pipe_vertex_array_state {
	uint8_t size;
	uint16_t stride;
	bool enabled;
	const void *pointer;
};

struct pipe_context {
	struct pipe_clear_state clear_state;
	struct pipe_vertex_array_state vertex_pointer_state;
	struct pipe_vertex_array_state color_pointer_state;
	uint32_t last_error;

	void (*destroy)(struct pipe_context *ctx);
	void (*draw_vbo)(struct pipe_context *ctx, struct pipe_draw_info *info);
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
