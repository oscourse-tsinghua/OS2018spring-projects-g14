#include <dev.h>
#include <inode.h>
#include <error.h>

#include "vc4_drv.h"
#include "bcm2708_fb.h"
#include "mailbox_property.h"

static int vc4_probe(struct device *dev)
{
	int ret = 0;
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

	dev->driver_data = get_fb_info();

	kprintf("VideoCore IV GPU initialized.\n");

	goto out;

fail:
	kprintf("VideoCore IV GPU failed to initialize.\n");
out:
	return ret;
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
	struct fb_info *info = (struct fb_info *)dev->driver_data;
	if (!info)
		return -E_NODEV;

	kprintf("%08x %08x\n", op, data);
	int ret = 0;
	extern void vc4_hello_triangle(struct fb_info * fb);

	switch (op) {
	case DRM_IOCTL_VC4_SUBMIT_CL:
		vc4_hello_triangle(info);
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

	goto out;

free_node:
	dev_kill_inode(node);
out:
	return ret;
}
