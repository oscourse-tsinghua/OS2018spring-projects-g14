#include <error.h>
#include <assert.h>

#include "vc4_cl.h"
#include "vc4_drm.h"
#include "vc4_drv.h"
#include "vc4_packet.h"

static void vc4_tile_coordinates(struct vc4_cl *setup, uint32_t x, uint32_t y)
{
	cl_u8(setup, VC4_PACKET_TILE_COORDINATES);
	cl_u8(setup, x);
	cl_u8(setup, y);
}

static void emit_tile(struct vc4_exec_info *exec, struct vc4_cl *setup,
		      uint8_t x, uint8_t y, bool first, bool last)
{
	struct drm_vc4_submit_cl *args = exec->args;
	bool has_bin = args->bin_cl_size != 0;

	/* Clipping depends on tile coordinates having been
	 * emitted, so we always need one here.
	 */
	vc4_tile_coordinates(setup, x, y);

	/* Wait for the binner before jumping to the first
	 * tile's lists.
	 */
	if (first && has_bin)
		cl_u8(setup, VC4_PACKET_WAIT_ON_SEMAPHORE);

	if (has_bin) {
		cl_u8(setup, VC4_PACKET_BRANCH_TO_SUB_LIST);
		cl_u32(setup, (exec->tile_alloc_offset +
			       (y * exec->bin_tiles_x + x) * 32));
	}

	// if (setup->color_write)
	if (last)
		cl_u8(setup, VC4_PACKET_STORE_MS_TILE_BUFFER_AND_EOF);
	else
		cl_u8(setup, VC4_PACKET_STORE_MS_TILE_BUFFER);
}

static int vc4_create_rcl_bo(struct device *dev, struct vc4_exec_info *exec,
			     struct vc4_cl *setup)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct drm_vc4_submit_cl *args = exec->args;
	bool has_bin = args->bin_cl_size != 0;
	uint8_t min_x_tile = args->min_x_tile;
	uint8_t min_y_tile = args->min_y_tile;
	uint8_t max_x_tile = args->max_x_tile;
	uint8_t max_y_tile = args->max_y_tile;
	uint8_t xtiles = max_x_tile - min_x_tile + 1;
	uint8_t ytiles = max_y_tile - min_y_tile + 1;
	uint8_t x, y;
	uint32_t size, loop_body_size;

	size = VC4_PACKET_TILE_RENDERING_MODE_CONFIG_SIZE;
	loop_body_size = VC4_PACKET_TILE_COORDINATES_SIZE;

	if (args->flags & VC4_SUBMIT_CL_USE_CLEAR_COLOR) {
		size += VC4_PACKET_CLEAR_COLORS_SIZE +
			VC4_PACKET_TILE_COORDINATES_SIZE +
			VC4_PACKET_STORE_TILE_BUFFER_GENERAL_SIZE;
	}

	if (has_bin) {
		size += VC4_PACKET_WAIT_ON_SEMAPHORE_SIZE;
		loop_body_size += VC4_PACKET_BRANCH_TO_SUB_LIST_SIZE;
	}

	// if (setup->color_write)
	loop_body_size += VC4_PACKET_STORE_MS_TILE_BUFFER_SIZE;

	size += xtiles * ytiles * loop_body_size;

	struct vc4_bo *bo = vc4_bo_create(size, 1);
	if (bo == NULL) {
		return -E_NOMEM;
	}
	vc4_init_cl(setup, bo->paddr, bo->vaddr, size);

	/* The tile buffer gets cleared when the previous tile is stored.  If
	 * the clear values changed between frames, then the tile buffer has
	 * stale clear values in it, so we have to do a store in None mode (no
	 * writes) so that we trigger the tile buffer clear.
	 */
	if (args->flags & VC4_SUBMIT_CL_USE_CLEAR_COLOR) {
		cl_u8(setup, VC4_PACKET_CLEAR_COLORS);
		cl_u32(setup, args->clear_color[0]);
		cl_u32(setup, args->clear_color[1]);
		cl_u32(setup, args->clear_z);
		cl_u8(setup, args->clear_s);

		vc4_tile_coordinates(setup, 0, 0);

		cl_u8(setup, VC4_PACKET_STORE_TILE_BUFFER_GENERAL);
		cl_u16(setup, VC4_LOADSTORE_TILE_BUFFER_NONE);
		cl_u32(setup, 0); /* no address, since we're in None mode */
	}

	cl_u8(setup, VC4_PACKET_TILE_RENDERING_MODE_CONFIG);
	cl_u32(setup, vc4->fb->fb_bus_address);
	cl_u16(setup, args->width);
	cl_u16(setup, args->height);
	cl_u16(setup, args->color_write.bits);

	for (y = min_y_tile; y <= max_y_tile; y++) {
		for (x = min_x_tile; x <= max_x_tile; x++) {
			bool first = (x == min_x_tile && y == min_y_tile);
			bool last = (x == max_x_tile && y == max_y_tile);

			emit_tile(exec, setup, x, y, first, last);
		}
	}

	assert(cl_offset(setup) == size);
	exec->ct1ca = setup->paddr;
	exec->ct1ea = setup->paddr + cl_offset(setup);

	return 0;
}

int vc4_get_rcl(struct device *dev, struct vc4_exec_info *exec)
{
	struct vc4_cl setup;

	return vc4_create_rcl_bo(dev, exec, &setup);
}
