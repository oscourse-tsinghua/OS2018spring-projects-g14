#ifndef VC4_CONTEXT
#define VC4_CONTEXT

#include <types.h>

#include "gl_context.h"
#include "vc4_cl.h"
#include "vc4_bufmgr.h"

struct vc4_program_stateobj {
	struct vc4_bo *vs, *fs;
};

struct vc4_framebuffer_state {
	uint32_t width, height;
	uint32_t bits_per_pixel;
	void *screen_base;
};

struct vc4_context {
	struct gl_context base;
	int fd;

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

	/* Bitmask of PIPE_CLEAR_* of buffers that were cleared before the
	 * first rendering.
	 */
	uint32_t cleared;
	/* Bitmask of PIPE_CLEAR_* of buffers that have been rendered to
	 * (either clears or draws).
	 */
	uint32_t resolve;
	uint32_t clear_color[2];
	uint32_t clear_depth; /**< 24-bit unorm depth */
	uint8_t clear_stencil;

	/**
	 * Set if some drawing (triangles, blits, or just a glClear()) has
	 * been done to the FBO, meaning that we need to
	 * DRM_IOCTL_VC4_SUBMIT_CL.
	 */
	bool needs_flush;

	struct vc4_program_stateobj prog;
	struct vc4_bo *uniforms;

	struct vc4_framebuffer_state framebuffer;
};

static inline struct vc4_context *vc4_context(struct gl_context *ctx)
{
	return (struct vc4_context *)ctx;
}

struct gl_context *vc4_context_create(int fd);
void vc4_draw_init(struct gl_context *ctx);
int vc4_program_init(struct vc4_context *vc4);
void vc4_emit_state(struct vc4_context *vc4);

void vc4_context_destroy(struct gl_context *ctx);
void vc4_flush(struct gl_context *ctx);

#endif // VC4_CONTEXT
