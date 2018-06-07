#include <fb.h>
#include <file.h>
#include <error.h>
#include <malloc.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include "gl_context.h"
#include "pipe/p_context.h"

static int fb_fd = 0;
static int gpu_fd = 0;
static struct fb_var_screeninfo var;

// XXX deprecated
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

	struct gl_context *context = gl_context_create(gpu_fd);
	struct pipe_framebuffer_state *framebuffer;
	framebuffer = (struct pipe_framebuffer_state *)dpy;

	if (context == NULL) {
		return EGL_NO_CONTEXT;
	}

	// XXX
	context->framebuffer = *framebuffer;
	context->pipe->set_framebuffer_state(context->pipe, framebuffer);
	if ((ret = context->pipe->create_fs_state(context->pipe))) {
		return EGL_NO_CONTEXT;
	}

	gl_default_state(context);

	return context;
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLContext ctx)
{
	if (dpy == EGL_NO_DISPLAY || ctx == EGL_NO_CONTEXT) {
		return 0;
	}

	extern struct gl_context *context;
	context = (struct gl_context *)ctx;

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

	struct gl_context *context = (struct gl_context *)ctx;
	gl_context_destroy(context);

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
	struct gl_context *context = (struct gl_context *)ctx;
	struct pipe_framebuffer_state *framebuffer = &context->framebuffer;
	int ret = 0;

	uint32_t offset = framebuffer->offset;
	offset = var.xres * var.yres * (var.bits_per_pixel >> 3) - offset;

	var.yoffset = offset;
	if ((ret = ioctl(fb_fd, FBIOPAN_DISPLAY, &var))) {
		return 0;
	}

	framebuffer->offset = offset;
	context->pipe->set_framebuffer_state(context->pipe, framebuffer);

	return 1;
}
