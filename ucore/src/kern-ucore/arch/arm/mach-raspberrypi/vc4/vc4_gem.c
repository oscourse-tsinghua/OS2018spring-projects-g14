#include <proc.h>
#include <error.h>
#include <assert.h>

#include "vc4_drv.h"
#include "vc4_drm.h"
#include "vc4_regs.h"

static void submit_cl(struct device *dev, uint32_t thread, uint32_t start,
		      uint32_t end)
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

static void vc4_flush_caches(struct device *dev)
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

static void vc4_submit_next_bin_job(struct device *dev,
				    struct vc4_exec_info *exec)
{
	if (!exec)
		return;

	vc4_flush_caches(dev);

	/* Either put the job in the binner if it uses the binner, or
	 * immediately move it to the to-be-rendered queue.
	 */
	if (exec->ct0ca == exec->ct0ea) {
		return;
	}

	// reset binning frame count
	V3D_WRITE(V3D_BFC, 1);

	submit_cl(dev, 0, exec->ct0ca, exec->ct0ea);

	// wait for binning to finish
	while (V3D_READ(V3D_BFC) == 0);
}

static void vc4_submit_next_render_job(struct device *dev,
				       struct vc4_exec_info *exec)
{
	if (!exec)
		return;

	// reset rendering frame count
	V3D_WRITE(V3D_RFC, 1);

	submit_cl(dev, 1, exec->ct1ca, exec->ct1ea);

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
static void vc4_queue_submit(struct device *dev, struct vc4_exec_info *exec)
{
	// TODO
	vc4_submit_next_bin_job(dev, exec);
	vc4_submit_next_render_job(dev, exec);
}

/**
 * vc4_cl_lookup_bos() - Sets up exec->bo[] with the GEM objects
 * referenced by the job.
 * @dev: device
 * @exec: V3D job being set up
 *
 * The command validator needs to reference BOs by their index within
 * the submitted job's BO list.  This does the validation of the job's
 * BO list and reference counting for the lifetime of the job.
 *
 * Note that this function doesn't need to unreference the BOs on
 * failure, because that will happen at vc4_complete_exec() time.
 */
static int vc4_cl_lookup_bos(struct device *dev, struct vc4_exec_info *exec)
{
	struct drm_vc4_submit_cl *args = exec->args;
	struct mm_struct *mm = current->mm;
	uint32_t *handles;
	int ret = 0;
	int i;
	assert(mm);

	exec->bo_count = args->bo_handle_count;

	if (!exec->bo_count) {
		return 0;
	}

	exec->bo = (struct vc4_bo **)kmalloc(exec->bo_count *
					     sizeof(struct vc4_bo *));
	if (!exec->bo) {
		kprintf("vc4: Failed to allocate validated BO pointers\n");
		return -E_NOMEM;
	}

	handles = (uint32_t *)kmalloc(exec->bo_count, sizeof(uint32_t));
	if (!handles) {
		ret = -E_NOMEM;
		kprintf("vc4: Failed to allocate incoming GEM handles\n");
		goto fail;
	}

	if (!copy_from_user(mm, handles, (uintptr_t)args->bo_handles,
			    exec->bo_count * sizeof(uint32_t), 0)) {
		ret = -E_FAULT;
		kprintf("vc4: Failed to copy in GEM handles\n");
		goto fail;
	}

	for (i = 0; i < exec->bo_count; i++) {
		struct vc4_bo *bo = vc4_lookup_bo(dev, handles[i]);
		if (!bo) {
			kprintf("vc4: Failed to look up GEM BO %d: %d\n", i,
				handles[i]);
			ret = -E_INVAL;
			goto fail;
		}
		exec->bo[i] = bo;
	}

fail:
	kfree(handles);
	return ret;
}

static int vc4_get_bcl(struct device *dev, struct vc4_exec_info *exec)
{
	struct drm_vc4_submit_cl *args = exec->args;
	void *temp = NULL;
	void *bin;
	int ret = 0;
	struct mm_struct *mm = current->mm;
	assert(mm);

	uint32_t bin_offset = 0;
	uint32_t shader_rec_offset =
		ROUNDUP(bin_offset + args->bin_cl_size, 16);
	uint32_t uniforms_offset = shader_rec_offset + args->shader_rec_size;
	uint32_t exec_size = uniforms_offset + args->uniforms_size;
	uint32_t temp_size = exec_size + (sizeof(struct vc4_shader_state) *
					  args->shader_rec_count);
	struct vc4_bo *bo;

	if (shader_rec_offset < args->bin_cl_size ||
	    uniforms_offset < shader_rec_offset ||
	    exec_size < uniforms_offset ||
	    args->shader_rec_count >=
		    ((~0U) / sizeof(struct vc4_shader_state)) ||
	    temp_size < exec_size) {
		kprintf("vc4: overflow in exec arguments\n");
		ret = -E_INVAL;
		goto fail;
	}

	temp = (void *)kmalloc(temp_size);
	if (!temp) {
		kprintf("vc4: Failed to allocate storage for copying "
			"in bin/render CLs.\n");
		ret = -E_NOMEM;
		goto fail;
	}
	bin = temp + bin_offset;
	exec->shader_rec_u = temp + shader_rec_offset;
	exec->shader_state = temp + exec_size;
	exec->shader_state_size = args->shader_rec_count;

	if (args->bin_cl_size &&
	    !copy_from_user(mm, bin, (uintptr_t)args->bin_cl, args->bin_cl_size,
			    0)) {
		ret = -E_FAULT;
		goto fail;
	}

	if (args->shader_rec_size &&
	    !copy_from_user(mm, exec->shader_rec_u, (uintptr_t)args->shader_rec,
			    args->shader_rec_size, 0)) {
		ret = -E_FAULT;
		goto fail;
	}

	bo = vc4_bo_create(dev, exec_size);
	if (bo == NULL) {
		kprintf("vc4: Couldn't allocate BO for binning\n");
		ret = -E_NOMEM;
		goto fail;
	}
	exec->exec_bo = bo;

	list_add_before(&exec->unref_list, &exec->exec_bo->unref_head);

	exec->ct0ca = exec->exec_bo->paddr + bin_offset;

	exec->bin_u = bin;

	exec->shader_rec_v = exec->exec_bo->vaddr + shader_rec_offset;
	exec->shader_rec_p = exec->exec_bo->paddr + shader_rec_offset;
	exec->shader_rec_size = args->shader_rec_size;

	ret = vc4_validate_bin_cl(dev, exec->exec_bo->vaddr + bin_offset, bin,
				  exec);
	if (ret)
		goto fail;

	ret = vc4_validate_shader_recs(dev, exec);
	if (ret)
		goto fail;

fail:
	kfree(temp);
	return ret;
}

static void vc4_complete_exec(struct device *dev, struct vc4_exec_info *exec)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);

	if (exec->bo) {
		kfree(exec->bo);
	}

	while (!list_empty(&exec->unref_list)) {
		list_entry_t *le = list_next(&exec->unref_list);
		struct vc4_bo *bo = le2bo(le, unref_head);
		list_del(&bo->unref_head);
		vc4_bo_destroy(dev, bo);
	}

	kfree(exec);
}

