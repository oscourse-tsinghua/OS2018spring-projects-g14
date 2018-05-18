#ifndef VC4_CONTEXT
#define VC4_CONTEXT

#include <types.h>

#include "vc4_cl.h"
#include "vc4_bufmgr.h"
#include "bcm2708_fb.h"

struct vc4_program_stateobj {
	struct vc4_bo *vs, *fs;
};

struct vc4_context {
	struct vc4_cl bcl;
	struct vc4_cl shader_rec;
	struct vc4_cl bo_handles;
	uint32_t shader_rec_count;

	/** @{
	 * Bounding box of the scissor across all queued drawing.
	 *
	 * Note that the max values are exclusive.
	 */
	uint32_t draw_min_x;
	uint32_t draw_min_y;
	uint32_t draw_max_x;
	uint32_t draw_max_y;
	/** @} */
	/** @{
	 * Width/height of the color framebuffer being rendered to,
	 * for VC4_TILE_RENDERING_MODE_CONFIG.
	*/
	uint32_t draw_width;
	uint32_t draw_height;
	/** @} */
	/** @{ Tile information, depending on MSAA and float color buffer. */
	uint32_t draw_tiles_x; /** @< Number of tiles wide for framebuffer. */
	uint32_t draw_tiles_y; /** @< Number of tiles high for framebuffer. */

	uint32_t tile_width; /** @< Width of a tile. */
	uint32_t tile_height; /** @< Height of a tile. */

	/**
	 * Set if some drawing (triangles, blits, or just a glClear()) has
	 * been done to the FBO, meaning that we need to
	 * DRM_IOCTL_VC4_SUBMIT_CL.
	 */
	bool needs_flush;

	struct vc4_program_stateobj prog;
	struct vc4_bo *uniforms;

	struct fb_info *framebuffer;
};

struct vc4_context *vc4_context_create(struct device *dev);
void vc4_program_init(struct vc4_context *vc4);
void vc4_flush(struct vc4_context *vc4);
void vc4_emit_state(struct vc4_context *vc4);

#endif // VC4_CONTEXT
