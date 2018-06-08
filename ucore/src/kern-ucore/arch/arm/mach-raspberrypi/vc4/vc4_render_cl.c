#include <error.h>
#include <assert.h>

#include "vc4_cl.h"
#include "vc4_drm.h"
#include "vc4_drv.h"
#include "vc4_packet.h"

struct vc4_rcl_setup {
	struct vc4_bo *color_read;
	struct vc4_bo *color_write;
	struct vc4_bo *zs_read;
	struct vc4_bo *zs_write;

	struct vc4_cl rcl;
	uint32_t next_offset;
};

/*
 * Emits a no-op STORE_TILE_BUFFER_GENERAL.
 *
 * If we emit a PACKET_TILE_COORDINATES, it must be followed by a store of
 * some sort before another load is triggered.
 */
static void vc4_store_before_load(struct vc4_cl *rcl)
{
	cl_u8(rcl, VC4_PACKET_STORE_TILE_BUFFER_GENERAL);
	cl_u16(rcl, VC4_SET_FIELD(VC4_LOADSTORE_TILE_BUFFER_NONE,
				  VC4_LOADSTORE_TILE_BUFFER_BUFFER) |
			    VC4_STORE_TILE_BUFFER_DISABLE_COLOR_CLEAR |
			    VC4_STORE_TILE_BUFFER_DISABLE_ZS_CLEAR |
			    VC4_STORE_TILE_BUFFER_DISABLE_VG_MASK_CLEAR);
	cl_u32(rcl, 0); /* no address, since we're in None mode */
}

/*
 * Emits a PACKET_TILE_COORDINATES if one isn't already pending.
 *
 * The tile coordinates packet triggers a pending load if there is one, are
 * used for clipping during rendering, and determine where loads/stores happen
 * relative to their base address.
 */
static void vc4_tile_coordinates(struct vc4_cl *rcl, uint32_t x, uint32_t y)
{
	cl_u8(rcl, VC4_PACKET_TILE_COORDINATES);
	cl_u8(rcl, x);
	cl_u8(rcl, y);
}

static void emit_tile(struct vc4_exec_info *exec, struct vc4_rcl_setup *setup,
		      uint8_t x, uint8_t y, bool first, bool last)
{
	struct drm_vc4_submit_cl *args = exec->args;
	struct vc4_cl *rcl = &setup->rcl;
	bool has_bin = args->bin_cl_size != 0;

	/* Note that the load doesn't actually occur until the
	 * tile coords packet is processed, and only one load
	 * may be outstanding at a time.
	 */
	if (setup->color_read) {
		cl_u8(rcl, VC4_PACKET_LOAD_TILE_BUFFER_GENERAL);
		cl_u16(rcl, args->color_read.bits);
		cl_u32(rcl, setup->color_read->paddr + args->color_read.offset);
	}

	if (setup->zs_read) {
		if (setup->color_read) {
			/* Exec previous load. */
			vc4_tile_coordinates(rcl, x, y);
			vc4_store_before_load(rcl);
		}

		cl_u8(rcl, VC4_PACKET_LOAD_TILE_BUFFER_GENERAL);
		cl_u16(rcl, args->zs_read.bits);
		cl_u32(rcl, setup->zs_read->paddr + args->zs_read.offset);
	}

	/* Clipping depends on tile coordinates having been
	 * emitted, so we always need one here.
	 */
	vc4_tile_coordinates(rcl, x, y);

	/* Wait for the binner before jumping to the first
	 * tile's lists.
	 */
	if (first && has_bin)
		cl_u8(rcl, VC4_PACKET_WAIT_ON_SEMAPHORE);

	if (has_bin) {
		cl_u8(rcl, VC4_PACKET_BRANCH_TO_SUB_LIST);
		cl_u32(rcl, (exec->tile_alloc_offset +
			     (y * exec->bin_tiles_x + x) * 32));
	}

	if (setup->zs_write) {
		bool last_tile_write = !setup->color_write;

		cl_u8(rcl, VC4_PACKET_STORE_TILE_BUFFER_GENERAL);
		cl_u16(rcl,
		       args->zs_write.bits |
			       (last_tile_write ?
					0 :
					VC4_STORE_TILE_BUFFER_DISABLE_COLOR_CLEAR));
		cl_u32(rcl, (setup->zs_write->paddr + args->zs_write.offset) |
				    ((last && last_tile_write) ?
					     VC4_LOADSTORE_TILE_BUFFER_EOF :
					     0));
	}

	if (setup->color_write) {
		if (setup->zs_write) {
			/* Reset after previous store */
			vc4_tile_coordinates(rcl, x, y);
		}

		if (last)
			cl_u8(rcl, VC4_PACKET_STORE_MS_TILE_BUFFER_AND_EOF);
		else
			cl_u8(rcl, VC4_PACKET_STORE_MS_TILE_BUFFER);
	}
}

