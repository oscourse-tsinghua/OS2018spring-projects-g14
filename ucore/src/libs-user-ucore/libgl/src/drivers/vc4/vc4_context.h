#ifndef VC4_CONTEXT
#define VC4_CONTEXT

#include <types.h>

#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "vc4_cl.h"
#include "vc4_bufmgr.h"

#define VC4_DIRTY_BLEND         (1 <<  0)
#define VC4_DIRTY_RASTERIZER    (1 <<  1)
#define VC4_DIRTY_ZSA           (1 <<  2)
#define VC4_DIRTY_FRAGTEX       (1 <<  3)
#define VC4_DIRTY_VERTTEX       (1 <<  4)

#define VC4_DIRTY_BLEND_COLOR   (1 <<  7)
#define VC4_DIRTY_STENCIL_REF   (1 <<  8)
#define VC4_DIRTY_SAMPLE_MASK   (1 <<  9)
#define VC4_DIRTY_FRAMEBUFFER   (1 << 10)
#define VC4_DIRTY_STIPPLE       (1 << 11)
#define VC4_DIRTY_VIEWPORT      (1 << 12)
#define VC4_DIRTY_CONSTBUF      (1 << 13)
#define VC4_DIRTY_VTXSTATE      (1 << 14)
#define VC4_DIRTY_VTXBUF        (1 << 15)

#define VC4_DIRTY_SCISSOR       (1 << 17)
#define VC4_DIRTY_FLAT_SHADE_FLAGS (1 << 18)
#define VC4_DIRTY_PRIM_MODE     (1 << 19)
#define VC4_DIRTY_CLIP          (1 << 20)
#define VC4_DIRTY_UNCOMPILED_VS (1 << 21)
#define VC4_DIRTY_UNCOMPILED_FS (1 << 22)
#define VC4_DIRTY_COMPILED_CS   (1 << 23)
#define VC4_DIRTY_COMPILED_VS   (1 << 24)
#define VC4_DIRTY_COMPILED_FS   (1 << 25)
#define VC4_DIRTY_FS_INPUTS     (1 << 26)

struct vc4_program_stateobj {
	struct vc4_bo *vs, *fs;
};

struct vc4_context {
	struct pipe_context base;
	int fd;

	struct vc4_cl bcl;
	struct vc4_cl shader_rec;
	struct vc4_cl bo_handles;
	struct vc4_cl bo_pointers;
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

	/** bitfield of VC4_DIRTY_* */
	uint32_t dirty;

	/** Maximum index buffer valid for the current shader_rec. */
	uint32_t max_index;

	struct vc4_program_stateobj prog;
	struct vc4_bo *uniforms;

	struct pipe_framebuffer_state framebuffer;

	struct pipe_viewport_state viewport;

	struct pipe_vertex_buffer vertexbuf;

	list_entry_t bo_list;
};

static inline struct vc4_context *vc4_context(struct pipe_context *pctx)
{
	return (struct vc4_context *)pctx;
}

void vc4_emit_state(struct vc4_context *vc4);

void vc4_draw_init(struct pipe_context *pctx);
void vc4_state_init(struct pipe_context *pctx);
void vc4_program_init(struct pipe_context *pctx);

void vc4_context_destroy(struct pipe_context *pctx);
void vc4_flush(struct pipe_context *pctx);

#endif // VC4_CONTEXT
