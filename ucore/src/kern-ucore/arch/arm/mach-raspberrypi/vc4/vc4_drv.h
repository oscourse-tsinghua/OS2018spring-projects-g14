#ifndef VC4_DRV_H
#define VC4_DRV_H

#include <arm.h>
#include <dev.h>
#include <mmu.h>
#include <list.h>
#include <types.h>

#include "vc4_regs.h"
#include "bcm2708_fb.h"

struct vc4_dev {
	struct device *dev;

	/* The memory used for storing binner tile alloc, tile state,
	 * and overflow memory allocations.  This is freed when V3D
	 * powers down.
	 */
	struct vc4_bo *bin_bo;

	/* Size of blocks allocated within bin_bo. */
	uint32_t bin_alloc_size;

	struct fb_info *fb;

	struct vc4_bo *handle_bo_map;
};

#define VC4_DEV_BUFSIZE (2 * PGSIZE - sizeof(struct vc4_dev))
#define VC4_DEV_BO_NENTRY (VC4_DEV_BUFSIZE / sizeof(struct vc4_bo))

static inline struct vc4_dev *to_vc4_dev(struct device *dev)
{
	return (struct vc4_dev *)dev->driver_data;
}

struct vc4_bo {
	size_t size;
	uint32_t handle;
	uint32_t paddr;
	void *vaddr;

	/* List entry for the BO's position in either
	 * vc4_exec_info->unref_list or vc4_dev->bo_cache.time_list
	 */
	list_entry_t unref_head;
};

struct vc4_exec_info {
	/* Kernel-space copy of the ioctl arguments */
	struct drm_vc4_submit_cl *args;

	/* This is the array of BOs that were looked up at the start of exec.
	 * Command validation will use indices into this array.
	 */
	struct vc4_bo **bo;
	uint32_t bo_count;

	/* List of other BOs used in the job that need to be released
	 * once the job is complete.
	 */
	list_entry_t unref_list;

	/* Current unvalidated indices into @bo loaded by the non-hardware
	 * VC4_PACKET_GEM_HANDLES.
	 */
	uint32_t bo_index[2];

	/* This is the BO where we store the validated command lists, shader
	 * records, and uniforms.
	 */
	struct vc4_bo *exec_bo;

	/**
	 * This tracks the per-shader-record state (packet 64) that
	 * determines the length of the shader record and the offset
	 * it's expected to be found at.  It gets read in from the
	 * command lists.
	 */
	struct vc4_shader_state {
		uint32_t addr;
		/* Maximum vertex index referenced by any primitive using this
		 * shader state.
		 */
		uint32_t max_index;
	} *shader_state;

	/** How many shader states the user declared they were using. */
	uint32_t shader_state_size;
	/** How many shader state records the validator has seen. */
	uint32_t shader_state_count;

	uint8_t bin_tiles_x, bin_tiles_y;
	/* Physical address of the start of the tile alloc array
	 * (where each tile's binned CL will start)
	 */
	uint32_t tile_alloc_offset;

	/**
	 * Computed addresses pointing into exec_bo where we start the
	 * bin thread (ct0) and render thread (ct1).
	 */
	uint32_t ct0ca, ct0ea;
	uint32_t ct1ca, ct1ea;

	/* Pointer to the unvalidated bin CL (if present). */
	void *bin_u;

	/* Pointers to the shader recs.  These paddr gets incremented as CL
	 * packets are relocated in validate_gl_shader_state, and the vaddrs
	 * (u and v) get incremented and size decremented as the shader recs
	 * themselves are validated.
	 */
	void *shader_rec_u;
	void *shader_rec_v;
	uint32_t shader_rec_p;
	uint32_t shader_rec_size;
};

#define le2bo(le, member) to_struct((le), struct vc4_bo, member)

#define V3D_READ(offset) inw(V3D_BASE + offset)
#define V3D_WRITE(offset, val) outw(V3D_BASE + offset, val)

int dev_init_vc4();

int vc4_create_bo_ioctl(struct device *dev, void *data);
int vc4_mmap_bo_ioctl(struct device *dev, void *data);
int vc4_free_bo_ioctl(struct device *dev, void *data);
int vc4_submit_cl_ioctl(struct device *dev, void *data);

struct vc4_bo *vc4_bo_create(struct device *dev, size_t size);
struct vc4_bo *vc4_lookup_bo(struct device *dev, uint32_t handle);
void vc4_bo_destroy(struct device *dev, struct vc4_bo *bo);

/* vc4_validate.c */
int vc4_validate_bin_cl(struct device *dev, void *validated, void *unvalidated,
			struct vc4_exec_info *exec);

int vc4_validate_shader_recs(struct device *dev, struct vc4_exec_info *exec);

int vc4_get_rcl(struct device *dev, struct vc4_exec_info *exec);

#endif // VC4_DRV_H
