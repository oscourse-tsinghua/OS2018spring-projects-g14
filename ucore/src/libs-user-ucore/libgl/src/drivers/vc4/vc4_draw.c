#include <unistd.h>
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
	cl_ensure_space(&vc4->bo_pointers, 20 * sizeof(uintptr_t));
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
	bo = vc4_bo_alloc(vc4, sizeof(struct shaded_vertex) * 12);
	map = vc4_bo_map(vc4, bo);

	float sqrt3 = 1.7320508075688772f;
	float sqrt6 = 2.449489742783178;
	int size = 600;
	int center_x = 0;
	int center_y = 0;
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

	bo = vc4_bo_alloc(vc4, sizeof(indices));
	map = vc4_bo_map(vc4, bo);
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

static void vc4_emit_nv_shader_state(struct vc4_context *vc4)
{
	struct vc4_bo *vbo = get_vbo(vc4);

	cl_u8(&vc4->bcl, VC4_PACKET_NV_SHADER_STATE);
	cl_u32(&vc4->bcl, 0); // offset into shader_rec

	cl_start_shader_reloc(&vc4->shader_rec, 3);
	cl_u8(&vc4->shader_rec, 0);
	cl_u8(&vc4->shader_rec, sizeof(struct shaded_vertex)); // stride
	cl_u8(&vc4->shader_rec, 0xcc); // num uniforms (not used)
	cl_u8(&vc4->shader_rec, 3); // num varyings
	cl_reloc(&vc4->shader_rec, vc4, vc4->prog.fs, 0);
	cl_reloc(&vc4->shader_rec, vc4, vc4->uniforms, 0);
	cl_reloc(&vc4->shader_rec, vc4, vbo, 0);

	vc4->shader_rec_count++;
}

static void vc4_draw_vbo(struct pipe_context *pctx)
{
	struct vc4_context *vc4 = vc4_context(pctx);

	vc4_init_context_fbo(vc4);

	vc4_start_draw(vc4);

	vc4_emit_state(vc4);

	vc4_emit_nv_shader_state(vc4);

	vc4->dirty = 0;

	struct vc4_bo *ibo = get_ibo(vc4);

	uint32_t mode = 4;
	uint32_t index_size = 1;
	cl_start_reloc(&vc4->bcl, 1);
	cl_u8(&vc4->bcl, VC4_PACKET_GL_INDEXED_PRIMITIVE);
	cl_u8(&vc4->bcl, mode | (index_size == 2 ? VC4_INDEX_BUFFER_U16 :
						   VC4_INDEX_BUFFER_U8));
	cl_u32(&vc4->bcl, 12); // Length
	cl_reloc(&vc4->bcl, vc4, ibo, 0);
	cl_u32(&vc4->bcl, 12); // Maximum index
}

static uint8_t float_to_ubyte(float f)
{
	union fi {
		float f;
		int32_t i;
		uint32_t ui;
	} tmp;

	tmp.f = f;
	if (tmp.i < 0) {
		return (uint8_t)0;
	} else if (tmp.i >= 0x3f800000 /* 1.0f */) {
		return (uint8_t)255;
	} else {
		tmp.f = tmp.f * (255.0f / 256.0f) + 32768.0f;
		return (uint8_t)tmp.i;
	}
}

static uint32_t pack_rgba(const float *rgba)
{
	uint8_t r = float_to_ubyte(rgba[0]);
	uint8_t g = float_to_ubyte(rgba[1]);
	uint8_t b = float_to_ubyte(rgba[2]);
	uint8_t a = float_to_ubyte(rgba[3]);
	return (a << 24) | (b << 16) | (g << 8) | r;
}

static void vc4_clear(struct pipe_context *pctx, unsigned buffers,
		      const union pipe_color_union *color, double depth,
		      unsigned stencil)
{
	struct vc4_context *vc4 = vc4_context(pctx);

	if (buffers & PIPE_CLEAR_COLOR0) {
		uint32_t clear_color;
		clear_color = pack_rgba(color->f);
		vc4->clear_color[0] = vc4->clear_color[1] = clear_color;
	}

	if (buffers & PIPE_CLEAR_DEPTHSTENCIL) {
		if (buffers & PIPE_CLEAR_DEPTH)
			vc4->clear_depth =
				depth == 1.0 ?
					0xffffff :
					(uint32_t)(depth * 0xffffff + 0.5f);
		if (buffers & PIPE_CLEAR_STENCIL)
			vc4->clear_stencil = stencil;
	}

	vc4_init_context_fbo(vc4);

	vc4->draw_min_x = 0;
	vc4->draw_min_y = 0;
	vc4->draw_max_x = vc4->framebuffer.width;
	vc4->draw_max_y = vc4->framebuffer.height;
	vc4->cleared |= buffers;

	vc4_start_draw(vc4);
}

void vc4_draw_init(struct pipe_context *pctx)
{
	pctx->draw_vbo = vc4_draw_vbo;
	pctx->clear = vc4_clear;
}
