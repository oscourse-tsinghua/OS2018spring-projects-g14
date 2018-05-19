#ifndef GL_CONTEXT_H
#define GL_CONTEXT_H

struct gl_context {
	void (*destroy)(struct gl_context *ctx);
	void (*draw_vbo)(struct gl_context *ctx);
	void (*clear)(struct gl_context *ctx, uint32_t color);
	void (*flush)(struct gl_context *ctx);
};

#endif // GL_CONTEXT_H
