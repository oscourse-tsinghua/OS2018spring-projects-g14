#include "vc4_drm.h"
#include "vc4_bufmgr.h"

struct vc4_bo *vc4_bo_alloc(size_t size, size_t align)
{
	struct drm_vc4_create_bo create;
	struct vc4_bo *bo = NULL;
	int ret;

	bo = (struct vc4_bo *)kmalloc(sizeof(struct vc4_bo));
	if (bo == NULL) {
		return NULL;
	}

	memset(bo, 0, sizeof(struct vc4_bo));
	memset(&create, 0, sizeof(create));
	create.size = size;
	create.align = align;
	ret = vc4_create_bo_ioctl(current_dev, &create);
	if (ret != 0) {
		kprintf("GLES: alloc bo ioctl failure.\n");
		kfree(bo);
		return NULL;
	}

	bo->size = size;
	bo->handle = create.handle;

	return bo;
}

struct vc4_bo *vc4_bo_free(struct vc4_bo *bo)
{
}

void *vc4_bo_map(struct vc4_bo *bo)
{
	int ret;

	if (bo->map)
		return bo->map;

	struct drm_vc4_mmap_bo map;
	memset(&map, 0, sizeof(map));

	map.handle = bo->handle;
	ret = vc4_mmap_bo_ioctl(current_dev, &map);
	if (ret != 0) {
		kprintf("GLES: map ioctl failure.\n");
		return NULL;
	}

	bo->map = (void *)map.offset;

	return bo->map;
}
