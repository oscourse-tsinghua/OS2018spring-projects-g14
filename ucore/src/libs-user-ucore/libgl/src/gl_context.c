#include <fb.h>
#include <file.h>
#include <error.h>
#include <unistd.h>
#include <malloc.h>

#include "gl_context.h"
#include "pipe/p_context.h"

/** Clamp X to [MIN,MAX] */
#define CLAMP(X, MIN, MAX) ((X) < (MIN) ? (MIN) : ((X) > (MAX) ? (MAX) : (X)))

struct gl_context *context = NULL;

static void set_viewport_no_notify(struct gl_context *ctx, GLint x, GLint y,
				   GLsizei width, GLsizei height)
{
	struct pipe_viewport_state *viewport = &ctx->viewport;
	struct pipe_framebuffer_state *framebuffer = &ctx->framebuffer;
	x = CLAMP(x, 0, framebuffer->width);
	y = CLAMP(y, 0, framebuffer->height);
	if (width > framebuffer->width)
		width = framebuffer->width;
	if (height > framebuffer->height)
		height = framebuffer->height;

	float half_width = 0.5f * width;
	float half_height = 0.5f * height;
	viewport->scale[0] = half_width;
	viewport->translate[0] = half_width + x;
	viewport->scale[1] = half_height;
	viewport->translate[1] = half_height + y;
}

static void set_depth_range_no_notify(struct gl_context *ctx, GLfloat n,
				      GLfloat f)
{
	struct pipe_viewport_state *viewport = &ctx->viewport;
	f = CLAMP(f, 0.0, 1.0);
	n = CLAMP(n, 0.0, 1.0);

	viewport->scale[2] = 0.5 * (f - n);
	viewport->translate[2] = 0.5 * (n + f);
}

inline struct gl_context *gl_current_context(void)
{
	return context;
}

struct gl_context *gl_context_create(void)
{
	struct gl_context *ctx;

	ctx = (struct gl_context *)malloc(sizeof(struct gl_context));
	if (ctx == NULL)
		return NULL;

	memset(ctx, 0, sizeof(struct gl_context));

	int fd = open("gpu0:", O_RDWR);
	if (fd == 0)
		return NULL;

	ctx->screen.gpu_fd = fd;
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

	if (ctx->screen.gpu_fd) {
		close(ctx->screen.gpu_fd);
	}
	if (ctx->screen.fb_fd) {
		close(ctx->screen.fb_fd);
	}

	free(ctx);
}

int gl_create_window(struct gl_context *ctx)
{
	struct fb_var_screeninfo *var = &ctx->screen.var;
	struct pipe_framebuffer_state *framebuffer = &ctx->framebuffer;

	int ret = 0;
	int fd = open("fb0:", O_RDWR);
	if (fd == 0) {
		return -E_NODEV;
	}

	if ((ret = ioctl(fd, FBIOGET_VSCREENINFO, var))) {
		cprintf("GLES: fb ioctl error 0x%x\n", FBIOGET_VSCREENINFO);
		goto out;
	}

	uint32_t size = var->xres_virtual * var->yres_virtual *
			(var->bits_per_pixel >> 3);

	void *buf = (void *)sys_linux_mmap(0, size, fd, 0);
	if (buf == NULL) {
		cprintf("GLES: fb mmap error\n");
		ret = -E_NOMEM;
		goto out;
	}

	ctx->screen.fb_fd = fd;
	ctx->screen.screen_size = size;
	ctx->screen.double_buffer_offset =
		var->xres * var->yres * (var->bits_per_pixel >> 3);

	framebuffer->width = var->xres;
	framebuffer->height = var->yres;
	framebuffer->bits_per_pixel = var->bits_per_pixel;
	framebuffer->offset = 0;
	framebuffer->screen_base = buf;

	ctx->pipe->set_framebuffer_state(ctx->pipe, framebuffer);

out:
	return ret;
}

int gl_default_state(struct gl_context *ctx)
{
	int ret = 0;

	// current_color
	{
		gl_current_color(ctx, 1.0f, 1.0f, 1.0f, 1.0f);
	}

	// clear_state
	{
		struct pipe_clear_state *clear_state = &ctx->clear_state;
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
		set_viewport_no_notify(ctx, 0, 0, ctx->framebuffer.width,
				       ctx->framebuffer.height);
		set_depth_range_no_notify(ctx, 0, 1);
		ctx->pipe->set_viewport_state(ctx->pipe, &ctx->viewport);
	}

	// fs
	{
		if ((ret = ctx->pipe->create_fs_state(ctx->pipe))) {
			goto out;
		}
	}

out:
	return ret;
}

int gl_swap_buffers(struct gl_context *ctx)
{
	struct fb_var_screeninfo *var = &ctx->screen.var;
	struct pipe_framebuffer_state *framebuffer = &ctx->framebuffer;
	int ret = 0;

	var->yoffset = framebuffer->offset;
	if ((ret = ioctl(ctx->screen.fb_fd, FBIOPAN_DISPLAY, var))) {
		cprintf("GLES: fb ioctl error 0x%x\n", FBIOPAN_DISPLAY);
		return ret;
	}

	framebuffer->offset =
		ctx->screen.double_buffer_offset - framebuffer->offset;
	ctx->pipe->set_framebuffer_state(ctx->pipe, framebuffer);

	return ret;
}

