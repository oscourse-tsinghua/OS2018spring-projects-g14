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

#define DRM_IOCTL_VC4_SUBMIT_CL                         0x00
#define DRM_IOCTL_VC4_WAIT_SEQNO                        0x01
#define DRM_IOCTL_VC4_WAIT_BO                           0x02
#define DRM_IOCTL_VC4_CREATE_BO                         0x03
#define DRM_IOCTL_VC4_MMAP_BO                           0x04
#define DRM_IOCTL_VC4_CREATE_SHADER_BO                  0x05
#define DRM_IOCTL_VC4_GET_HANG_STATE                    0x06
#define DRM_IOCTL_VC4_GET_PARAM                         0x07
#define DRM_IOCTL_VC4_SET_TILING                        0x08
#define DRM_IOCTL_VC4_GET_TILING                        0x09
#define DRM_IOCTL_VC4_LABEL_BO                          0x0a

#define V3D_READ(offset) inw(V3D_BASE + offset)
#define V3D_WRITE(offset, val) outw(V3D_BASE + offset, val)

int dev_init_vc4();

struct vc4_bo *vc4_bo_create(size_t size, size_t align);
void vc4_bo_destroy(struct vc4_bo *bo);

#endif // VC4_DRV_H
