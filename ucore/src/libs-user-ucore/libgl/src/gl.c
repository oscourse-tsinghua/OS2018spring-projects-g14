#include <GLES/gl.h>

#include "pipe/p_state.h"
#include "pipe/p_defines.h"
#include "pipe/p_context.h"

struct pipe_context *pipe_ctx;

GL_API void GL_APIENTRY glClearColor(GLfloat red, GLfloat green, GLfloat blue,
				     GLfloat alpha)
{
	struct pipe_clear_state *clear_state = &pipe_ctx->clear_state;
	clear_state->color.f[0] = red;
	clear_state->color.f[1] = green;
	clear_state->color.f[2] = blue;
	clear_state->color.f[3] = alpha;
}

GL_API void GL_APIENTRY glClearDepthf(GLclampf depth)
{
	pipe_ctx->clear_state.depth = depth;
}

GL_API void GL_APIENTRY glClearStencil(GLint s)
{
	pipe_ctx->clear_state.stencil = s;
}

GL_API void GL_APIENTRY glClear(GLbitfield mask)
{
	if (mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
		     GL_STENCIL_BUFFER_BIT)) {
		pipe_ctx->last_error = GL_INVALID_VALUE;
		return;
	}

	struct pipe_clear_state *clear_state = &pipe_ctx->clear_state;

	clear_state->buffers = 0;
	if (mask & GL_COLOR_BUFFER_BIT) {
		clear_state->buffers |= PIPE_CLEAR_COLOR;
	}
	if (mask & GL_DEPTH_BUFFER_BIT) {
		clear_state->buffers |= PIPE_CLEAR_DEPTH;
	}
	if (mask & GL_STENCIL_BUFFER_BIT) {
		clear_state->buffers |= PIPE_CLEAR_STENCIL;
	}

	pipe_ctx->clear(pipe_ctx, clear_state->buffers, &clear_state->color,
			clear_state->depth, clear_state->stencil);
}

GL_API void GL_APIENTRY glColorPointer(GLint size, GLenum type, GLsizei stride,
				       const void *pointer)
{
}

GL_API void GL_APIENTRY glDisableClientState(GLenum array)
{
	if (array == GL_VERTEX_ARRAY) {
		pipe_ctx->vertex_pointer_state.enabled = 0;
	} else if (array == GL_COLOR_ARRAY) {
		pipe_ctx->color_pointer_state.enabled = 0;
	} else {
		pipe_ctx->last_error = GL_INVALID_ENUM;
	}
}

GL_API void GL_APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	if (mode < GL_POINTS || mode > GL_TRIANGLE_FAN) {
		pipe_ctx->last_error = GL_INVALID_ENUM;
		return;
	}
	if (count < 0) {
		pipe_ctx->last_error = GL_INVALID_VALUE;
		return;
	}

	struct pipe_vertex_array_state *vertex_state =
		&pipe_ctx->vertex_pointer_state;
	struct pipe_vertex_array_state *color_state =
		&pipe_ctx->color_pointer_state;

	if (!vertex_state->enabled) {
		return;
	}

	struct pipe_draw_info info;
	memset(&info, 0, sizeof(struct pipe_draw_info));

	info.mode = mode;
	info.start = first;
	info.count = count;

	pipe_ctx->draw_vbo(pipe_ctx, &info);
}

GL_API void GL_APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type,
				       const void *indices)
{
}

GL_API void GL_APIENTRY glEnableClientState(GLenum array)
{
	if (array == GL_VERTEX_ARRAY) {
		pipe_ctx->vertex_pointer_state.enabled = 1;
	} else if (array == GL_COLOR_ARRAY) {
		pipe_ctx->color_pointer_state.enabled = 1;
	} else {
		pipe_ctx->last_error = GL_INVALID_ENUM;
	}
}

GL_API void GL_APIENTRY glFlush(void)
{
	pipe_ctx->flush(pipe_ctx);
}

GL_API GLenum GL_APIENTRY glGetError(void)
{
	return pipe_ctx->last_error;
}

GL_API void GL_APIENTRY glVertexPointer(GLint size, GLenum type, GLsizei stride,
					const void *pointer)
{
	if (size != 3) {
		pipe_ctx->last_error = GL_INVALID_VALUE;
		return;
	}
	if (type != GL_FLOAT) {
		pipe_ctx->last_error = GL_INVALID_ENUM;
		return;
	}
	if (stride < 0) {
		pipe_ctx->last_error = GL_INVALID_VALUE;
		return;
	}

	struct pipe_vertex_array_state *state = &pipe_ctx->vertex_pointer_state;
	state->size = size;
	state->stride = stride;
	state->pointer = pointer;
}

GL_API void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width,
				   GLsizei height)
{
	if (width < 0 || height < 0) {
		pipe_ctx->last_error = GL_INVALID_VALUE;
		return;
	}

	struct pipe_viewport_state viewport;
	float half_width = 0.5f * width;
	float half_height = 0.5f * height;
	double n = 0;
	double f = 1;

	viewport.scale[0] = half_width;
	viewport.translate[0] = half_width + x;
	viewport.scale[1] = half_height;
	viewport.translate[1] = half_height + y;
	viewport.scale[2] = 0.5 * (f - n);
	viewport.translate[2] = 0.5 * (n + f);

	pipe_ctx->set_viewport_state(pipe_ctx, &viewport);
}
