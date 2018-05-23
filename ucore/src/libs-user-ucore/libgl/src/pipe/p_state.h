#ifndef PIPE_STATE_H
#define PIPE_STATE_H

#include <types.h>

struct pipe_viewport_state {
	float scale[3];
	float translate[3];
};

struct pipe_framebuffer_state {
	uint32_t width, height;
	uint32_t bits_per_pixel;
	void *screen_base;
};

#endif // PIPE_STATE_H
