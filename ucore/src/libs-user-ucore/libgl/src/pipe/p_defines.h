#ifndef PIPE_DEFINES_H
#define PIPE_DEFINES_H

/**
 * Inequality functions.  Used for depth test, stencil compare, alpha
 * test, shadow compare, etc.
 */
#define PIPE_FUNC_NEVER    0
#define PIPE_FUNC_LESS     1
#define PIPE_FUNC_EQUAL    2
#define PIPE_FUNC_LEQUAL   3
#define PIPE_FUNC_GREATER  4
#define PIPE_FUNC_NOTEQUAL 5
#define PIPE_FUNC_GEQUAL   6
#define PIPE_FUNC_ALWAYS   7

/**
 * Clear buffer bits
 */
#define PIPE_CLEAR_DEPTH        (1 << 0)
#define PIPE_CLEAR_STENCIL      (1 << 1)
#define PIPE_CLEAR_COLOR0       (1 << 2)
#define PIPE_CLEAR_COLOR1       (1 << 3)
#define PIPE_CLEAR_COLOR2       (1 << 4)
#define PIPE_CLEAR_COLOR3       (1 << 5)
#define PIPE_CLEAR_COLOR4       (1 << 6)
#define PIPE_CLEAR_COLOR5       (1 << 7)
#define PIPE_CLEAR_COLOR6       (1 << 8)
#define PIPE_CLEAR_COLOR7       (1 << 9)
/** Combined flags */
/** All color buffers currently bound */
#define PIPE_CLEAR_COLOR        (PIPE_CLEAR_COLOR0 | PIPE_CLEAR_COLOR1 | \
                                 PIPE_CLEAR_COLOR2 | PIPE_CLEAR_COLOR3 | \
                                 PIPE_CLEAR_COLOR4 | PIPE_CLEAR_COLOR5 | \
                                 PIPE_CLEAR_COLOR6 | PIPE_CLEAR_COLOR7)
#define PIPE_CLEAR_DEPTHSTENCIL (PIPE_CLEAR_DEPTH | PIPE_CLEAR_STENCIL)

struct nv_shaded_vertex {
	uint16_t x, y;
	float z, rhw;
	float r, g, b;
};

union pipe_color_union {
	float f[4];
	int i[4];
	unsigned int ui[4];
};

/**
 * Primitive types:
 */
enum pipe_prim_type {
   PIPE_PRIM_POINTS,
   PIPE_PRIM_LINES,
   PIPE_PRIM_LINE_LOOP,
   PIPE_PRIM_LINE_STRIP,
   PIPE_PRIM_TRIANGLES,
   PIPE_PRIM_TRIANGLE_STRIP,
   PIPE_PRIM_TRIANGLE_FAN,
   PIPE_PRIM_QUADS,
   PIPE_PRIM_QUAD_STRIP,
   PIPE_PRIM_POLYGON,
   PIPE_PRIM_LINES_ADJACENCY,
   PIPE_PRIM_LINE_STRIP_ADJACENCY,
   PIPE_PRIM_TRIANGLES_ADJACENCY,
   PIPE_PRIM_TRIANGLE_STRIP_ADJACENCY,
   PIPE_PRIM_PATCHES,
   PIPE_PRIM_MAX,
};

#endif
