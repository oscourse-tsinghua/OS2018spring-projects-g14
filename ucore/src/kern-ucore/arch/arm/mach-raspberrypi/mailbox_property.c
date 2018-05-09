/*
Copyright (c) 2012, Broadcom Europe Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "mailbox.h"
#include "mailbox_property.h"

#define PAGE_SIZE (4 * 1024)

void *mbox_mapmem(unsigned int base, unsigned int size)
{
	// FIXME
	kprintf("mapmem before: %x\n", base);

	unsigned offset = base % PAGE_SIZE;
	base = base - offset;
	base = __ucore_ioremap(base, size, 0);

	kprintf("mapmem after: %x\n", base);
	return (char *)base + offset;
}

void mbox_unmapmem(void *addr, unsigned int size)
{
	// FIXME unimplemented
}

static void mbox_property(void *buf)
{
	volatile unsigned int mailbuffer[32] __attribute__((aligned(16)));
	memcpy(mailbuffer, buf, sizeof(mailbuffer));

	writemailbox(8, (unsigned int)mailbuffer);
}

unsigned int mbox_mem_alloc(unsigned int size, unsigned int align,
			    unsigned int flags)
{
	int i = 0;
	unsigned int p[32];
	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request

	p[i++] = 0x3000c; // (the tag id)
	p[i++] = 12; // (size of the buffer)
	p[i++] = 12; // (size of the data)
	p[i++] = size; // (num bytes? or pages?)
	p[i++] = align; // (alignment)
	p[i++] = flags; // (MEM_FLAG_L1_NONALLOCATING)

	p[i++] = 0x00000000; // end tag
	p[0] = i * sizeof *p; // actual size

	mbox_property(p);
	return p[5];
}

unsigned int mbox_mem_free(unsigned int handle)
{
	int i = 0;
	unsigned p[32];
	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request

	p[i++] = 0x3000f; // (the tag id)
	p[i++] = 4; // (size of the buffer)
	p[i++] = 4; // (size of the data)
	p[i++] = handle;

	p[i++] = 0x00000000; // end tag
	p[0] = i * sizeof *p; // actual size

	mbox_property(p);
	return p[5];
}

unsigned int mbox_mem_lock(unsigned int handle)
{
	int i = 0;
	unsigned int p[32];
	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request

	p[i++] = 0x3000d; // (the tag id)
	p[i++] = 4; // (size of the buffer)
	p[i++] = 4; // (size of the data)
	p[i++] = handle;

	p[i++] = 0x00000000; // end tag
	p[0] = i * sizeof *p; // actual size

	mbox_property(p);
	return p[5];
}

unsigned int mbox_mem_unlock(unsigned int handle)
{
	int i = 0;
	unsigned int p[32];
	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request

	p[i++] = 0x3000e; // (the tag id)
	p[i++] = 4; // (size of the buffer)
	p[i++] = 4; // (size of the data)
	p[i++] = handle;

	p[i++] = 0x00000000; // end tag
	p[0] = i * sizeof *p; // actual size

	mbox_property(p);
	return p[5];
}

unsigned int mbox_execute_code(unsigned int code, unsigned int r0,
			       unsigned int r1, unsigned int r2,
			       unsigned int r3, unsigned int r4,
			       unsigned int r5)
{
	int i = 0;
	unsigned int p[32];
	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request

	p[i++] = 0x30010; // (the tag id)
	p[i++] = 28; // (size of the buffer)
	p[i++] = 28; // (size of the data)
	p[i++] = code;
	p[i++] = r0;
	p[i++] = r1;
	p[i++] = r2;
	p[i++] = r3;
	p[i++] = r4;
	p[i++] = r5;

	p[i++] = 0x00000000; // end tag
	p[0] = i * sizeof *p; // actual size

	mbox_property(p);
	return p[5];
}

unsigned int mbox_qpu_enable(unsigned int enable)
{
	int i = 0;
	unsigned p[32];

	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request

	p[i++] = 0x30012; // (the tag id)
	p[i++] = 4; // (size of the buffer)
	p[i++] = 4; // (size of the data)
	p[i++] = enable;

	p[i++] = 0x00000000; // end tag
	p[0] = i * sizeof *p; // actual size

	mbox_property(p);
	return p[5];
}

unsigned int mbox_execute_qpu(unsigned int num_qpus, unsigned int control,
			      unsigned int noflush, unsigned int timeout)
{
	int i = 0;
	unsigned int p[32];

	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request
	p[i++] = 0x30011; // (the tag id)
	p[i++] = 16; // (size of the buffer)
	p[i++] = 16; // (size of the data)
	p[i++] = num_qpus;
	p[i++] = control;
	p[i++] = noflush;
	p[i++] = timeout; // ms

	p[i++] = 0x00000000; // end tag
	p[0] = i * sizeof *p; // actual size

	mbox_property(p);
	return p[5];
}
