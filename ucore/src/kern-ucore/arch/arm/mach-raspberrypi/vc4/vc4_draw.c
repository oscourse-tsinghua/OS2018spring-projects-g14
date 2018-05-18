#include <unistd.h>
#include <assert.h>
#include <types.h>

#include "vc4_context.h"
#include "vc4_cl.h"
#include "vc4_bufmgr.h"
#include "vc4_packet.h"

struct shaded_vertex {
	uint16_t x, y;
	float z, rhw;
	float r, g, b;
};

static void vc4_get_draw_cl_space(struct vc4_context *vc4, int vert_count)
{
	int num_draws = 1;

	cl_ensure_space(&vc4->bcl, 256 + (VC4_PACKET_GL_INDEXED_PRIMITIVE_SIZE +
					  VC4_PACKET_NV_SHADER_STATE_SIZE) *
						   num_draws);

	cl_ensure_space(&vc4->shader_rec, 16 * num_draws);

	cl_ensure_space(&vc4->bo_handles, 20 * sizeof(uint32_t));
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

static struct vc4_bo *get_vbo(struct vc4_context *vc4)
{
	struct vc4_bo *bo;
	void *map;
	bo = vc4_bo_alloc(sizeof(struct shaded_vertex) * 12, 0x10);
	map = vc4_bo_map(bo);

	float sqrt3 = 1.7320508075688772f;
	float sqrt6 = 2.449489742783178;
	int size = 600;
	uint32_t center_x = (vc4->draw_max_x + vc4->draw_min_x) / 2;
	uint32_t center_y = (vc4->draw_max_y + vc4->draw_min_y) / 2;
	float x0 = center_x - size / 2, y0 = center_y + sqrt3 / 6 * size,
	      z0 = 1;
	float x1 = x0 + size / 2, y1 = y0 - sqrt3 / 2 * size, z1 = z0;
	float x2 = x0, y2 = y0, z2 = z0;
	float x3 = x0 + size, y3 = y0, z3 = z0;
	float x4 = x0 + size / 2, y4 = y0 - sqrt3 / 6 * size,
	      z4 = z0; // + sqrt6 / 3 * size;

	uint32_t offset = 0;
	offset += emit_triangle(map + offset, 1, 1, 1, x1, y1 - 10, z1, x2 - 10,
				y2 + 10, z2, x3 + 10, y3 + 10, z3);
	offset += emit_triangle(map + offset, 1, 0, 0, x4, y4, z4, x2, y2, z2,
				x1, y1, z1);
	offset += emit_triangle(map + offset, 0, 0, 1, x4, y4, z4, x1, y1, z1,
				x3, y3, z3);
	offset += emit_triangle(map + offset, 0, 1, 0, x4, y4, z4, x2, y2, z2,
				x3, y3, z3);

	return bo;
}

static struct vc4_bo *get_ibo(struct vc4_context *vc4)
{
	static const uint8_t indices[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
	};
	struct vc4_bo *bo;
	void *map;

	bo = vc4_bo_alloc(sizeof(indices), 1);
	map = vc4_bo_map(bo);
	memcpy(map, indices, sizeof(indices));

	return bo;
}

/**
 * Does the initial bining command list setup for drawing to a given FBO.
 */
static void vc4_start_draw(struct vc4_context *vc4)
{
	if (vc4->needs_flush)
		return;

	vc4_get_draw_cl_space(vc4, 0);

	cl_u8(&vc4->bcl, VC4_PACKET_TILE_BINNING_MODE_CONFIG);
	cl_u32(&vc4->bcl, 0);
	cl_u32(&vc4->bcl, 0);
	cl_u32(&vc4->bcl, 0);
	cl_u8(&vc4->bcl, vc4->draw_tiles_x);
	cl_u8(&vc4->bcl, vc4->draw_tiles_y);
	cl_u8(&vc4->bcl, VC4_BIN_CONFIG_AUTO_INIT_TSDA);

	/* START_TILE_BINNING resets the statechange counters in the hardware,
	 * which are what is used when a primitive is binned to a tile to
	 * figure out what new state packets need to be written to that tile's
	 * command list.
	 */
	cl_u8(&vc4->bcl, VC4_PACKET_START_TILE_BINNING);

	/* Reset the current compressed primitives format.  This gets modified
	 * by VC4_PACKET_GL_INDEXED_PRIMITIVE and
	 * VC4_PACKET_GL_ARRAY_PRIMITIVE, so it needs to be reset at the start
	 * of every tile.
	 */
	cl_u8(&vc4->bcl, VC4_PACKET_PRIMITIVE_LIST_FORMAT);
	cl_u8(&vc4->bcl, VC4_PRIMITIVE_LIST_FORMAT_16_INDEX |
				 VC4_PRIMITIVE_LIST_FORMAT_TYPE_TRIANGLES);

	vc4->needs_flush = 1;
	vc4->draw_width = vc4->framebuffer.width;
	vc4->draw_height = vc4->framebuffer.height;
}

static void vc4_init_context_fbo(struct vc4_context *vc4)
{
	vc4->tile_width = 64;
	vc4->tile_height = 64;

	vc4->draw_tiles_x =
		ROUNDUP_DIV(vc4->framebuffer.width, vc4->tile_width);
	vc4->draw_tiles_y =
		ROUNDUP_DIV(vc4->framebuffer.height, vc4->tile_height);
}

static void vc4_draw_vbo(struct vc4_context *vc4)
{
	vc4_init_context_fbo(vc4);

	vc4_start_draw(vc4);

	vc4_emit_state(vc4);

	struct vc4_bo *ibo = get_ibo(vc4);
	struct vc4_bo *vbo = get_vbo(vc4);

	cl_u8(&vc4->bcl, VC4_PACKET_NV_SHADER_STATE);
	cl_u32(&vc4->bcl, 0); // offset into shader_rec

	uint32_t mode = 4;
	uint32_t index_size = 1;
	cl_start_reloc(&vc4->bcl, 1);
	cl_u8(&vc4->bcl, VC4_PACKET_GL_INDEXED_PRIMITIVE);
	cl_u8(&vc4->bcl, mode | (index_size == 2 ? VC4_INDEX_BUFFER_U16 :
						   VC4_INDEX_BUFFER_U8));
	cl_u32(&vc4->bcl, 12); // Length
	cl_reloc(&vc4->bcl, vc4, ibo, 0);
	cl_u32(&vc4->bcl, 12); // Maximum index

	// Shader Record
	cl_start_shader_reloc(&vc4->shader_rec, 3);
	cl_u8(&vc4->shader_rec, 0);
	cl_u8(&vc4->shader_rec, sizeof(struct shaded_vertex)); // stride
	cl_u8(&vc4->shader_rec, 0xcc); // num uniforms (not used)
	cl_u8(&vc4->shader_rec, 3); // num varyings
	cl_reloc(&vc4->shader_rec, vc4, vc4->prog.fs, 0);
	cl_reloc(&vc4->shader_rec, vc4, vc4->uniforms, 0);
	cl_reloc(&vc4->shader_rec, vc4, vbo, 0);

	vc4->shader_rec_count++;

	vc4_flush(vc4);
}

static void vc4_clear(struct vc4_context *vc4, uint32_t color)
{
	vc4->clear_color[0] = vc4->clear_color[1] = color;
	vc4->clear_depth = 0;
	vc4->clear_stencil = 0;
	vc4->cleared |= 1;

	vc4_init_context_fbo(vc4);

	vc4->draw_min_x = 0;
	vc4->draw_min_y = 0;
	vc4->draw_max_x = vc4->framebuffer.width;
	vc4->draw_max_y = vc4->framebuffer.height;

	vc4_start_draw(vc4);
}

void vc4_hello_triangle(void)
{
	struct vc4_context *vc4 = vc4_context_create();

	vc4_clear(vc4, 0x282c34);
	vc4_draw_vbo(vc4);
}
