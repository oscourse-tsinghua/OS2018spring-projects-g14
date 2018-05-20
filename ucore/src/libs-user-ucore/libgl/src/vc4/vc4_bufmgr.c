#include <file.h>
#include <malloc.h>

#include "vc4_cl.h"
#include "vc4_drm.h"
#include "vc4_bufmgr.h"

struct vc4_bo *vc4_bo_alloc(struct vc4_context *vc4, size_t size)
{
	struct drm_vc4_create_bo create;
	struct vc4_bo *bo = NULL;
	int ret;

	size = ROUNDUP(size, 4096);

	bo = (struct vc4_bo *)malloc(sizeof(struct vc4_bo));
	if (bo == NULL) {
		return NULL;
	}

	memset(bo, 0, sizeof(struct vc4_bo));
	memset(&create, 0, sizeof(create));
	create.size = size;
	ret = ioctl(vc4->fd, DRM_IOCTL_VC4_CREATE_BO, &create);
	if (ret != 0) {
		cprintf("GLES: alloc bo ioctl failure: %e.\n", ret);
		free(bo);
		return NULL;
	}

	bo->size = size;
	bo->handle = create.handle;

	cl_u32(&vc4->bo_pointers, (uintptr_t)bo);

	return bo;
}

void vc4_bo_free(struct vc4_context *vc4, struct vc4_bo *bo)
{
	int ret;

	if (bo == NULL) {
		return;
	}

	if (bo->map) {
		sys_munmap(bo->map, bo->size);
	}

	struct drm_vc4_free_bo f;
	memset(&f, 0, sizeof(f));

	f.handle = bo->handle;
	ret = ioctl(vc4->fd, DRM_IOCTL_VC4_FREE_BO, &f);
	if (ret != 0) {
		cprintf("GLES: free bo ioctl failure: %e.\n", ret);
	}

	free(bo);
}

void *vc4_bo_map(struct vc4_context *vc4, struct vc4_bo *bo)
{
	int ret;

	if (bo == NULL) {
		return NULL;
	}

	if (bo->map)
		return bo->map;

	struct drm_vc4_mmap_bo map;
	memset(&map, 0, sizeof(map));

	map.handle = bo->handle;
	ret = ioctl(vc4->fd, DRM_IOCTL_VC4_MMAP_BO, &map);
	if (ret != 0) {
		cprintf("GLES: map ioctl failure: %e.\n", ret);
		return NULL;
	}

	bo->map = (void *)map.offset;

	return bo->map;
}
