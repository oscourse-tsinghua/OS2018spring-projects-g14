#ifndef VC4_CONTEXT
#define VC4_CONTEXT

#include <types.h>

#include "vc4_cl.h"
#include "bcm2708_fb.h"

struct vc4_program_stateobj {
	struct vc4_bo *vs, *fs;
};

struct vc4_context {
	struct vc4_cl bcl;
	struct vc4_cl shader_rec;
	uint32_t shader_rec_count;

	/**
	 * Set if some drawing (triangles, blits, or just a glClear()) has
	 * been done to the FBO, meaning that we need to
	 * DRM_IOCTL_VC4_SUBMIT_CL.
	 */
	bool needs_flush;

	struct vc4_program_stateobj prog;
	struct fb_info *framebuffer;
};

struct vc4_context *vc4_context_create(struct device *dev);
void vc4_program_init(struct vc4_context *vc4);
void vc4_flush(struct vc4_context *vc4);

#endif // VC4_CONTEXT
