#include <fb.h>
#include <file.h>
#include <error.h>
#include <unistd.h>
#include <malloc.h>

#include "gl_context.h"
#include "pipe/p_context.h"

struct gl_context *context = NULL;

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
		clear_state->buffers = 0;
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
		struct pipe_framebuffer_state *framebuffer = &ctx->framebuffer;
		struct pipe_viewport_state *viewport = &ctx->viewport;
		float half_width = 0.5f * framebuffer->width;
		float half_height = 0.5f * framebuffer->height;
		float n = 0;
		float f = 1;

		viewport->scale[0] = half_width;
		viewport->translate[0] = half_width;
		viewport->scale[1] = half_height;
		viewport->translate[1] = half_height;
		viewport->scale[2] = 0.5 * (f - n);
		viewport->translate[2] = 0.5 * (n + f);

		ctx->pipe->set_viewport_state(ctx->pipe, viewport);
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

void gl_record_error(struct gl_context *ctx, GLenum error)
{
	ctx->last_error = error;
}
