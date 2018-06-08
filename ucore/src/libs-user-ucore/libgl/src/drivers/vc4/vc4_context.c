#include <malloc.h>

#include "vc4_packet.h"
#include "vc4_context.h"

static void dump_fbo(struct vc4_context *vc4)
{
	struct pipe_framebuffer_state *fb = &vc4->framebuffer;
	uint32_t center_x = fb->width / 2;
	uint32_t center_y = fb->height / 2;

	int x, y;
	for (y = -6; y < 6; y++) {
		for (x = -6; x < 6; x++) {
			uint32_t bytes_per_pixel = fb->bits_per_pixel >> 3;
			uint32_t addr =
				((y + center_y) * fb->width + (x + center_x)) *
				bytes_per_pixel;
			char *ptr = fb->screen_base + fb->offset + addr;
			int k;
			for (k = bytes_per_pixel - 1; k >= 0; k--)
				cprintf("%02x", *(ptr + k));
			cprintf(" ");
		}
		cprintf("\n");
	}
}

void vc4_flush(struct pipe_context *pctx)
{
	struct vc4_context *vc4 = vc4_context(pctx);

	if (!vc4->needs_flush)
		return;

	/* The RCL setup would choke if the draw bounds cause no drawing, so
	 * just drop the drawing if that's the case.
	 */
	if (vc4->draw_max_x <= vc4->draw_min_x ||
	    vc4->draw_max_y <= vc4->draw_min_y) {
		vc4_job_reset(vc4);
		return;
	}

	if (cl_offset(&vc4->bcl) > 0) {
		/* Increment the semaphore indicating that binning is done and
		 * unblocking the render thread.  Note that this doesn't act
		 * until the FLUSH completes.
		 */
		cl_ensure_space(&vc4->bcl, 8);
		cl_u8(&vc4->bcl, VC4_PACKET_INCREMENT_SEMAPHORE);
		/* The FLUSH caps all of our bin lists with a
		 * VC4_PACKET_RETURN.
		 */
		cl_u8(&vc4->bcl, VC4_PACKET_FLUSH);
	}

	vc4_job_submit(vc4);

	// dump_fbo(vc4);
}

void vc4_context_destroy(struct pipe_context *pctx)
{
	struct vc4_context *vc4 = vc4_context(pctx);

	vc4_flush(pctx);

	while (!list_empty(&vc4->bo_list)) {
		list_entry_t *le = list_next(&vc4->bo_list);
		struct vc4_bo *bo = le2bo(le, bo_link);
		list_del(&bo->bo_link);
		vc4_bo_free(bo);
	}

	if (vc4->bcl.base) {
		free(vc4->bcl.base);
	}
	if (vc4->shader_rec.base) {
		free(vc4->shader_rec.base);
	}
	if (vc4->bo_handles.base) {
		free(vc4->bo_handles.base);
	}
	if (vc4->bo_pointers.base) {
		free(vc4->bo_pointers.base);
	}

	free(vc4);
}

static struct pipe_context *vc4_context_create(int fd)
{
	struct vc4_context *vc4;

	vc4 = (struct vc4_context *)malloc(sizeof(struct vc4_context));
	if (vc4 == NULL)
		return NULL;

	memset(vc4, 0, sizeof(struct vc4_context));

	struct pipe_context *pctx = &vc4->base;
	pctx->destroy = vc4_context_destroy;
	pctx->flush = vc4_flush;
	vc4->fd = fd;

	vc4_draw_init(pctx);
	vc4_state_init(pctx);
	vc4_program_init(pctx);
	vc4_resource_init(pctx);

	vc4_job_init(vc4);

	/* space for fs and uniforms */
	cl_ensure_space(&vc4->bo_pointers, 2 * sizeof(uintptr_t));

	list_init(&vc4->bo_list);

	return pctx;
}

struct pipe_context *pipe_context_create(int fd)
{
	return vc4_context_create(fd);
}
