#include <dev.h>
#include <inode.h>
#include <error.h>

#include "vc4_drv.h"
#include "vc4_drm.h"
#include "bcm2708_fb.h"
#include "mailbox_property.h"

static void bo_map_init(struct vc4_bo *bo)
{
	memset(bo, 0, sizeof(struct vc4_bo) * VC4_DEV_BO_NENTRY);
}

static int vc4_allocate_bin_bo(struct device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct vc4_bo *bo;

	uint32_t size = 512 * 1024;
	bo = vc4_bo_create(dev, size);
	if (bo == NULL) {
		return -E_NOMEM;
	}

	vc4->bin_bo = bo;
	vc4->bin_alloc_size = size;

	return 0;
}

static int vc4_bind_fb_bo(struct device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct vc4_bo *bo;
	struct fb_info *fb;

	bo = (struct vc4_bo *)kmalloc(sizeof(struct vc4_bo));
	if (bo == NULL)
		return -E_NOMEM;

	fb = get_fb_info();
	if (fb == NULL)
		return -E_NODEV;

	unsigned long screen_size; /* Amount of ioremapped VRAM or 0 */
	unsigned long fb_bus_address; /* Physical address */

	bo->size = fb->screen_size;
	bo->handle = 0;
	bo->paddr = fb->fb_bus_address;
	bo->vaddr = fb->screen_base;
	list_init(&bo->unref_head);

	vc4->fb_bo = bo;

	return 0;
}

static int vc4_probe(struct device *dev)
{
	struct vc4_dev *vc4;
	int ret = 0;

	static_assert((int)VC4_DEV_BO_NENTRY > 128);
	vc4 = (struct vc4_dev *)kmalloc(sizeof(struct vc4_dev) +
					VC4_DEV_BUFSIZE);
	if (!vc4)
		return -E_NOMEM;

	if (fb_check() == 0) {
		ret = -E_NODEV;
		kprintf("VC4: no framebuffer found.\n");
		goto fail;
	}

	// The blob now has this nice handy call which powers up the v3d pipeline.
	if ((ret = mbox_qpu_enable(1)) != 0) {
		kprintf("VC4: cannot enable qpu.\n");
		goto fail;
	}

	if (V3D_READ(V3D_IDENT0) != V3D_EXPECTED_IDENT0) {
		ret = -E_INVAL;
		kprintf("VC4: V3D_IDENT0 read 0x%08x instead of 0x%08x\n",
			V3D_READ(V3D_IDENT0), V3D_EXPECTED_IDENT0);
		goto fail;
	}

	vc4->dev = dev;
	vc4->handle_bo_map = (struct vc4_bo *)(vc4 + 1);
	dev->driver_data = vc4;

	bo_map_init(vc4->handle_bo_map);

	if ((ret = vc4_bind_fb_bo(dev))) {
		kprintf("VC4: cannot bind framebuffer bo.\n");
		goto fail;
	}
	if ((ret = vc4_allocate_bin_bo(dev))) {
		kprintf("VC4: cannot alloc bin bo.\n");
		goto fail;
	}

	kprintf("VideoCore IV GPU initialized.\n");

	goto out;

fail:
	kfree(vc4);
	kprintf("VideoCore IV GPU failed to initialize.\n");
out:
	return ret;
}

static void vc4_gem_destroy()
{
	// TODO
}

static int vc4_open(struct device *dev, uint32_t open_flags)
{
	return 0;
}

static int vc4_close(struct device *dev)
{
	return 0;
}

static int vc4_ioctl(struct device *dev, int op, void *data)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	if (!vc4)
		return -E_NODEV;

	int ret = 0;

	switch (op) {
	case DRM_IOCTL_VC4_SUBMIT_CL:
		ret = vc4_submit_cl_ioctl(dev, data);
		break;
	case DRM_IOCTL_VC4_CREATE_BO:
		ret = vc4_create_bo_ioctl(dev, data);
		break;
	case DRM_IOCTL_VC4_MMAP_BO:
		ret = vc4_mmap_bo_ioctl(dev, data);
		break;
	case DRM_IOCTL_VC4_FREE_BO:
		ret = vc4_free_bo_ioctl(dev, data);
		break;
	default:
		ret = -E_INVAL;
	}
	return ret;
}

static int vc4_device_init(struct device *dev)
{
	memset(dev, 0, sizeof(*dev));

	int ret;
	if ((ret = vc4_probe(dev)) != 0) {
		return ret;
	}

	dev->d_blocks = 0;
	dev->d_blocksize = 1;
	dev->d_open = vc4_open;
	dev->d_close = vc4_close;
	dev->d_io = NULL_VOP_INVAL;
	dev->d_ioctl = vc4_ioctl;
	dev->d_mmap = NULL_VOP_INVAL;

	return ret;
}

int dev_init_vc4()
{
	struct inode *node;
	int ret;
	if ((node = dev_create_inode()) == NULL) {
		ret = -E_NODEV;
		kprintf("vc4: dev_create_node failed: %e\n", ret);
		goto out;
	}

	if ((ret = vc4_device_init(vop_info(node, device))) != 0) {
		kprintf("vc4: vc4_device_init failed: %e\n", ret);
		goto free_node;
	}
	if ((ret = vfs_add_dev("gpu0", node, 0)) != 0) {
		kprintf("vc4: vfs_add_dev failed: %e\n", ret);
		goto free_node;
	}

	return 0;

free_node:
	dev_kill_inode(node);
out:
	return ret;
}
