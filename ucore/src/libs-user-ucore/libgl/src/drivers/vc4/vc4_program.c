#include <error.h>

#include "vc4_bufmgr.h"
#include "vc4_context.h"

static int vc4_create_fs(struct pipe_context *pctx)
{
	struct vc4_context *vc4 = vc4_context(pctx);

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

	struct vc4_bo *fs = vc4_bo_alloc(vc4, sizeof(ins));
	void *map = vc4_bo_map(fs);
	if (map == NULL) {
		return -E_NOMEM;
	}
	memcpy(map, ins, sizeof(ins));

	struct vc4_bo *uniforms = vc4_bo_alloc(vc4, 0x1000);
	if (uniforms == NULL) {
		return -E_NOMEM;
	}

	vc4->prog.fs = fs;
	vc4->uniforms = uniforms;

	return 0;
}

void vc4_program_init(struct pipe_context *pctx)
{
	pctx->create_fs_state = vc4_create_fs;
}
