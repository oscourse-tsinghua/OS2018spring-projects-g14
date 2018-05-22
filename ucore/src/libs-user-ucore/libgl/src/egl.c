#include <fb.h>
#include <file.h>
#include <error.h>
#include <malloc.h>
#include <unistd.h>

#include <EGL/egl.h>

#include "pipe/p_state.h"
#include "pipe/p_context.h"

static int gpu_fd = 0;

extern struct pipe_context *pipe_ctx;

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id)
{
	struct fb_var_screeninfo var;
	struct pipe_framebuffer_state *framebuffer;

	int fb_fd = open("fb0:", O_RDWR);
	int ret = 0;

	if (fb_fd == 0) {
		return EGL_NO_DISPLAY;
	}

	if ((ret = ioctl(fb_fd, FBIOGET_VSCREENINFO, &var))) {
		cprintf("GLES: fb ioctl error\n");
		goto out;
	}

	uint32_t size = var.xres * var.yres * var.bits_per_pixel >> 3;

	void *buf = (void *)sys_linux_mmap(0, size, fb_fd, 0);
	if (buf == EGL_NO_DISPLAY) {
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
	framebuffer->screen_base = buf;

out:
	close(fb_fd);
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

	return 1;
}
