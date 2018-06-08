#ifndef GL_CONTEXT_H
#define GL_CONTEXT_H

#include <fb.h>

#include <GLES/gl.h>

#include "pipe/p_state.h"

#define GET_CURRENT_CONTEXT(C) struct gl_context *C = gl_current_context()

struct gl_screen {
	int fb_fd;
	int gpu_fd;
	uint32_t screen_size;
	uint32_t double_buffer_offset;
	struct fb_var_screeninfo var;
};

struct gl_context {
	struct gl_screen screen;
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

inline struct gl_context *gl_current_context(void);

struct gl_context *gl_context_create(void);
void gl_context_destroy(struct gl_context *ctx);

int gl_create_window(struct gl_context *ctx);
int gl_default_state(struct gl_context *ctx);
int gl_swap_buffers(struct gl_context *ctx);

void gl_current_color(struct gl_context *ctx, GLfloat red, GLfloat green,
		      GLfloat blue, GLfloat alpha);
void gl_record_error(struct gl_context *ctx, GLenum error);

#endif // GL_CONTEXT_H
