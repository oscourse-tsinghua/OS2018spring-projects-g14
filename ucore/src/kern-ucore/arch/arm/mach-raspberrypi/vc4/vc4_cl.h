#ifndef VC4_CL_H
#define VC4_CL_H

#include <types.h>

struct vc4_cl {
	uint32_t paddr;
	void *vaddr;
	void *next;
	size_t size;
};

void vc4_init_cl(struct vc4_cl *cl, uint32_t paddr, void *vaddr, size_t size);
void vc4_reset_cl(struct vc4_cl *cl);
void vc4_dump_cl(void *cl, size_t size, size_t cols, const char *name);

struct __attribute__((__packed__)) unaligned_16 { uint16_t x; };
struct __attribute__((__packed__)) unaligned_32 { uint32_t x; };

static inline uint32_t cl_offset(struct vc4_cl *cl)
{
	return (char *)cl->next - (char *)cl->vaddr;
}

static inline void cl_advance(struct vc4_cl *cl, uint32_t n)
{
	cl->next += n;
}

static inline void put_unaligned_32(void *ptr, uint32_t val)
{
	struct unaligned_32 *p = (void *)ptr;
	p->x = val;
}

static inline void put_unaligned_16(void *ptr, uint16_t val)
{
	struct unaligned_16 *p = (void *)ptr;
	p->x = val;
}

static inline uint32_t get_unaligned_32(void *ptr)
{
	struct unaligned_32 *p = (void *)ptr;
	return p->x;
}

static inline uint16_t get_unaligned_16(void *ptr)
{
	struct unaligned_16 *p = (void *)ptr;
	return p->x;
}

static inline void cl_u8(struct vc4_cl *cl, uint8_t n)
{
	*(uint8_t *)cl->next = n;
	cl_advance(cl, 1);
}

static inline void cl_u16(struct vc4_cl *cl, uint16_t n)
{
	put_unaligned_16(cl->next, n);
	cl_advance(cl, 2);
}

static inline void cl_u32(struct vc4_cl *cl, uint32_t n)
{
	put_unaligned_32(cl->next, n);
	cl_advance(cl, 4);
}

static inline void cl_aligned_u32(struct vc4_cl *cl, uint32_t n)
{
	*(uint32_t *)cl->next = n;
	cl_advance(cl, 4);
}

static inline void cl_f(struct vc4_cl *cl, float f)
{
	cl_u32(cl, *((uint32_t *)&f));
}

static inline void cl_aligned_f(struct vc4_cl *cl, float f)
{
	cl_aligned_u32(cl, *((uint32_t *)&f));
}

#endif /* VC4_CL_H */
