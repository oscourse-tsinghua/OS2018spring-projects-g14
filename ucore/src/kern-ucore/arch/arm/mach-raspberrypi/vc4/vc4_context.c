#include <assert.h>

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
		cl_ensure_space(&vc4->bcl, 8);
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

	submit.bin_cl = (uintptr_t)vc4->bcl.base;
	submit.bin_cl_size = cl_offset(&vc4->bcl);
	submit.shader_rec = (uintptr_t)vc4->shader_rec.base;
	submit.shader_rec_size = cl_offset(&vc4->shader_rec);
	submit.shader_rec_count = vc4->shader_rec_count;
	submit.bo_handles = (uintptr_t)vc4->bo_handles.base;
	submit.bo_handle_count = cl_offset(&vc4->bo_handles) / 4;

	submit.min_x_tile = vc4->draw_min_x / vc4->tile_width;
	submit.min_y_tile = vc4->draw_min_y / vc4->tile_height;
	submit.max_x_tile = (vc4->draw_max_x - 1) / vc4->tile_width;
	submit.max_y_tile = (vc4->draw_max_y - 1) / vc4->tile_height;
	submit.width = vc4->draw_width;
	submit.height = vc4->draw_height;

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

	vc4_init_cl(&vc4->bcl);
	vc4_init_cl(&vc4->shader_rec);
	vc4_init_cl(&vc4->bo_handles);

	return vc4;
}
