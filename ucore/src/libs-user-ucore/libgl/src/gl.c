#include <GLES/gl.h>

#include "pipe/p_state.h"
#include "pipe/p_defines.h"
#include "pipe/p_context.h"

struct pipe_context *pipe_ctx;
static GLenum last_error = GL_NO_ERROR;

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
		last_error = GL_INVALID_VALUE;
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

GL_API void GL_APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type,
				       const void *indices)
{
}

GL_API void GL_APIENTRY glFlush(void)
{
	pipe_ctx->flush(pipe_ctx);
}

GL_API GLenum GL_APIENTRY glGetError(void)
{
	return last_error;
}

GL_API void GL_APIENTRY glVertexPointer(GLint size, GLenum type, GLsizei stride,
					const void *pointer)
{
}

GL_API void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width,
				   GLsizei height)
{
	if (width < 0 || height < 0) {
		last_error = GL_INVALID_VALUE;
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

void glDrawTriangle(void)
{
	if (pipe_ctx == NULL) {
		return;
	}

	pipe_ctx->draw_vbo(pipe_ctx);
}