void gl_current_color(struct gl_context *ctx, GLfloat red, GLfloat green,
		      GLfloat blue, GLfloat alpha)
{
	ctx->current_color =
		(union pipe_color_union){ red, green, blue, alpha };
}

void gl_error(struct gl_context *ctx, GLenum error)
{
	if (ctx && ctx->last_error == GL_NO_ERROR) {
		ctx->last_error = error;
	}
}

GLenum gl_get_error(struct gl_context *ctx)
{
	GLenum e = ctx->last_error;
	ctx->last_error = GL_NO_ERROR;
	return e;
}

void gl_clear(struct gl_context *ctx, GLbitfield mask)
{
	struct pipe_clear_state *clear_state = &ctx->clear_state;

	ctx->pipe->clear(ctx->pipe, mask, &clear_state->color,
			 clear_state->depth, clear_state->stencil);
}

void gl_flush(struct gl_context *ctx)
{
	ctx->pipe->flush(ctx->pipe);
}

void gl_set_enable(struct gl_context *ctx, GLenum cap, GLboolean state)
{
	switch (cap) {
	case GL_DEPTH_TEST:
		ctx->depth_stencil.depth.enabled = state;
		gl_depth_stencil_alpha(ctx);
		break;
	default:
		gl_error(ctx, GL_INVALID_ENUM);
	}
}

void gl_client_state(struct gl_context *ctx, GLenum array, GLboolean state)
{
	switch (array) {
	case GL_VERTEX_ARRAY:
		ctx->vertex_pointer_state.enabled = state;
		break;
	case GL_COLOR_ARRAY:
		ctx->color_pointer_state.enabled = state;
		break;
	default:
		gl_error(ctx, GL_INVALID_ENUM);
	}
}

void gl_pointer_array(struct gl_context *ctx,
		      struct pipe_vertex_array_state *state, GLint sizeMin,
		      GLint sizeMax, GLint size, GLenum type, GLsizei stride,
		      const void *pointer)
{
	if (size < sizeMin || size > sizeMax) {
		gl_error(ctx, GL_INVALID_VALUE);
		return;
	}
	if (type != GL_FLOAT) {
		gl_error(ctx, GL_INVALID_ENUM);
		return;
	}
	if (stride < 0) {
		gl_error(ctx, GL_INVALID_VALUE);
		return;
	}

	state->size = size;
	state->stride = stride;
	state->pointer = pointer;
}

void gl_depth_range(struct gl_context *ctx, GLfloat n, GLfloat f)
{
	set_depth_range_no_notify(ctx, n, f);
	ctx->pipe->set_viewport_state(ctx->pipe, &ctx->viewport);
}

void gl_viewport(struct gl_context *ctx, GLint x, GLint y, GLsizei width,
		 GLsizei height)
{
	set_viewport_no_notify(ctx, x, y, width, height);
	ctx->pipe->set_viewport_state(ctx->pipe, &ctx->viewport);
}

void gl_depth_stencil_alpha(struct gl_context *ctx)
{
	ctx->pipe->set_depth_stencil_alpha_state(ctx->pipe,
						 &ctx->depth_stencil);
}

void gl_draw_arrays(struct gl_context *ctx, GLenum mode, GLint first,
		    GLsizei count)
{
	struct pipe_vertex_array_state *vertex_state =
		&ctx->vertex_pointer_state;
	struct pipe_vertex_array_state *color_state = &ctx->color_pointer_state;

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

	rsc = ctx->pipe->resource_create(ctx->pipe, stride, count);
	if (rsc == NULL) {
		gl_error(ctx, GL_OUT_OF_MEMORY);
		return;
	}

	buffer = (struct nv_shaded_vertex *)ctx->pipe->resource_transfer_map(
		ctx->pipe, rsc);

	int i;
	const float *pointer =
		(float *)vertex_state->pointer + first * vertex_stride;
	const float *scale = ctx->viewport.scale;
	const float *translate = ctx->viewport.translate;
	const float *color = ctx->current_color.f;
	if (color_state->enabled) {
		color_stride = color_state->stride ?
				       color_state->stride / sizeof(float) :
				       color_state->size;
		color = (float *)color_state->pointer + first * color_stride;
	}

	for (i = 0; i < count; i++, pointer += vertex_stride) {
		buffer[i] = (struct nv_shaded_vertex){
			.x = (int16_t)((pointer[0] * scale[0]) * 16),
			.y = (int16_t)((pointer[1] * -scale[1]) * 16),
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

	ctx->pipe->set_vertex_buffers(ctx->pipe, &vb);
	ctx->pipe->draw_vbo(ctx->pipe, &info);
	ctx->pipe->resource_destroy(ctx->pipe, rsc);
}
