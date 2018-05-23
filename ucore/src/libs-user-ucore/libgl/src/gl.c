#include <GLES/gl.h>

#include "pipe/p_state.h"
#include "pipe/p_context.h"

struct pipe_context *pipe_ctx;

GL_API void GL_APIENTRY glClearColor(GLfloat red, GLfloat green, GLfloat blue,
				     GLfloat alpha)
{
}

GL_API void GL_APIENTRY glClear(GLbitfield mask)
{
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

GL_API void GL_APIENTRY glVertexPointer(GLint size, GLenum type, GLsizei stride,
					const void *pointer)
{
}

GL_API void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width,
				   GLsizei height)
{
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

	pipe_ctx->clear(pipe_ctx, 0x282c34);
	pipe_ctx->draw_vbo(pipe_ctx);
}
