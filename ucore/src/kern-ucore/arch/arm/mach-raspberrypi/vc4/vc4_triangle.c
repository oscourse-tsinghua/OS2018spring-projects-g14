#include <unistd.h>
#include <arm.h>

#include "mailbox_property.h"
#include "framebuffer.h"
#include "vc4_regs.h"

// I/O access
volatile unsigned *v3d;

static void addbyte(uint8_t **list, uint8_t d)
{
	*((*list)++) = d;
}

static void addshort(uint8_t **list, uint16_t d)
{
	*((*list)++) = (d)&0xff;
	*((*list)++) = (d >> 8) & 0xff;
}

static void addword(uint8_t **list, uint32_t d)
{
	*((*list)++) = (d)&0xff;
	*((*list)++) = (d >> 8) & 0xff;
	*((*list)++) = (d >> 16) & 0xff;
	*((*list)++) = (d >> 24) & 0xff;
}

static void addfloat(uint8_t **list, float f)
{
	uint32_t d = *((uint32_t *)&f);
	*((*list)++) = (d)&0xff;
	*((*list)++) = (d >> 8) & 0xff;
	*((*list)++) = (d >> 16) & 0xff;
	*((*list)++) = (d >> 24) & 0xff;
}

static void drawTriangle(uint8_t **p, float r, float g, float b, float x1,
			 float y1, float z1, float x2, float y2, float z2,
			 float x3, float y3, int z3)
{
	addshort(p, ((int)x1) << 4); // X in 12.4 fixed point
	addshort(p, ((int)y1) << 4); // Y in 12.4 fixed point
	addfloat(p, z1); // Z
	addfloat(p, 1.0f); // 1/W
	addfloat(p, r); // Varying 0 (Red)
	addfloat(p, g); // Varying 1 (Green)
	addfloat(p, b); // Varying 2 (Blue)

	addshort(p, ((int)x2) << 4); // X in 12.4 fixed point
	addshort(p, ((int)y2) << 4); // Y in 12.4 fixed point
	addfloat(p, z2); // Z
	addfloat(p, 1.0f); // 1/W
	addfloat(p, r); // Varying 0 (Red)
	addfloat(p, g); // Varying 1 (Green)
	addfloat(p, b); // Varying 2 (Blue)

	addshort(p, ((int)x3) << 4); // X in 12.4 fixed point
	addshort(p, ((int)y3) << 4); // Y in 12.4 fixed point
	addfloat(p, z3); // Z
	addfloat(p, 1.0f); // 1/W
	addfloat(p, r); // Varying 0 (Red)
	addfloat(p, g); // Varying 1 (Green)
	addfloat(p, b); // Varying 2 (Blue)
}

