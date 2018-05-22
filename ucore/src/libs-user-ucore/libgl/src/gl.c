#include <fb.h>
#include <file.h>
#include <error.h>
#include <unistd.h>

#include <GLES/gl.h>

#include "pipe/p_state.h"
#include "pipe/p_context.h"

static int fd = 0;
static struct pipe_context *ctx;

int eglCreateWindowSurface()
{
	struct fb_var_screeninfo var;
	struct pipe_framebuffer_state framebuffer;

	int fb_fd = open("fb0:", O_RDWR);
	int ret = 0;

	if (fb_fd == 0) {
		return -E_NOENT;
	}

	if ((ret = ioctl(fb_fd, FBIOGET_VSCREENINFO, &var))) {
		cprintf("GLES: fb ioctl error\n");
		goto out;
	}

	uint32_t size = var.xres * var.yres * var.bits_per_pixel >> 3;

	void *buf = (void *)sys_linux_mmap(0, size, fb_fd, 0);
	if (buf == NULL) {
		cprintf("GLES: fb mmap error\n");
		ret = -E_NOMEM;
		goto out;
	}

	framebuffer.width = var.xres;
	framebuffer.height = var.yres;
	framebuffer.bits_per_pixel = var.bits_per_pixel;
	framebuffer.screen_base = buf;

	ctx->set_framebuffer_state(ctx, &framebuffer);

out:
	close(fb_fd);
	return ret;
}

int glOpen(void)
{
	int fd = open("gpu0:", O_RDWR);
	int ret = 0;

	if (!fd) {
		return -E_NOENT;
	}

	ctx = pipe_context_create(fd);
	if (!ctx) {
		return -E_FAULT;
	}

	if ((ret = ctx->create_fs_state(ctx))) {
		return ret;
	}

	return ret;
}

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
}

GL_API void GL_APIENTRY glVertexPointer(GLint size, GLenum type, GLsizei stride,
					const void *pointer)
{
}

GL_API void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width,
				   GLsizei height)
{
	struct pipe_viewport_state viewport;
	viewport.x = x;
	viewport.y = y;
	viewport.width = width;
	viewport.height = height;
	ctx->set_viewport_state(ctx, &viewport);
}

void glDrawTriangle(void)
{
	if (ctx == NULL) {
		return;
	}

	ctx->clear(ctx, 0x282c34);
	ctx->draw_vbo(ctx);
}

void glClose(void)
{
	if (fd) {
		close(fd);
	}
	if (ctx == NULL) {
		return;
	}
	ctx->destroy(ctx);
}
