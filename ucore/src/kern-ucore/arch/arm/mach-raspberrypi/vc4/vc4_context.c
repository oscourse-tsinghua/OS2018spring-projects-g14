#include "vc4_drv.h"
#include "vc4_drm.h"
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

	submit.bin_cl = vc4->bcl.paddr;
	submit.bin_cl_size = cl_offset(&vc4->bcl);
	submit.render_cl = vc4->rcl.paddr;
	submit.render_cl_size = cl_offset(&vc4->rcl);
	submit.shader_rec = vc4->shader_rec.paddr;
	submit.shader_rec_size = cl_offset(&vc4->shader_rec);
	submit.shader_rec_count = vc4->shader_rec_count;

	// cl_dump(&vc4->bcl, 8, "bcl");
	// cl_dump(&vc4->shader_rec, 8, "shader_rec");
	// cl_dump(&vc4->rcl, 18, "rcl");

	int ret = vc4_submit_cl_ioctl(&submit);
	if (ret) {
		kprintf("vc4: submit failed: %e.\n", ret);
	}

	vc4_reset_cl(&vc4->bcl);
	vc4_reset_cl(&vc4->rcl);
	vc4_reset_cl(&vc4->shader_rec);
	vc4->shader_rec_count = 0;

	vc4->needs_flush = 0;

	dump_fbo(vc4);
}

struct vc4_context *vc4_context_create(struct fb_info *fb)
{
#define BUFFER_SHADER_OFFSET 0x80
#define BUFFER_RENDER_CONTROL 0x1000

	struct vc4_context *vc4;

	vc4 = (struct vc4_context *)kmalloc(sizeof(struct vc4_context));
	if (vc4 == NULL)
		return NULL;

	memset(vc4, 0, sizeof(struct vc4_context));
	vc4->framebuffer = fb;

	vc4_program_init(vc4);

	struct vc4_bo *bo = vc4_bo_create(0x8000, 0x1000);

	vc4_init_cl(&vc4->bcl, bo->paddr, bo->vaddr, 0);
	vc4_init_cl(&vc4->rcl, bo->paddr + BUFFER_RENDER_CONTROL,
		    bo->vaddr + BUFFER_RENDER_CONTROL, 0);
	vc4_init_cl(&vc4->shader_rec, bo->paddr + BUFFER_SHADER_OFFSET,
		    bo->vaddr + BUFFER_SHADER_OFFSET, 0);

	return vc4;
}