/**
 * vc4_submit_cl_ioctl() - Submits a job (frame) to the VC4.
 * @dev: vc4 device
 * @data: ioctl argument
 *
 * This is the main entrypoint for userspace to submit a 3D frame to
 * the GPU.  Userspace provides the binner command list (if
 * applicable), and the kernel sets up the render command list to draw
 * to the framebuffer described in the ioctl, using the command lists
 * that the 3D engine's binner will produce.
 */
int vc4_submit_cl_ioctl(struct device *dev, void *data)
{
	struct drm_vc4_submit_cl *args = data;
	struct vc4_exec_info *exec;
	int ret = 0;

	exec = (struct vc4_exec_info *)kmalloc(sizeof(struct vc4_exec_info));
	if (!exec) {
		kprintf("vc4: malloc failure on exec struct\n");
		return -E_NOMEM;
	}

	memset(exec, 0, sizeof(struct vc4_exec_info));
	exec->args = args;
	list_init(&exec->unref_list);

	ret = vc4_cl_lookup_bos(dev, exec);
	if (ret)
		goto fail;

	if (exec->args->bin_cl_size != 0) {
		ret = vc4_get_bcl(dev, exec);
		if (ret)
			goto fail;
	} else {
		exec->ct0ca = 0;
		exec->ct0ea = 0;
	}

	ret = vc4_get_rcl(dev, exec);
	if (ret)
		goto fail;

	/* Clear this out of the struct we'll be putting in the queue,
	 * since it's part of our stack.
	 */
	exec->args = NULL;

	vc4_queue_submit(dev, exec);

fail:
	vc4_complete_exec(dev, exec);

	return ret;
}
