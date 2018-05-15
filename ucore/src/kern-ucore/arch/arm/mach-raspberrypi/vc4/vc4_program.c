#include "vc4_drv.h"
#include "vc4_context.h"

static struct vc4_bo *vc4_create_fs()
{
	static const uint32_t ins[] = {
		0x958e0dbf, 0xd1724823, /* mov r0, vary; mov r3.8d, 1.0 */
		0x818e7176, 0x40024821, /* fadd r0, r0, r5; mov r1, vary */
		0x818e7376, 0x10024862, /* fadd r1, r1, r5; mov r2, vary */
		0x819e7540, 0x114248a3, /* fadd r2, r2, r5; mov r3.8a, r0 */
		0x809e7009, 0x115049e3, /* nop; mov r3.8b, r1 */
		0x809e7012, 0x116049e3, /* nop; mov r3.8c, r2 */
		0x159e76c0, 0x30020ba7, /* mov tlbc, r3; nop; thrend */
		0x009e7000, 0x100009e7, /* nop; nop; nop */
		0x009e7000, 0x500009e7, /* nop; nop; sbdone */
	};

	struct vc4_bo *fs = vc4_bo_create(sizeof(ins), 1);
	memcpy(fs->vaddr, ins, sizeof(ins));

	return fs;
}

void vc4_program_init(struct vc4_context *vc4)
{
	vc4->prog.fs = vc4_create_fs();
}
