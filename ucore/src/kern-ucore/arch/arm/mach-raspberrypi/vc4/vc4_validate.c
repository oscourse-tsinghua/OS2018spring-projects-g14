#include <error.h>
#include <assert.h>

#include "vc4_cl.h"
#include "vc4_drv.h"
#include "vc4_drm.h"
#include "vc4_packet.h"

#define VALIDATE_ARGS                                                          \
	struct device *dev, struct vc4_exec_info *exec, void *validated,       \
		void *untrusted

struct vc4_bo *vc4_use_bo(struct vc4_exec_info *exec, uint32_t hindex)
{
	if (hindex >= exec->bo_count) {
		kprintf("vc4: BO index %d greater than BO count %d\n", hindex,
			exec->bo_count);
		return NULL;
	}

	return exec->bo[hindex];
}

static struct vc4_bo *vc4_use_handle(struct vc4_exec_info *exec,
				     uint32_t gem_handles_packet_index)
{
	return vc4_use_bo(exec, exec->bo_index[gem_handles_packet_index]);
}

static int validate_indexed_prim_list(VALIDATE_ARGS)
{
	struct vc4_bo *ib;
	uint32_t length = get_unaligned_32(untrusted + 1);
	uint32_t offset = get_unaligned_32(untrusted + 5);
	uint32_t max_index = get_unaligned_32(untrusted + 9);
	uint32_t index_size = (*(uint8_t *)(untrusted + 0) >> 4) ? 2 : 1;
	struct vc4_shader_state *shader_state;

	/* Check overflow condition */
	if (exec->shader_state_count == 0) {
		kprintf("vc4: shader state must precede primitives\n");
		return -E_INVAL;
	}
	shader_state = &exec->shader_state[exec->shader_state_count - 1];

	if (max_index > shader_state->max_index)
		shader_state->max_index = max_index;

	ib = vc4_use_handle(exec, 0);
	if (!ib)
		return -E_INVAL;

	if (offset > ib->size || (ib->size - offset) / index_size < length) {
		kprintf("vc4: IB access overflow (%d + %d*%d > %d)\n", offset,
			length, index_size, ib->size);
		return -E_INVAL;
	}

	put_unaligned_32(validated + 5, ib->paddr + offset);

	return 0;
}

static int validate_nv_shader_state(VALIDATE_ARGS)
{
	uint32_t i = exec->shader_state_count++;

	if (i >= exec->shader_state_size) {
		kprintf("vc4: More requests for shader states than declared\n");
		return -E_INVAL;
	}

	exec->shader_state[i].addr = get_unaligned_32(untrusted);
	exec->shader_state[i].max_index = 0;

	put_unaligned_32(validated,
			 (exec->shader_rec_p + exec->shader_state[i].addr));

	exec->shader_rec_p += 16;

	return 0;
}

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

static int validate_gem_handles(VALIDATE_ARGS)
{
	memcpy(exec->bo_index, untrusted, sizeof(exec->bo_index));
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
			  validate_indexed_prim_list),
	VC4_DEFINE_PACKET(VC4_PACKET_GL_ARRAY_PRIMITIVE,
			  NULL), // validate_gl_array_primitive

	VC4_DEFINE_PACKET(VC4_PACKET_PRIMITIVE_LIST_FORMAT, NULL),

	VC4_DEFINE_PACKET(VC4_PACKET_GL_SHADER_STATE,
			  NULL), // validate_gl_shader_state
	VC4_DEFINE_PACKET(VC4_PACKET_NV_SHADER_STATE, validate_nv_shader_state),

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

	VC4_DEFINE_PACKET(VC4_PACKET_GEM_HANDLES, validate_gem_handles),
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

		if (cmd != VC4_PACKET_GEM_HANDLES)
			memcpy(dst_pkt, src_pkt, info->len);

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

static int validate_nv_shader_rec(struct device *dev,
				  struct vc4_exec_info *exec,
				  struct vc4_shader_state *state)
{
	uint32_t *src_handles;
	void *pkt_u, *pkt_v;
	uint32_t shader_reloc_count = 1;
	struct vc4_bo *bo[shader_reloc_count];
	uint32_t nr_relocs = 3, packet_size = 16;
	int i;

	nr_relocs = shader_reloc_count + 2;
	if (nr_relocs * 4 > exec->shader_rec_size) {
		kprintf("vc4: overflowed shader recs reading %d handles "
			"from %d bytes left\n",
			nr_relocs, exec->shader_rec_size);
		return -E_INVAL;
	}
	src_handles = exec->shader_rec_u;
	exec->shader_rec_u += nr_relocs * 4;
	exec->shader_rec_size -= nr_relocs * 4;

	if (packet_size > exec->shader_rec_size) {
		kprintf("vc4: overflowed shader recs copying %db packet "
			"from %d bytes left\n",
			packet_size, exec->shader_rec_size);
		return -E_INVAL;
	}
	pkt_u = exec->shader_rec_u;
	pkt_v = exec->shader_rec_v;
	memcpy(pkt_v, pkt_u, packet_size);
	exec->shader_rec_u += packet_size;
	exec->shader_rec_v += packet_size;
	exec->shader_rec_size -= packet_size;

	for (i = 0; i < nr_relocs; i++) {
		bo[i] = vc4_use_bo(exec, src_handles[i]);
		if (!bo[i])
			return -E_INVAL;
	}

	uint8_t stride = *(uint8_t *)(pkt_u + 1);
	uint32_t fs_offset = get_unaligned_32(pkt_u + 4);
	uint32_t uniform_offset = get_unaligned_32(pkt_u + 8);
	uint32_t data_offset = get_unaligned_32(pkt_u + 12);
	uint32_t max_index;

	put_unaligned_32(pkt_v + 4, bo[0]->paddr + fs_offset);
	put_unaligned_32(pkt_v + 8, bo[1]->paddr + uniform_offset);

	if (stride != 0) {
		max_index = (bo[2]->size - data_offset) / stride;
		if (state->max_index > max_index) {
			kprintf("vc4: primitives use index %d out of "
				"supplied %d\n",
				state->max_index, max_index);
			return -E_INVAL;
		}
	}

	put_unaligned_32(pkt_v + 12, bo[2]->paddr + data_offset);

	return 0;
}

int vc4_validate_shader_recs(struct device *dev, struct vc4_exec_info *exec)
{
	uint32_t i;
	int ret = 0;

	for (i = 0; i < exec->shader_state_count; i++) {
		ret = validate_nv_shader_rec(dev, exec, &exec->shader_state[i]);
		if (ret)
			return ret;
	}

	return ret;
}