// Render a single triangle to memory.
static void testTriangle()
{
#define BUFFER_VERTEX_INDEX 0x50
#define BUFFER_SHADER_OFFSET 0x80
#define BUFFER_VERTEX_DATA 0x100
#define BUFFER_TILE_STATE 0x300
#define BUFFER_TILE_DATA 0x6300
#define BUFFER_RENDER_CONTROL 0xe200
#define BUFFER_FRAGMENT_SHADER 0xfe00
#define BUFFER_FRAGMENT_UNIFORM 0xff00

	// Like above, we allocate/lock/map some videocore memory
	// I'm just shoving everything in a single buffer because I'm lazy
	// 8Mb, 4k alignment
	unsigned int handle = mbox_mem_alloc(0x800000, 0x1000,
					     MEM_FLAG_COHERENT | MEM_FLAG_ZERO);

	if (!handle) {
		kprintf("Error: Unable to allocate memory");
		return;
	}
	uint32_t bus_addr = mbox_mem_lock(handle);
	uint8_t *list = (uint8_t *)mbox_mapmem(bus_addr, 0x800000);

	uint8_t *p = list;

	uint32_t renderWth = 1920;
	uint32_t renderHt = 1080;
	uint32_t binWth = (renderWth + 63) / 64; // Tiles across
	uint32_t binHt = (renderHt + 63) / 64; // Tiles down

	// Configuration stuff
	// Tile Binning Configuration.
	//   Tile state data is 48 bytes per tile, I think it can be thrown away
	//   as soon as binning is finished.
	addbyte(&p, 112);
	addword(&p,
		bus_addr + BUFFER_TILE_DATA); // tile allocation memory address
	addword(&p, 0x8000); // tile allocation memory size
	addword(&p, bus_addr + BUFFER_TILE_STATE); // Tile state data address
	addbyte(&p, binWth); // 1920/64
	addbyte(&p, binHt); // 1080/64 (16.875)
	addbyte(&p, 0x04); // config

	addbyte(&p, 6);

	// Primitive type
	addbyte(&p, 56);
	addbyte(&p, 0x12); // 16 bit triangle

	// Clip Window
	addbyte(&p, 102);
	addshort(&p, 0);
	addshort(&p, 0);
	addshort(&p, renderWth); // width
	addshort(&p, renderHt); // height

	// State
	addbyte(&p, 96);
	addbyte(&p, 0x03); // enable both foward and back facing polygons
	addbyte(&p, 0x00); // depth testing disabled
	addbyte(&p, 0x02); // enable early depth write

	// Viewport offset
	addbyte(&p, 103);
	addshort(&p, 0);
	addshort(&p, 0);

	// The triangle
	// No Vertex Shader state (takes pre-transformed vertexes,
	// so we don't have to supply a working coordinate shader to test the binner.
	addbyte(&p, 65);
	addword(&p, bus_addr + BUFFER_SHADER_OFFSET); // Shader Record

	// primitive index list
	addbyte(&p, 32);
	addbyte(&p, 0x04); // 8bit index, trinagles
	addword(&p, 12); // Length
	addword(&p, bus_addr + BUFFER_VERTEX_INDEX); // address
	addword(&p, 16); // Maximum index

	// End of bin list
	// Flush
	addbyte(&p, 5);
	// Nop
	addbyte(&p, 1);
	// Halt
	addbyte(&p, 0);

	int length = p - list;

	// Shader Record
	p = list + BUFFER_SHADER_OFFSET;
	addbyte(&p, 0x01); // flags
	addbyte(&p, 6 * 4); // stride
	addbyte(&p, 0xcc); // num uniforms (not used)
	addbyte(&p, 3); // num varyings
	addword(&p, bus_addr + BUFFER_FRAGMENT_SHADER); // Fragment shader code
	addword(&p,
		bus_addr + BUFFER_FRAGMENT_UNIFORM); // Fragment shader uniforms

	unsigned int vertex_data_size = 12 * 24;
	unsigned int vertex_data_base = mbox_mem_alloc(
		vertex_data_size, 0x10, MEM_FLAG_COHERENT | MEM_FLAG_ZERO);
	vertex_data_base = mbox_mem_lock(vertex_data_base);
	addword(&p, vertex_data_base); // Vertex Data

	// Vertex Data
	p = (uint8_t *)mbox_mapmem(vertex_data_size, vertex_data_size);

	float sqrt3 = 1.7320508075688772f;
	float sqrt6 = 2.449489742783178;
	int size = 600;
	float x0 = (1920 / 2) - size / 2, y0 = 800, z0 = 1;
	float x1 = x0 + size / 2, y1 = y0 - sqrt3 / 2 * size, z1 = z0;
	float x2 = x0, y2 = y0, z2 = z0;
	float x3 = x0 + size, y3 = y0, z3 = z0;
	float x4 = x0 + size / 2, y4 = y0 - sqrt3 / 6 * size,
	      z4 = z0; // + sqrt6 / 3 * size;

	drawTriangle(&p, 1, 1, 1, x1, y1 - 10, z1, x2 - 10, y2 + 10, z2,
		     x3 + 10, y3 + 10, z3);

	drawTriangle(&p, 1, 0, 0, x4, y4, z4, x2, y2, z2, x1, y1, z1);

	drawTriangle(&p, 0, 0, 1, x4, y4, z4, x1, y1, z1, x3, y3, z3);

	drawTriangle(&p, 0, 1, 0, x4, y4, z4, x2, y2, z2, x3, y3, z3);

	// Vertex list
	p = list + BUFFER_VERTEX_INDEX;
	addbyte(&p, 0);
	addbyte(&p, 1);
	addbyte(&p, 2);
	addbyte(&p, 3);
	addbyte(&p, 4);
	addbyte(&p, 5);
	addbyte(&p, 6);
	addbyte(&p, 7);
	addbyte(&p, 8);
	addbyte(&p, 9);
	addbyte(&p, 10);
	addbyte(&p, 11);

	// fragment shader
	p = list + BUFFER_FRAGMENT_SHADER;
	addword(&p, 0x958e0dbf);
	addword(&p, 0xd1724823); /* mov r0, vary; mov r3.8d, 1.0 */
	addword(&p, 0x818e7176);
	addword(&p, 0x40024821); /* fadd r0, r0, r5; mov r1, vary */
	addword(&p, 0x818e7376);
	addword(&p, 0x10024862); /* fadd r1, r1, r5; mov r2, vary */
	addword(&p, 0x819e7540);
	addword(&p, 0x114248a3); /* fadd r2, r2, r5; mov r3.8a, r0 */
	addword(&p, 0x809e7009);
	addword(&p, 0x115049e3); /* nop; mov r3.8b, r1 */
	addword(&p, 0x809e7012);
	addword(&p, 0x116049e3); /* nop; mov r3.8c, r2 */
	addword(&p, 0x159e76c0);
	addword(&p, 0x30020ba7); /* mov tlbc, r3; nop; thrend */
	addword(&p, 0x009e7000);
	addword(&p, 0x100009e7); /* nop; nop; nop */
	addword(&p, 0x009e7000);
	addword(&p, 0x500009e7); /* nop; nop; sbdone */

	// Render control list
	p = list + BUFFER_RENDER_CONTROL;

	// Clear color
	addbyte(&p, 114);
	addword(&p, 0x282c34); // Opaque Black
	addword(&p, 0x282c34); // 32 bit clear colours need to be repeated twice
	addword(&p, 0);
	addbyte(&p, 0);

	// Tile Rendering Mode Configuration
	addbyte(&p, 113);
	addword(&p, raspberrypi_fb->paddr); // framebuffer addresss
	addshort(&p, renderWth); // width
	addshort(&p, renderHt); // height
	addbyte(&p, 0x04); // framebuffer mode (linear rgba8888)
	addbyte(&p, 0x00);

	// Do a store of the first tile to force the tile buffer to be cleared
	// Tile Coordinates
	addbyte(&p, 115);
	addbyte(&p, 0);
	addbyte(&p, 0);
	// Store Tile Buffer General
	addbyte(&p, 28);
	addshort(&p, 0); // Store nothing (just clear)
	addword(&p, 0); // no address is needed

	// Link all binned lists together
	int x, y;
	for (x = 0; x < binWth; x++) {
		for (y = 0; y < binHt; y++) {
			// Tile Coordinates
			addbyte(&p, 115);
			addbyte(&p, x);
			addbyte(&p, y);

			// Call Tile sublist
			addbyte(&p, 17);
			addword(&p, bus_addr + BUFFER_TILE_DATA +
					    (y * binWth + x) * 32);

			// Last tile needs a special store instruction
			if (x == binWth - 1 && y == binHt - 1) {
				// Store resolved tile color buffer and signal end of frame
				addbyte(&p, 25);
			} else {
				// Store resolved tile color buffer
				addbyte(&p, 24);
			}
		}
	}

	int render_length = p - (list + BUFFER_RENDER_CONTROL);

	// clear caches
	v3d[V3D_L2CACTL] = 4;
	v3d[V3D_SLCACTL] = 0x0F0F0F0F;

	// stop the thread
	v3d[V3D_CT0CS] = 0x20;
	// Wait for control list to execute
	while (v3d[V3D_CT0CS] & 0x20)
		;

	// Run our control list
	kprintf("Binner control list constructed\n");
	kprintf("Start Address: 0x%08x, length: 0x%x\n", bus_addr, length);

	v3d[V3D_BFC] = 1; // reset binning frame count
	v3d[V3D_CT0CA] = bus_addr;
	v3d[V3D_CT0EA] = bus_addr + length;
	kprintf("V3D_CT0CS: 0x%08x, Address: 0x%08x\n", v3d[V3D_CT0CS],
		v3d[V3D_CT0CA]);

	// wait for binning to finish
	while (v3d[V3D_BFC] == 0)
		;
	kprintf("V3D_CT0CS: 0x%08x, Address: 0x%08x\n", v3d[V3D_CT0CS],
		v3d[V3D_CT0CA]);

	// stop the thread
	v3d[V3D_CT1CS] = 0x20;
	// Wait for control list to execute
	while (v3d[V3D_CT1CS] & 0x20)
		;

	kprintf("Start Address: 0x%08x, length: 0x%x\n",
		bus_addr + BUFFER_RENDER_CONTROL, render_length);

	v3d[V3D_RFC] = 1; // reset rendering frame count
	v3d[V3D_CT1CA] = bus_addr + BUFFER_RENDER_CONTROL;
	v3d[V3D_CT1EA] = bus_addr + BUFFER_RENDER_CONTROL + render_length;
	kprintf("V3D_CT1CS: 0x%08x, Address: 0x%08x\n", v3d[V3D_CT1CS],
		v3d[V3D_CT1CA]);

	// wait for render to finish
	while (v3d[V3D_RFC] == 0)
		;
	kprintf("V3D_CT1CS: 0x%08x, Address: 0x%08x\n", v3d[V3D_CT1CS],
		v3d[V3D_CT1CA]);

	// Release resources
	mbox_unmapmem((void *)list, 0x800000);
	mbox_mem_unlock(handle);
	mbox_mem_free(handle);
	mbox_mem_unlock(vertex_data_base);
	mbox_mem_free(vertex_data_base);
}

void vc4_hello_triangle()
{
	kprintf("Hello VC4 triangle!\n");

	// The blob now has this nice handy call which powers up the v3d pipeline.
	int x = mbox_qpu_enable(1);

	// map v3d's registers into our address space.
	v3d = (unsigned *)0x20c00000;

	if (v3d[V3D_IDENT0] != 0x02443356) { // Magic number.
		kprintf("Error: V3D pipeline isn't powered up and accessable.\n");
	}

	// We now have access to the v3d registers, we should do something.
	testTriangle();
}
