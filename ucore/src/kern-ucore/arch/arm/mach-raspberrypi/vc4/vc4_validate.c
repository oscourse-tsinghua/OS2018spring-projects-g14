#include <error.h>

#include "vc4_cl.h"
#include "vc4_drv.h"
#include "vc4_drm.h"
#include "vc4_packet.h"

#define VALIDATE_ARGS                                                          \
	struct device *dev, struct vc4_exec_info *exec, void *validated,       \
		void *untrusted

static int validate_tile_binning_config(VALIDATE_ARGS)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	uint32_t tile_state_size;
	uint32_t tile_count, bin_addr;

	exec->bin_tiles_x = *(uint8_t *)(untrusted + 12);
	exec->bin_tiles_y = *(uint8_t *)(untrusted + 13);
	tile_count = exec->bin_tiles_x * exec->bin_tiles_y;

	bin_addr = vc4->bin_bo->paddr;

	/* The tile state data array is 48 bytes per tile, and we put it at
	 * the start of a BO containing both it and the tile alloc.
	 */
	tile_state_size = 48 * tile_count;

	/* Since the tile alloc array will follow us, align. */
	exec->tile_alloc_offset = bin_addr + ROUNDUP(tile_state_size, 4096);

	/* tile alloc address. */
	put_unaligned_32(validated + 0, exec->tile_alloc_offset);
	/* tile alloc size. */
	put_unaligned_32(validated + 4, bin_addr + vc4->bin_alloc_size -
						exec->tile_alloc_offset);
	/* tile state address. */
	put_unaligned_32(validated + 8, bin_addr);

	return 0;
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define VC4_DEFINE_PACKET(packet, func)                                        \
	[packet] = { packet##_SIZE, #packet, func }

static const struct cmd_info {
	uint16_t len;
	const char *name;
	int (*func)(struct device *dev, struct vc4_exec_info *exec,
		    void *validated, void *untrusted);
} cmd_info[] = {
	VC4_DEFINE_PACKET(VC4_PACKET_HALT, NULL),
	VC4_DEFINE_PACKET(VC4_PACKET_NOP, NULL),
	VC4_DEFINE_PACKET(VC4_PACKET_FLUSH, NULL), // validate_flush
	VC4_DEFINE_PACKET(VC4_PACKET_FLUSH_ALL, NULL),
	VC4_DEFINE_PACKET(VC4_PACKET_START_TILE_BINNING,
			  NULL), // validate_start_tile_binning
	VC4_DEFINE_PACKET(VC4_PACKET_INCREMENT_SEMAPHORE,
			  NULL), // validate_increment_semaphore

	VC4_DEFINE_PACKET(VC4_PACKET_GL_INDEXED_PRIMITIVE,
			  NULL), // validate_indexed_prim_list
	VC4_DEFINE_PACKET(VC4_PACKET_GL_ARRAY_PRIMITIVE,
			  NULL), // validate_gl_array_primitive

	VC4_DEFINE_PACKET(VC4_PACKET_PRIMITIVE_LIST_FORMAT, NULL),

	VC4_DEFINE_PACKET(VC4_PACKET_GL_SHADER_STATE,
			  NULL), // validate_gl_shader_state
	VC4_DEFINE_PACKET(VC4_PACKET_NV_SHADER_STATE,
			  NULL), // validate_nv_shader_state

	VC4_DEFINE_PACKET(VC4_PACKET_CONFIGURATION_BITS, NULL),
	VC4_DEFINE_PACKET(VC4_PACKET_FLAT_SHADE_FLAGS, NULL),
	VC4_DEFINE_PACKET(VC4_PACKET_POINT_SIZE, NULL),
	VC4_DEFINE_PACKET(VC4_PACKET_LINE_WIDTH, NULL),
	VC4_DEFINE_PACKET(VC4_PACKET_RHT_X_BOUNDARY, NULL),
	VC4_DEFINE_PACKET(VC4_PACKET_DEPTH_OFFSET, NULL),
	VC4_DEFINE_PACKET(VC4_PACKET_CLIP_WINDOW, NULL),
	VC4_DEFINE_PACKET(VC4_PACKET_VIEWPORT_OFFSET, NULL),
	VC4_DEFINE_PACKET(VC4_PACKET_CLIPPER_XY_SCALING, NULL),
	/* Note: The docs say this was also 105, but it was 106 in the
	 * initial userland code drop.
	 */
	VC4_DEFINE_PACKET(VC4_PACKET_CLIPPER_Z_SCALING, NULL),

	VC4_DEFINE_PACKET(VC4_PACKET_TILE_BINNING_MODE_CONFIG,
			  validate_tile_binning_config),

	VC4_DEFINE_PACKET(VC4_PACKET_GEM_HANDLES, NULL), // validate_gem_handles
};

int vc4_validate_bin_cl(struct device *dev, void *validated, void *unvalidated,
			struct vc4_exec_info *exec)
{
	uint32_t len = exec->args->bin_cl_size;
	uint32_t dst_offset = 0;
	uint32_t src_offset = 0;

	while (src_offset < len) {
		void *dst_pkt = validated + dst_offset;
		void *src_pkt = unvalidated + src_offset;
		uint8_t cmd = *(uint8_t *)src_pkt;
		const struct cmd_info *info;

		if (cmd >= ARRAY_SIZE(cmd_info)) {
			kprintf("vc4: 0x%08x: packet %d out of bounds\n",
				src_offset, cmd);
			return -E_INVAL;
		}

		info = &cmd_info[cmd];
		if (!info->name) {
			kprintf("vc4: 0x%08x: packet %d invalid\n", src_offset,
				cmd);
			return -E_INVAL;
		}

		if (src_offset + info->len > len) {
			kprintf("vc4: 0x%08x: packet %d (%s) length 0x%08x "
				"exceeds bounds (0x%08x)\n",
				src_offset, cmd, info->name, info->len,
				src_offset + len);
			return -E_INVAL;
		}

		// if (cmd != VC4_PACKET_GEM_HANDLES)
		// 	memcpy(dst_pkt, src_pkt, info->len);

		if (info->func &&
		    info->func(dev, exec, dst_pkt + 1, src_pkt + 1)) {
			kprintf("vc4: 0x%08x: packet %d (%s) failed to validate\n",
				src_offset, cmd, info->name);
			return -E_INVAL;
		}

		src_offset += info->len;
		/* GEM handle loading doesn't produce HW packets. */
		if (cmd != VC4_PACKET_GEM_HANDLES)
			dst_offset += info->len;

		/* When the CL hits halt, it'll stop reading anything else. */
		if (cmd == VC4_PACKET_HALT)
			break;
	}

	exec->ct0ea = exec->ct0ca + dst_offset;

	return 0;
}

int vc4_validate_shader_recs(struct device *dev, struct vc4_exec_info *exec)
{
	uint32_t i;
	int ret = 0;

	// TODO

	return ret;
}
