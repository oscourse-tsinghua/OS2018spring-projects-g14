#include "vc4_context.h"

void vc4_emit_state(struct vc4_context *vc4)
{
	uint32_t minx, miny, maxx, maxy;
	minx = 0;
	miny = 0;
	maxx = vc4->draw_width;
	maxy = vc4->draw_height;

	cl_u8(&vc4->bcl, VC4_PACKET_CLIP_WINDOW);
	cl_u16(&vc4->bcl, minx);
	cl_u16(&vc4->bcl, miny);
	cl_u16(&vc4->bcl, maxx - minx); // width
	cl_u16(&vc4->bcl, maxy - miny); // height

	cl_u8(&vc4->bcl, VC4_PACKET_CONFIGURATION_BITS);
	cl_u8(&vc4->bcl, 0x03); // enable both foward and back facing polygons
	cl_u8(&vc4->bcl, 0x00); // depth testing disabled
	cl_u8(&vc4->bcl, 0x02); // enable early depth write

	cl_u8(&vc4->bcl, VC4_PACKET_VIEWPORT_OFFSET);
	cl_u16(&vc4->bcl, 0);
	cl_u16(&vc4->bcl, 0);

	vc4->draw_min_x = minx;
	vc4->draw_min_y = miny;
	vc4->draw_max_x = maxx;
	vc4->draw_max_y = maxy;
}
