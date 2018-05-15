#include "vc4_drm.h"
#include "vc4_drv.h"

int vc4_get_rcl(struct vc4_exec_info *exec)
{
	// TODO
	struct drm_vc4_submit_cl *args = exec->args;
	exec->ct1ca = args->render_cl;
	exec->ct1ea = exec->ct1ca + args->render_cl_size;
	return 0;
}
