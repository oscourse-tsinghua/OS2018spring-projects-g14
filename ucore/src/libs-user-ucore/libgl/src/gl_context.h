#ifndef GL_CONTEXT_H
#define GL_CONTEXT_H

#include <GLES/gl.h>

#include "pipe/p_state.h"

struct gl_context {
	struct pipe_context *pipe;

	struct pipe_viewport_state viewport;
	struct pipe_framebuffer_state framebuffer;
	struct pipe_clear_state clear_state;
	struct pipe_vertex_array_state vertex_pointer_state;
	struct pipe_vertex_array_state color_pointer_state;
	struct pipe_depth_stencil_alpha_state depth_stencil;

	union pipe_color_union current_color;
	GLenum last_error;
};

void gl_default_state(struct gl_context *ctx);

void gl_current_color(struct gl_context *ctx, GLfloat red, GLfloat green,
		      GLfloat blue, GLfloat alpha);
void gl_record_error(struct gl_context *ctx, GLenum error);

struct gl_context *gl_context_create(int fd);
void gl_context_destroy(struct gl_context *ctx);

#endif // GL_CONTEXT_H
