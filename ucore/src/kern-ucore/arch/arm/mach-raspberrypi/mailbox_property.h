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

#ifndef MAILBOX_PROPERTY_H
#define MAILBOX_PROPERTY_H

unsigned mbox_mem_alloc(unsigned int size, unsigned align, unsigned flags);
unsigned mbox_mem_free(unsigned handle);
unsigned mbox_mem_lock(unsigned handle);
unsigned mbox_mem_unlock(unsigned handle);
void *mbox_mapmem(unsigned base, unsigned int size);
void mbox_unmapmem(void *addr, unsigned int size);

unsigned mbox_execute_code(unsigned code, unsigned r0, unsigned r1, unsigned r2,
			   unsigned r3, unsigned r4, unsigned r5);
unsigned mbox_execute_qpu(unsigned num_qpus, unsigned control, unsigned noflush,
			  unsigned timeout);
unsigned mbox_qpu_enable(unsigned enable);

// Flags for allocate memory.
enum MAILBOX_ALLOC_MEM_FLAGS {
	/* can be resized to 0 at any time. Use for cached data */
	MEM_FLAG_DISCARDABLE = 1 << 0,

	/* normal allocating alias. Don't use from ARM */
	MEM_FLAG_NORMAL = 0 << 2,

	/* 0xC alias uncached */
	MEM_FLAG_DIRECT = 1 << 2,

	/* 0x8 alias. Non-allocating in L2 but coherent */
	MEM_FLAG_COHERENT = 2 << 2,

	/* Allocating in L2 */
	MEM_FLAG_L1_NONALLOCATING = (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT),

	/* initialise buffer to all zeros */
	MEM_FLAG_ZERO = 1 << 4,

	/* don't initialise (default is initialise to all ones */
	MEM_FLAG_NO_INIT = 1 << 5,

	/* Likely to be locked for long periods of time. */
	MEM_FLAG_HINT_PERMALOCK = 1 << 6,
};

#endif // MAILBOX_PROPERTY_H
