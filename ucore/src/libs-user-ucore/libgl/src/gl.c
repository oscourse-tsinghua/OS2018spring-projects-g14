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

GL_API void GL_APIENTRY glColor4f(GLfloat red, GLfloat green, GLfloat blue,
				  GLfloat alpha)
{
	pipe_ctx->current_color =
		(union pipe_color_union){ red, green, blue, alpha };
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
	if (size != 4) {
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

	struct pipe_vertex_array_state *state = &pipe_ctx->color_pointer_state;
	state->size = size;
	state->stride = stride;
	state->pointer = pointer;
}

GL_API void GL_APIENTRY glDisable(GLenum cap)
{
	switch (cap) {
	case GL_DEPTH_TEST:
		pipe_ctx->depth_stencil.depth.enabled = 0;
		pipe_ctx->set_depth_stencil_alpha_state(
			pipe_ctx, &pipe_ctx->depth_stencil);
		break;
	default:
		pipe_ctx->last_error = GL_INVALID_ENUM;
	}
}

GL_API void GL_APIENTRY glDisableClientState(GLenum array)
{
	switch (array) {
	case GL_VERTEX_ARRAY:
		pipe_ctx->vertex_pointer_state.enabled = 0;
		break;
	case GL_COLOR_ARRAY:
		pipe_ctx->color_pointer_state.enabled = 0;
		break;
	default:
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
	struct pipe_vertex_buffer vb;
	struct pipe_resource *rsc;
	struct nv_shaded_vertex *buffer;
	uint32_t stride = sizeof(struct nv_shaded_vertex);
	uint32_t vertex_stride = vertex_state->stride ?
					 vertex_state->stride / sizeof(float) :
					 vertex_state->size;
	uint32_t color_stride = 0;

	rsc = pipe_ctx->resource_create(pipe_ctx, stride, count);
	if (rsc == NULL) {
		pipe_ctx->last_error = GL_OUT_OF_MEMORY;
		return;
	}

	buffer = (struct nv_shaded_vertex *)pipe_ctx->resource_transfer_map(
		pipe_ctx, rsc);

	int i;
	const float *pointer =
		(float *)vertex_state->pointer + first * vertex_stride;
	const float *scale = pipe_ctx->viewport.scale;
	const float *translate = pipe_ctx->viewport.translate;
	const float *color = pipe_ctx->current_color.f;
	if (color_state->enabled) {
		color_stride = color_state->stride ?
				       color_state->stride / sizeof(float) :
				       color_state->size;
		color = (float *)color_state->pointer + first * color_stride;
	}

	for (i = 0; i < count; i++, pointer += vertex_stride) {
		buffer[i] = (struct nv_shaded_vertex){
			.x = (int16_t)((pointer[0] * scale[0]) * 16),
			.y = (int16_t)((pointer[1] * scale[1]) * 16),
			.z = 0.0,
			.rhw = 1.0,
			.r = color[0],
			.g = color[1],
			.b = color[2],
		};
		if (vertex_state->size == 3) {
			buffer[i].z = pointer[2] * scale[2] + translate[2];
		}
		color += color_stride;
	};

	memset(&info, 0, sizeof(struct pipe_draw_info));
	memset(&vb, 0, sizeof(struct pipe_vertex_buffer));

	info.mode = mode;
	info.start = 0;
	info.count = count;

	vb.stride = stride;
	vb.is_user_buffer = 1;
	vb.buffer_offset = 0;
	vb.buffer.resource = rsc;

	pipe_ctx->set_vertex_buffers(pipe_ctx, &vb);
	pipe_ctx->draw_vbo(pipe_ctx, &info);
	pipe_ctx->resource_destroy(pipe_ctx, rsc);
}

GL_API void GL_APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type,
				       const void *indices)
{
}

GL_API void GL_APIENTRY glEnable(GLenum cap)
{
	switch (cap) {
	case GL_DEPTH_TEST:
		pipe_ctx->depth_stencil.depth.enabled = 1;
		pipe_ctx->set_depth_stencil_alpha_state(
			pipe_ctx, &pipe_ctx->depth_stencil);
		break;
	default:
		pipe_ctx->last_error = GL_INVALID_ENUM;
	}
}

GL_API void GL_APIENTRY glEnableClientState(GLenum array)
{
	switch (array) {
	case GL_VERTEX_ARRAY:
		pipe_ctx->vertex_pointer_state.enabled = 1;
		break;
	case GL_COLOR_ARRAY:
		pipe_ctx->color_pointer_state.enabled = 1;
		break;
	default:
		pipe_ctx->last_error = GL_INVALID_ENUM;
	}
}

GL_API void GL_APIENTRY glFlush(void)
{
	pipe_ctx->flush(pipe_ctx);
}

GL_API GLenum GL_APIENTRY glGetError(void)
{
	if (!pipe_ctx) {
		return GL_NO_ERROR;
	} else {
		return pipe_ctx->last_error;
	}
}

GL_API void GL_APIENTRY glVertexPointer(GLint size, GLenum type, GLsizei stride,
					const void *pointer)
{
	if (size < 2 || size > 3) {
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

	struct pipe_viewport_state *viewport = &pipe_ctx->viewport;
	float half_width = 0.5f * width;
	float half_height = 0.5f * height;
	float n = 0;
	float f = 1;

	viewport->scale[0] = half_width;
	viewport->translate[0] = half_width + x;
	viewport->scale[1] = half_height;
	viewport->translate[1] = half_height + y;
	viewport->scale[2] = 0.5 * (f - n);
	viewport->translate[2] = 0.5 * (n + f);

	pipe_ctx->set_viewport_state(pipe_ctx, viewport);
}
