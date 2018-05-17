#ifndef VC4_DRV_H
#define VC4_DRV_H

#include <arm.h>
#include <dev.h>
#include <mmu.h>
#include <types.h>

#include "vc4_regs.h"
#include "bcm2708_fb.h"

struct device *current_dev; // FIXME temporary

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

	void *map; // user
};

struct vc4_exec_info {
	/* Kernel-space copy of the ioctl arguments */
	struct drm_vc4_submit_cl *args;

	/* This is the array of BOs that were looked up at the start of exec.
	 * Command validation will use indices into this array.
	 */
	struct drm_gem_cma_object **bo;
	uint32_t bo_count;

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
};

#define V3D_READ(offset) inw(V3D_BASE + offset)
#define V3D_WRITE(offset, val) outw(V3D_BASE + offset, val)

int dev_init_vc4();

int vc4_create_bo_ioctl(struct device *dev, void *data);
int vc4_mmap_bo_ioctl(struct device *dev, void *data);
int vc4_submit_cl_ioctl(struct device *dev, void *data);

struct vc4_bo *vc4_bo_create(struct device *dev, size_t size, size_t align);
struct vc4_bo *vc4_lookup_bo(struct device *dev, uint32_t handle);
void vc4_bo_destroy(struct device *dev, struct vc4_bo *bo);

/* vc4_validate.c */
int vc4_validate_bin_cl(struct device *dev, void *validated, void *unvalidated,
			struct vc4_exec_info *exec);

int vc4_validate_shader_recs(struct device *dev, struct vc4_exec_info *exec);

int vc4_get_rcl(struct device *dev, struct vc4_exec_info *exec);

#endif // VC4_DRV_H
