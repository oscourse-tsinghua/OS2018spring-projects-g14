#include <unistd.h>
#include <assert.h>
#include <types.h>

#include "vc4_context.h"
#include "vc4_cl.h"
#include "vc4_drv.h"
#include "vc4_packet.h"

struct shaded_vertex {
	uint16_t x, y;
	float z, rhw;
	float r, g, b;
};

static void vc4_emit_state(struct vc4_context *vc4, uint32_t width,
			   uint32_t height)
{
	cl_u8(&vc4->bcl, VC4_PACKET_CLIP_WINDOW);
	cl_u16(&vc4->bcl, 0);
	cl_u16(&vc4->bcl, 0);
	cl_u16(&vc4->bcl, width); // width
	cl_u16(&vc4->bcl, height); // height

	cl_u8(&vc4->bcl, VC4_PACKET_CONFIGURATION_BITS);
	cl_u8(&vc4->bcl, 0x03); // enable both foward and back facing polygons
	cl_u8(&vc4->bcl, 0x00); // depth testing disabled
	cl_u8(&vc4->bcl, 0x02); // enable early depth write

	cl_u8(&vc4->bcl, VC4_PACKET_VIEWPORT_OFFSET);
	cl_u16(&vc4->bcl, 0);
	cl_u16(&vc4->bcl, 0);
}

static size_t emit_triangle(void *vaddr, float r, float g, float b, float x1,
			    float y1, float z1, float x2, float y2, float z2,
			    float x3, float y3, int z3)
{
	struct shaded_vertex verts[] = {
		{ ((int)x1) << 4, ((int)y1) << 4, z1, 1, r, g, b },
		{ ((int)x2) << 4, ((int)y2) << 4, z2, 1, r, g, b },
		{ ((int)x3) << 4, ((int)y3) << 4, z3, 1, r, g, b },
	};

	memcpy(vaddr, verts, sizeof(verts));

	return sizeof(verts);
}

static struct vc4_bo *get_vbo(struct vc4_context *vc4, uint32_t width,
			      uint32_t height)
{
	struct vc4_bo *bo;
	bo = vc4_bo_create(sizeof(struct shaded_vertex) * 12, 0x10);

	float sqrt3 = 1.7320508075688772f;
	float sqrt6 = 2.449489742783178;
	int size = 600;
	uint32_t center_x = width / 2, center_y = height / 2;
	float x0 = center_x - size / 2, y0 = center_y + sqrt3 / 6 * size,
	      z0 = 1;
	float x1 = x0 + size / 2, y1 = y0 - sqrt3 / 2 * size, z1 = z0;
	float x2 = x0, y2 = y0, z2 = z0;
	float x3 = x0 + size, y3 = y0, z3 = z0;
	float x4 = x0 + size / 2, y4 = y0 - sqrt3 / 6 * size,
	      z4 = z0; // + sqrt6 / 3 * size;

	uint32_t offset = 0;
	offset += emit_triangle(bo->vaddr + offset, 1, 1, 1, x1, y1 - 10, z1,
				x2 - 10, y2 + 10, z2, x3 + 10, y3 + 10, z3);
	offset += emit_triangle(bo->vaddr + offset, 1, 0, 0, x4, y4, z4, x2, y2,
				z2, x1, y1, z1);
	offset += emit_triangle(bo->vaddr + offset, 0, 0, 1, x4, y4, z4, x1, y1,
				z1, x3, y3, z3);
	offset += emit_triangle(bo->vaddr + offset, 0, 1, 0, x4, y4, z4, x2, y2,
				z2, x3, y3, z3);

	return bo;
}

static struct vc4_bo *get_ibo(struct vc4_context *vc4)
{
	static const uint8_t indices[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
	};
	struct vc4_bo *bo;
	bo = vc4_bo_create(sizeof(indices), 1);
	memcpy(bo->vaddr, indices, sizeof(indices));

	return bo;
}

static void vc4_draw_vbo(struct vc4_context *vc4)
{
	uint32_t width = vc4->framebuffer->var.xres;
	uint32_t height = vc4->framebuffer->var.yres;
	uint32_t tilew = (width + 63) / 64; // Tiles across
	uint32_t tileh = (height + 63) / 64; // Tiles down

	size_t tile_alloc_size = 0x8000;
	struct vc4_bo *tile_alloc = vc4_bo_create(tile_alloc_size, 0x1000);
	struct vc4_bo *tile_state = vc4_bo_create(48 * tilew * tileh, 0x10);
	struct vc4_bo *fs_uniform = vc4_bo_create(0x1000, 1);

	struct vc4_bo *ibo = get_ibo(vc4);
	struct vc4_bo *vbo = get_vbo(vc4, width, height);

	vc4->needs_flush = 1;

	//   Tile state data is 48 bytes per tile, I think it can be thrown away
	//   as soon as binning is finished.
	cl_u8(&vc4->bcl, VC4_PACKET_TILE_BINNING_MODE_CONFIG);
	cl_u32(&vc4->bcl, tile_alloc->paddr);
	cl_u32(&vc4->bcl, tile_alloc_size); /* tile allocation memory size */
	cl_u32(&vc4->bcl, tile_state->paddr); // 16 byte aligned
	cl_u8(&vc4->bcl, tilew);
	cl_u8(&vc4->bcl, tileh);
	cl_u8(&vc4->bcl, VC4_BIN_CONFIG_AUTO_INIT_TSDA);

	cl_u8(&vc4->bcl, VC4_PACKET_START_TILE_BINNING);

	cl_u8(&vc4->bcl, VC4_PACKET_PRIMITIVE_LIST_FORMAT);
	cl_u8(&vc4->bcl, VC4_PRIMITIVE_LIST_FORMAT_32_XY |
				 VC4_PRIMITIVE_LIST_FORMAT_TYPE_TRIANGLES);

	vc4_emit_state(vc4, width, height);

	cl_u8(&vc4->bcl, VC4_PACKET_NV_SHADER_STATE);
	cl_u32(&vc4->bcl, vc4->shader_rec.paddr); // 16 byte aligned

	cl_u8(&vc4->bcl, VC4_PACKET_GL_INDEXED_PRIMITIVE);
	cl_u8(&vc4->bcl, 0x04); // 8bit index, trinagles
	cl_u32(&vc4->bcl, 12); // Length
	cl_u32(&vc4->bcl, ibo->paddr);
	cl_u32(&vc4->bcl, 16); // Maximum index

	// Shader Record
	cl_u8(&vc4->shader_rec, 0);
	cl_u8(&vc4->shader_rec, sizeof(struct shaded_vertex)); // stride
	cl_u8(&vc4->shader_rec, 0xcc); // num uniforms (not used)
	cl_u8(&vc4->shader_rec, 3); // num varyings
	cl_u32(&vc4->shader_rec, vc4->prog.fs->paddr);
	cl_u32(&vc4->shader_rec, fs_uniform->paddr);
	cl_u32(&vc4->shader_rec, vbo->paddr); // 128-bit aligned

	vc4->shader_rec_count++;

	vc4_flush(vc4);
}

void vc4_hello_triangle(struct device *dev)
{
	struct vc4_context *vc4 = vc4_context_create(dev);

	vc4_draw_vbo(vc4);
}