static int vc4_create_rcl_bo(struct device *dev, struct vc4_exec_info *exec,
			     struct vc4_rcl_setup *setup)
{
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

	if (setup->color_read) {
		loop_body_size += VC4_PACKET_LOAD_TILE_BUFFER_GENERAL_SIZE;
	}
	if (setup->zs_read) {
		if (setup->color_read) {
			loop_body_size += VC4_PACKET_TILE_COORDINATES_SIZE;
			loop_body_size +=
				VC4_PACKET_STORE_TILE_BUFFER_GENERAL_SIZE;
		}
		loop_body_size += VC4_PACKET_LOAD_TILE_BUFFER_GENERAL_SIZE;
	}

	if (has_bin) {
		size += VC4_PACKET_WAIT_ON_SEMAPHORE_SIZE;
		loop_body_size += VC4_PACKET_BRANCH_TO_SUB_LIST_SIZE;
	}

	if (setup->zs_write)
		loop_body_size += VC4_PACKET_STORE_TILE_BUFFER_GENERAL_SIZE;
	if (setup->color_write)
		loop_body_size += VC4_PACKET_STORE_MS_TILE_BUFFER_SIZE;

	/* We need a VC4_PACKET_TILE_COORDINATES in between each store. */
	loop_body_size +=
		VC4_PACKET_TILE_COORDINATES_SIZE *
		((setup->color_write != NULL) + (setup->zs_write != NULL) - 1);

	size += xtiles * ytiles * loop_body_size;

	struct vc4_cl *rcl = &setup->rcl;
	struct vc4_bo *rcl_bo = vc4_bo_create(dev, size);
	if (rcl_bo == NULL) {
		return -E_NOMEM;
	}
	vc4_init_cl(rcl);
	rcl->base = rcl->next = rcl_bo->vaddr;
	rcl->size = size;
	list_add_before(&exec->unref_list, &rcl_bo->unref_head);

	/* The tile buffer gets cleared when the previous tile is stored.  If
	 * the clear values changed between frames, then the tile buffer has
	 * stale clear values in it, so we have to do a store in None mode (no
	 * writes) so that we trigger the tile buffer clear.
	 */
	if (args->flags & VC4_SUBMIT_CL_USE_CLEAR_COLOR) {
		cl_u8(rcl, VC4_PACKET_CLEAR_COLORS);
		cl_u32(rcl, args->clear_color[0]);
		cl_u32(rcl, args->clear_color[1]);
		cl_u32(rcl, args->clear_z);
		cl_u8(rcl, args->clear_s);

		vc4_tile_coordinates(rcl, 0, 0);

		cl_u8(rcl, VC4_PACKET_STORE_TILE_BUFFER_GENERAL);
		cl_u16(rcl, VC4_LOADSTORE_TILE_BUFFER_NONE);
		cl_u32(rcl, 0); /* no address, since we're in None mode */
	}

	cl_u8(rcl, VC4_PACKET_TILE_RENDERING_MODE_CONFIG);
	cl_u32(rcl, (setup->color_write ? (setup->color_write->paddr +
					   args->color_write.offset) :
					  0));
	cl_u16(rcl, args->width);
	cl_u16(rcl, args->height);
	cl_u16(rcl, args->color_write.bits);

	for (y = min_y_tile; y <= max_y_tile; y++) {
		for (x = min_x_tile; x <= max_x_tile; x++) {
			bool first = (x == min_x_tile && y == min_y_tile);
			bool last = (x == max_x_tile && y == max_y_tile);

			emit_tile(exec, setup, x, y, first, last);
		}
	}

	assert(cl_offset(rcl) == size);
	exec->ct1ca = rcl_bo->paddr;
	exec->ct1ea = rcl_bo->paddr + cl_offset(rcl);

	return 0;
}

