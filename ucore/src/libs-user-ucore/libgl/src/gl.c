#include <file.h>
#include <unistd.h>

#include <GLES/gl.h>

#include "vc4/vc4_drm.h"
#include "vc4/vc4_context.h"

static int fd = 0;
static struct gl_context *ctx;

void glOpen(void)
{
	int fd = open("gpu0:", O_RDWR);
	ctx = vc4_context_create(fd);
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
