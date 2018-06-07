#include <fb.h>
#include <file.h>
#include <error.h>
#include <malloc.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include "pipe/p_state.h"
#include "pipe/p_context.h"

static int fb_fd = 0;
static int gpu_fd = 0;
static struct fb_var_screeninfo var;

extern struct pipe_context *pipe_ctx;

static void initContext(EGLDisplay dpy, EGLContext ctx)
{
	struct pipe_context *pctx = (struct pipe_context *)ctx;
	struct pipe_framebuffer_state *framebuffer =
		(struct pipe_framebuffer_state *)dpy;

	struct pipe_clear_state *clear_state = &pctx->clear_state;
	clear_state->buffers = 0;
	clear_state->depth = 1.0;
	clear_state->stencil = 0;
	memset(clear_state->color, 0, sizeof(union pipe_color_union));

	pctx->current_color =
		(union pipe_color_union){ 1.0f, 1.0f, 1.0f, 1.0f };

	pctx->depth_stencil.depth.enabled = 0;
	pctx->depth_stencil.depth.writemask = 1;
	pctx->depth_stencil.depth.func = PIPE_FUNC_LESS;
	pctx->set_depth_stencil_alpha_state(pctx, &pctx->depth_stencil);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
		GL_STENCIL_BUFFER_BIT);
	glViewport(0, 0, framebuffer->width, framebuffer->height);
}

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id)
{
	struct pipe_framebuffer_state *framebuffer;

	fb_fd = open("fb0:", O_RDWR);
	int ret = 0;

	if (fb_fd == 0) {
		return EGL_NO_DISPLAY;
	}

	if ((ret = ioctl(fb_fd, FBIOGET_VSCREENINFO, &var))) {
		cprintf("GLES: fb ioctl error\n");
		goto out;
	}

	uint32_t size =
		var.xres_virtual * var.yres_virtual * var.bits_per_pixel >> 3;

	void *buf = (void *)sys_linux_mmap(0, size, fb_fd, 0);
	if (buf == NULL) {
		cprintf("GLES: fb mmap error\n");
		ret = -E_NOMEM;
		goto out;
	}

	framebuffer = malloc(sizeof(struct pipe_framebuffer_state));
	if (framebuffer == NULL) {
		return EGL_NO_DISPLAY;
	}

	framebuffer->width = var.xres;
	framebuffer->height = var.yres;
	framebuffer->bits_per_pixel = var.bits_per_pixel;
	framebuffer->offset = 0;
	framebuffer->screen_base = buf;

out:
	return ret ? EGL_NO_DISPLAY : framebuffer;
}

EGLContext eglCreateContext(EGLDisplay dpy)
{
	gpu_fd = open("gpu0:", O_RDWR);
	int ret = 0;

	if (!gpu_fd || dpy == EGL_NO_DISPLAY) {
		return EGL_NO_CONTEXT;
	}

	struct pipe_context *pctx = pipe_context_create(gpu_fd);
	struct pipe_framebuffer_state *framebuffer;
	framebuffer = (struct pipe_framebuffer_state *)dpy;

	if (pctx == NULL) {
		return EGL_NO_CONTEXT;
	}

	pctx->set_framebuffer_state(pctx, framebuffer);
	if ((ret = pctx->create_fs_state(pctx))) {
		return EGL_NO_CONTEXT;
	}

	return pctx;
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLContext ctx)
{
	if (dpy == EGL_NO_DISPLAY || ctx == EGL_NO_CONTEXT) {
		return 0;
	}

	pipe_ctx = (struct pipe_context *)ctx;

	initContext(dpy, ctx);

	return 1;
}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
	if (dpy != EGL_NO_DISPLAY) {
		struct pipe_framebuffer_state *framebuffer;
		framebuffer = (struct pipe_framebuffer_state *)dpy;
		free(framebuffer);
	}

	if (ctx == EGL_NO_CONTEXT) {
		return 0;
	}

	struct pipe_context *pctx = (struct pipe_context *)ctx;
	pipe_ctx->destroy(pipe_ctx);

	if (gpu_fd) {
		close(gpu_fd);
	}
	if (fb_fd) {
		close(fb_fd);
	}

	return 1;
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLContext ctx)
{
	struct pipe_context *pctx = (struct pipe_context *)ctx;
	struct pipe_framebuffer_state *framebuffer;
	framebuffer = (struct pipe_framebuffer_state *)dpy;
	int ret = 0;

	uint32_t offset = framebuffer->offset;
	offset = var.xres * var.yres * (var.bits_per_pixel >> 3) - offset;

	var.yoffset = offset;
	if ((ret = ioctl(fb_fd, FBIOPAN_DISPLAY, &var))) {
		return 0;
	}

	framebuffer->offset = offset;
	pctx->set_framebuffer_state(pctx, framebuffer);

	return 1;
}
