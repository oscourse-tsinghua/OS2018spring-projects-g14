#include <malloc.h>

#include "vc4_drm.h"
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
			char *ptr = fb->screen_base + addr;
			int k;
			for (k = bytes_per_pixel - 1; k >= 0; k--)
				cprintf("%02x", *(ptr + k));
			cprintf(" ");
		}
		cprintf("\n");
	}
}

static void vc4_clear_context(struct vc4_context *vc4)
{
	int i;
	struct vc4_bo **referenced_bos = vc4->bo_pointers.base;
	for (i = 0; i < cl_offset(&vc4->bo_handles) / 4; i++) {
		vc4_bo_unreference(referenced_bos[i]);
	}

	vc4_reset_cl(&vc4->bcl);
	vc4_reset_cl(&vc4->shader_rec);
	vc4_reset_cl(&vc4->bo_handles);
	vc4_reset_cl(&vc4->bo_pointers);

	vc4->shader_rec_count = 0;
	vc4->needs_flush = 0;
	vc4->cleared = 0;
	vc4->dirty = ~0;
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
		return;
	}

	struct drm_vc4_submit_cl submit;
	memset(&submit, 0, sizeof(submit));

	if (cl_offset(&vc4->bcl) > 0) {
		cl_ensure_space(&vc4->bcl, 8);
		cl_u8(&vc4->bcl, VC4_PACKET_INCREMENT_SEMAPHORE);
		cl_u8(&vc4->bcl, VC4_PACKET_FLUSH);
	}

	submit.color_write.bits =
		VC4_SET_FIELD(vc4->framebuffer.bits_per_pixel == 16 ?
				      VC4_RENDER_CONFIG_FORMAT_BGR565 :
				      VC4_RENDER_CONFIG_FORMAT_RGBA8888,
			      VC4_RENDER_CONFIG_FORMAT) |
		VC4_SET_FIELD(VC4_TILING_FORMAT_LINEAR,
			      VC4_RENDER_CONFIG_MEMORY_FORMAT);

	submit.bin_cl = (uintptr_t)vc4->bcl.base;
	submit.bin_cl_size = cl_offset(&vc4->bcl);
	submit.shader_rec = (uintptr_t)vc4->shader_rec.base;
	submit.shader_rec_size = cl_offset(&vc4->shader_rec);
	submit.shader_rec_count = vc4->shader_rec_count;
	submit.bo_handles = (uintptr_t)vc4->bo_handles.base;
	submit.bo_handle_count = cl_offset(&vc4->bo_handles) / 4;

	if (vc4->draw_min_x == ~0 || vc4->draw_min_y == ~0) {
		cprintf("GLES: draw_min_x or draw_min_y not set.\n");
		return;
	}
	submit.min_x_tile = vc4->draw_min_x / vc4->tile_width;
	submit.min_y_tile = vc4->draw_min_y / vc4->tile_height;
	submit.max_x_tile = (vc4->draw_max_x - 1) / vc4->tile_width;
	submit.max_y_tile = (vc4->draw_max_y - 1) / vc4->tile_height;
	submit.width = vc4->draw_width;
	submit.height = vc4->draw_height;

	if (vc4->cleared) {
		submit.flags |= VC4_SUBMIT_CL_USE_CLEAR_COLOR;
		submit.clear_color[0] = vc4->clear_color[0];
		submit.clear_color[1] = vc4->clear_color[1];
		submit.clear_z = vc4->clear_depth;
		submit.clear_s = vc4->clear_stencil;
	}

	// vc4_dump_cl(vc4->bcl.base, cl_offset(&vc4->bcl), 8, "bcl");
	// vc4_dump_cl(vc4->shader_rec.base, cl_offset(&vc4->shader_rec), 8,
	// 	    "shader_rec");
	// vc4_dump_cl(vc4->bo_handles.base, cl_offset(&vc4->bo_handles), 8,
	// 	    "bo_handles");

	int ret = ioctl(vc4->fd, DRM_IOCTL_VC4_SUBMIT_CL, &submit);
	if (ret) {
		cprintf("GLES: submit failed: %e.\n", ret);
	}

	vc4_clear_context(vc4);

	dump_fbo(vc4);
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

	vc4_init_cl(&vc4->bcl);
	vc4_init_cl(&vc4->shader_rec);
	vc4_init_cl(&vc4->bo_handles);
	vc4_init_cl(&vc4->bo_pointers);
	cl_ensure_space(&vc4->bo_pointers, 2 * sizeof(uintptr_t));

	vc4_draw_init(pctx);
	vc4_state_init(pctx);
	vc4_program_init(pctx);
	vc4_resouce_init(pctx);

	vc4->draw_min_x = ~0;
	vc4->draw_min_y = ~0;
	vc4->draw_max_x = 0;
	vc4->draw_max_y = 0;
	vc4->dirty = ~0;

	list_init(&vc4->bo_list);

	return pctx;
}

struct pipe_context *pipe_context_create(int fd)
{
	return vc4_context_create(fd);
}
