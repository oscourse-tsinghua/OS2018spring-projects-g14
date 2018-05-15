#ifndef VC4_DRV_H
#define VC4_DRV_H

#include <arm.h>
#include <types.h>

#include "vc4_regs.h"

struct vc4_bo {
	size_t size;
	uint32_t handle;
	uint32_t paddr;
	void *vaddr;
};

struct vc4_exec_info {
	/* Kernel-space copy of the ioctl arguments */
	struct drm_vc4_submit_cl *args;

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

int vc4_submit_cl_ioctl(void *data);

struct vc4_bo *vc4_bo_create(size_t size, size_t align);
void vc4_bo_destroy(struct vc4_bo *bo);

int vc4_get_rcl(struct vc4_exec_info *exec);

#endif // VC4_DRV_H
