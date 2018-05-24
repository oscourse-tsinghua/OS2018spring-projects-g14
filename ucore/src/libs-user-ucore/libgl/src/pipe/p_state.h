#ifndef PIPE_STATE_H
#define PIPE_STATE_H

#include <types.h>

#include "p_defines.h"

struct pipe_viewport_state {
	float scale[3];
	float translate[3];
};

struct pipe_framebuffer_state {
	uint32_t width, height;
	uint32_t bits_per_pixel;
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

#endif // PIPE_STATE_H
