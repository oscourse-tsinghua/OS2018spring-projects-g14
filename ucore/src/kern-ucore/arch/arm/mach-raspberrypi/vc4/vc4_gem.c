#include <error.h>
#include <assert.h>

#include "vc4_drv.h"
#include "vc4_drm.h"
#include "vc4_regs.h"

static void submit_cl(uint32_t thread, uint32_t start, uint32_t end)
{
	/* Set the current and end address of the control list.
	 * Writing the end register is what starts the job.
	 */

	// stop the thread
	V3D_WRITE(V3D_CTNCS(thread), 0x20);

	// Wait for thread to stop
	while (V3D_READ(V3D_CTNCS(thread)) & 0x20);

	V3D_WRITE(V3D_CTNCA(thread), start);
	V3D_WRITE(V3D_CTNEA(thread), end);
}

static void vc4_flush_caches()
{
	/* Flush the GPU L2 caches.  These caches sit on top of system
	 * L3 (the 128kb or so shared with the CPU), and are
	 * non-allocating in the L3.
	 */
	V3D_WRITE(V3D_L2CACTL, V3D_L2CACTL_L2CCLR);

	V3D_WRITE(V3D_SLCACTL, VC4_SET_FIELD(0xf, V3D_SLCACTL_T1CC) |
				       VC4_SET_FIELD(0xf, V3D_SLCACTL_T0CC) |
				       VC4_SET_FIELD(0xf, V3D_SLCACTL_UCC) |
				       VC4_SET_FIELD(0xf, V3D_SLCACTL_ICC));
}

static void vc4_submit_next_bin_job(struct vc4_exec_info *exec)
{
	if (!exec)
		return;

	vc4_flush_caches();

	/* Either put the job in the binner if it uses the binner, or
	 * immediately move it to the to-be-rendered queue.
	 */
	if (exec->ct0ca == exec->ct0ea) {
		return;
	}

	// reset binning frame count
	V3D_WRITE(V3D_BFC, 1);

	submit_cl(0, exec->ct0ca, exec->ct0ea);

	// wait for binning to finish
	while (V3D_READ(V3D_BFC) == 0);
}

static void vc4_submit_next_render_job(struct vc4_exec_info *exec)
{
	if (!exec)
		return;

	// reset rendering frame count
	V3D_WRITE(V3D_RFC, 1);

	submit_cl(1, exec->ct1ca, exec->ct1ea);

	// wait for render to finish
	while (V3D_READ(V3D_RFC) == 0);
}

/* Queues a struct vc4_exec_info for execution.  If no job is
 * currently executing, then submits it.
 *
 * Unlike most GPUs, our hardware only handles one command list at a
 * time.  To queue multiple jobs at once, we'd need to edit the
 * previous command list to have a jump to the new one at the end, and
 * then bump the end address.  That's a change for a later date,
 * though.
 */
static void vc4_queue_submit(struct vc4_exec_info *exec)
{
	// TODO
	vc4_submit_next_bin_job(exec);
	vc4_submit_next_render_job(exec);
}

static int vc4_get_bcl(struct vc4_exec_info *exec)
{
	// TODO
	struct drm_vc4_submit_cl *args = exec->args;
	exec->ct0ca = args->bin_cl;
	exec->ct0ea = exec->ct0ca + args->bin_cl_size;
	return 0;
}

static void vc4_complete_exec(struct vc4_exec_info *exec)
{
	kfree(exec);
}

/**
 * vc4_submit_cl_ioctl() - Submits a job (frame) to the VC4.
 * @data: ioctl argument
 *
 * This is the main entrypoint for userspace to submit a 3D frame to
 * the GPU.  Userspace provides the binner command list (if
 * applicable), and the kernel sets up the render command list to draw
 * to the framebuffer described in the ioctl, using the command lists
 * that the 3D engine's binner will produce.
 */
int vc4_submit_cl_ioctl(void *data)
{
	struct drm_vc4_submit_cl *args = data;
	struct vc4_exec_info *exec;
	int ret = 0;

	exec = (struct vc4_exec_info *)kmalloc(sizeof(struct vc4_exec_info));
	if (!exec) {
		kprintf("vc4: malloc failure on exec struct\n");
		return -E_NOMEM;
	}

	exec->args = args;
	if (exec->args->bin_cl_size != 0) {
		ret = vc4_get_bcl(exec);
		if (ret)
			goto fail;
	} else {
		exec->ct0ca = 0;
		exec->ct0ea = 0;
	}

	ret = vc4_get_rcl(exec);
	if (ret)
		goto fail;

	/* Clear this out of the struct we'll be putting in the queue,
	 * since it's part of our stack.
	 */
	exec->args = NULL;

	vc4_queue_submit(exec);

	return 0;

fail:
	vc4_complete_exec(exec);

	return ret;
}