static int vc4_rcl_surface_setup(struct vc4_exec_info *exec,
				 struct vc4_bo **obj,
				 struct drm_vc4_submit_rcl_surface *surf,
				 bool is_depth)
{
	uint8_t tiling =
		VC4_GET_FIELD(surf->bits, VC4_LOADSTORE_TILE_BUFFER_TILING);
	uint8_t buffer =
		VC4_GET_FIELD(surf->bits, VC4_LOADSTORE_TILE_BUFFER_BUFFER);
	uint8_t format =
		VC4_GET_FIELD(surf->bits, VC4_LOADSTORE_TILE_BUFFER_FORMAT);
	int cpp;

	if (surf->hindex == ~0)
		return 0;

	if (is_depth)
		*obj = vc4_use_bo(exec, surf->hindex);
	else
		*obj = exec->fb_bo;
	if (!*obj)
		return -E_INVAL;

	if (surf->bits & ~(VC4_LOADSTORE_TILE_BUFFER_TILING_MASK |
			   VC4_LOADSTORE_TILE_BUFFER_BUFFER_MASK |
			   VC4_LOADSTORE_TILE_BUFFER_FORMAT_MASK)) {
		kprintf("vc4: Unknown bits in load/store: 0x%04x\n",
			surf->bits);
		return -E_INVAL;
	}

	if (tiling > VC4_TILING_FORMAT_LT) {
		kprintf("vc4: Bad tiling format\n");
		return -E_INVAL;
	}

	if (buffer == VC4_LOADSTORE_TILE_BUFFER_ZS) {
		if (format != 0) {
			kprintf("vc4: No color format should be set for ZS\n");
			return -E_INVAL;
		}
		cpp = 4;
	} else if (buffer == VC4_LOADSTORE_TILE_BUFFER_COLOR) {
		switch (format) {
		case VC4_LOADSTORE_TILE_BUFFER_BGR565:
		case VC4_LOADSTORE_TILE_BUFFER_BGR565_DITHER:
			cpp = 2;
			break;
		case VC4_LOADSTORE_TILE_BUFFER_RGBA8888:
			cpp = 4;
			break;
		default:
			kprintf("vc4: Bad tile buffer format\n");
			return -E_INVAL;
		}
	} else {
		kprintf("vc4: Bad load/store buffer %d.\n", buffer);
		return -E_INVAL;
	}

	if (surf->offset & 0xf) {
		kprintf("vc4: load/store buffer must be 16b aligned.\n");
		return -E_INVAL;
	}

	return 0;
}

static int vc4_rcl_render_config_surface_setup(
	struct vc4_exec_info *exec, struct vc4_bo **obj,
	struct drm_vc4_submit_rcl_surface *surf)
{
	uint8_t tiling =
		VC4_GET_FIELD(surf->bits, VC4_RENDER_CONFIG_MEMORY_FORMAT);
	uint8_t format = VC4_GET_FIELD(surf->bits, VC4_RENDER_CONFIG_FORMAT);
	int cpp;

	if (surf->bits & ~(VC4_RENDER_CONFIG_MEMORY_FORMAT_MASK |
			   VC4_RENDER_CONFIG_FORMAT_MASK)) {
		kprintf("vc4: Unknown bits in render config: 0x%04x\n",
			surf->bits);
		return -E_INVAL;
	}

	if (surf->hindex == ~0)
		return 0;

	*obj = exec->fb_bo;
	if (!*obj)
		return -E_INVAL;

	if (tiling > VC4_TILING_FORMAT_LT) {
		kprintf("vc4: Bad tiling format\n");
		return -E_INVAL;
	}

	switch (format) {
	case VC4_RENDER_CONFIG_FORMAT_BGR565_DITHERED:
	case VC4_RENDER_CONFIG_FORMAT_BGR565:
		cpp = 2;
		break;
	case VC4_RENDER_CONFIG_FORMAT_RGBA8888:
		cpp = 4;
		break;
	default:
		kprintf("vc4: Bad tile buffer format\n");
		return -E_INVAL;
	}

	return 0;
}

int vc4_get_rcl(struct device *dev, struct vc4_exec_info *exec)
{
	struct vc4_rcl_setup setup;
	struct drm_vc4_submit_cl *args = exec->args;
	bool has_bin = args->bin_cl_size != 0;
	int ret = 0;

	if (args->min_x_tile > args->max_x_tile ||
	    args->min_y_tile > args->max_y_tile) {
		kprintf("vc4: Bad render tile set (%d,%d)-(%d,%d)\n",
			args->min_x_tile, args->min_y_tile, args->max_x_tile,
			args->max_y_tile);
		return -E_INVAL;
	}

	if (has_bin && (args->max_x_tile > exec->bin_tiles_x ||
			args->max_y_tile > exec->bin_tiles_y)) {
		kprintf("vc4: Render tiles (%d,%d) outside of bin config "
			"(%d,%d)\n",
			args->max_x_tile, args->max_y_tile, exec->bin_tiles_x,
			exec->bin_tiles_y);
		return -E_INVAL;
	}

	memset(&setup, 0, sizeof(struct vc4_rcl_setup));

	ret = vc4_rcl_surface_setup(exec, &setup.color_read, &args->color_read,
				    false);
	if (ret)
		return ret;

	ret = vc4_rcl_render_config_surface_setup(exec, &setup.color_write,
						  &args->color_write);
	if (ret)
		return ret;

	ret = vc4_rcl_surface_setup(exec, &setup.zs_read, &args->zs_read, true);
	if (ret)
		return ret;

	ret = vc4_rcl_surface_setup(exec, &setup.zs_write, &args->zs_write,
				    true);
	if (ret)
		return ret;

	/* We shouldn't even have the job submitted to us if there's no
	 * surface to write out.
	 */
	if (!setup.color_write && !setup.zs_write) {
		kprintf("vc4: RCL requires color or Z/S write\n");
		return -E_INVAL;
	}

	return vc4_create_rcl_bo(dev, exec, &setup);
}
