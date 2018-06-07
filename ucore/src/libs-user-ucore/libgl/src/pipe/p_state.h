#ifndef PIPE_STATE_H
#define PIPE_STATE_H

#include <types.h>

#include "p_defines.h"

struct pipe_viewport_state {
	float scale[3];
	float translate[3];
};

struct pipe_depth_state {
	unsigned enabled : 1; /**< depth test enabled? */
	unsigned writemask : 1; /**< allow depth buffer writes? */
	unsigned func : 3; /**< depth test func (PIPE_FUNC_x) */
	unsigned bounds_test : 1; /**< depth bounds test enabled? */
	float bounds_min; /**< minimum depth bound */
	float bounds_max; /**< maximum depth bound */
};

struct pipe_stencil_state {
	unsigned enabled : 1; /**< stencil[0]: stencil enabled, stencil[1]: two-side enabled */
	unsigned func : 3; /**< PIPE_FUNC_x */
	unsigned fail_op : 3; /**< PIPE_STENCIL_OP_x */
	unsigned zpass_op : 3; /**< PIPE_STENCIL_OP_x */
	unsigned zfail_op : 3; /**< PIPE_STENCIL_OP_x */
	unsigned valuemask : 8;
	unsigned writemask : 8;
};

struct pipe_alpha_state {
	unsigned enabled : 1;
	unsigned func : 3; /**< PIPE_FUNC_x */
	float ref_value; /**< reference value */
};

struct pipe_depth_stencil_alpha_state {
	struct pipe_depth_state depth;
	struct pipe_stencil_state stencil[2]; /**< [0] = front, [1] = back */
	struct pipe_alpha_state alpha;
};

struct pipe_framebuffer_state {
	uint32_t width, height;
	uint32_t bits_per_pixel;
	uint32_t offset;
	void *screen_base;
};

/**
 * A memory object/resource such as a vertex buffer or texture.
 */
struct pipe_resource {
	uint16_t array_size;
};

/**
 * A vertex buffer.
 */
struct pipe_vertex_buffer {
	uint16_t stride; /**< stride to same attrib in next vertex, in bytes */
	bool is_user_buffer;
	unsigned buffer_offset; /**< offset to start of data in buffer, in bytes */

	union {
		struct pipe_resource *resource; /**< the actual buffer */
		const void *user; /**< pointer to a user buffer */
	} buffer;
};

/**
 * Information to describe a draw_vbo call.
 */
struct pipe_draw_info {
	uint8_t index_size; /**< if 0, the draw is not indexed. */
	enum pipe_prim_type mode : 8; /**< the mode of the primitive */
	unsigned has_user_indices : 1; /**< if true, use index.user_buffer */

	unsigned start;
	unsigned count; /**< number of vertices */

	union {
		//   struct pipe_resource *resource;  /**< real buffer */
		const void *user; /**< pointer to a user buffer */
	} index;
};

struct pipe_clear_state {
	unsigned buffers;
	union pipe_color_union color;
	double depth;
	unsigned stencil;
};

struct pipe_vertex_array_state {
	uint8_t size;
	uint16_t stride;
	bool enabled;
	const void *pointer;
};

#endif // PIPE_STATE_H
