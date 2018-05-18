#include <assert.h>

#include "vc4_drv.h"
#include "vc4_drm.h"
#include "vc4_packet.h"
#include "vc4_context.h"

static void dump_fbo(struct vc4_context *vc4)
{
	struct fb_info *fb = vc4->framebuffer;
	uint32_t width = fb->var.xres;
	uint32_t height = fb->var.yres;

	uint32_t center_x = width / 2;
	uint32_t center_y = height / 2;

	int x, y;
	for (y = -6; y < 6; y++) {
		for (x = -6; x < 6; x++) {
			uint32_t bytes_per_pixel = fb->var.bits_per_pixel >> 3;
			uint32_t addr = (y + center_y) * fb->fix.line_length +
					(x + center_x) * bytes_per_pixel;
			char *ptr = fb->screen_base + addr;
			int k;
			for (k = bytes_per_pixel - 1; k >= 0; k--)
				kprintf("%02x", *(ptr + k));
			kprintf(" ");
		}
		kprintf("\n");
	}
}

void vc4_flush(struct vc4_context *vc4)
{
	struct drm_vc4_submit_cl submit;
	memset(&submit, 0, sizeof(submit));

	if (cl_offset(&vc4->bcl) > 0) {
		cl_u8(&vc4->bcl, VC4_PACKET_INCREMENT_SEMAPHORE);
		cl_u8(&vc4->bcl, VC4_PACKET_FLUSH);
	}

	submit.color_write.bits =
		VC4_SET_FIELD(vc4->framebuffer->var.bits_per_pixel == 16 ?
				      VC4_RENDER_CONFIG_FORMAT_BGR565 :
				      VC4_RENDER_CONFIG_FORMAT_RGBA8888,
			      VC4_RENDER_CONFIG_FORMAT) |
		VC4_SET_FIELD(VC4_TILING_FORMAT_LINEAR,
			      VC4_RENDER_CONFIG_MEMORY_FORMAT);

	submit.bin_cl = vc4->bcl.paddr;
	submit.bin_cl_size = cl_offset(&vc4->bcl);
	submit.shader_rec = vc4->shader_rec.paddr;
	submit.shader_rec_size = cl_offset(&vc4->shader_rec);
	submit.shader_rec_count = vc4->shader_rec_count;
	submit.bo_handles = (uint32_t)vc4->bo_handles.vaddr;
	submit.bo_handle_count = cl_offset(&vc4->bo_handles) / 4;

	const uint32_t tile_width = 64, tile_height = 64;
	uint32_t draw_width = vc4->framebuffer->var.xres;
	uint32_t draw_height = vc4->framebuffer->var.yres;
	uint32_t draw_min_x = 0, draw_max_x = draw_width;
	uint32_t draw_min_y = 0, draw_max_y = draw_height;
	submit.min_x_tile = draw_min_x / tile_width;
	submit.min_y_tile = draw_min_y / tile_height;
	submit.max_x_tile = (draw_max_x - 1) / tile_width;
	submit.max_y_tile = (draw_max_y - 1) / tile_height;
	submit.width = draw_width;
	submit.height = draw_height;

	uint32_t cleared = 1;
	uint32_t clear_color = 0x282c34;
	if (cleared) {
		submit.flags |= VC4_SUBMIT_CL_USE_CLEAR_COLOR;
		submit.clear_color[0] = clear_color;
		submit.clear_color[1] = clear_color;
		submit.clear_z = 0;
		submit.clear_s = 0;
	}

	// vc4_dump_cl(vc4->bcl.vaddr, cl_offset(&vc4->bcl), 8, "bcl");
	// vc4_dump_cl(vc4->shader_rec.vaddr, cl_offset(&vc4->shader_rec), 8,
	// 	    "shader_rec");
	// vc4_dump_cl(vc4->bo_handles.vaddr, cl_offset(&vc4->bo_handles), 8,
	// 	    "bo_handles");

	int ret = vc4_submit_cl_ioctl(current_dev, &submit);
	if (ret) {
		kprintf("vc4: submit failed: %e.\n", ret);
	}

	vc4_reset_cl(&vc4->bcl);
	vc4_reset_cl(&vc4->shader_rec);
	vc4_reset_cl(&vc4->bo_handles);
	vc4->shader_rec_count = 0;

	vc4->needs_flush = 0;

	dump_fbo(vc4);
}

struct vc4_context *vc4_context_create(struct device *dev)
{
#define BUFFER_SHADER_OFFSET 0x80

	struct vc4_context *vc4;

	vc4 = (struct vc4_context *)kmalloc(sizeof(struct vc4_context));
	if (vc4 == NULL)
		return NULL;

	memset(vc4, 0, sizeof(struct vc4_context));
	vc4->framebuffer = to_vc4_dev(dev)->fb;

	vc4_program_init(vc4);

	struct vc4_bo *bo = vc4_bo_create(dev, 0x8000, 0x1000);

	vc4_init_cl(&vc4->bcl, bo->paddr, bo->vaddr, 0);
	vc4_init_cl(&vc4->shader_rec, bo->paddr + BUFFER_SHADER_OFFSET,
		    bo->vaddr + BUFFER_SHADER_OFFSET, 0);

	uint32_t bo_handles_base = (uint32_t)kmalloc(0x1000);
	vc4_init_cl(&vc4->bo_handles, bo_handles_base, (void *)bo_handles_base,
		    0);

	return vc4;
}
