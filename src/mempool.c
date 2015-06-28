/*
 *   mempool.c
 *   splitstream - Stream object splitter
 *
 *   Copyright © 2015 Rickard Lyrenius
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

/* This file implements a small object memory pool. It allocates a block of quanta (each
   quantum is a power of 2, default 256 bytes). The size of the block is
   
      quantumsize * sizeof(long) * 8
   
   which is 8k for 32 bit processes and 16k for 64 bit processes.
   
   Memory is allocated by setting a bitmask indicating of the memory quantum is used. If
   the amount of allocated memory does not fit within a memory pool, a child pool is 
   allocated recursively (which means the memory pool grows indefinitely). The memory
   pool never shrinks.
   
   If a (re)allocation does not fit within a block, that allocation will be made outside
   of the memory pool (using normal malloc/realloc).
   
   When a memory pool is destroyed, all memory allocated from it should have been freed.
   If this is not true, an exception is raised.
   
   Some useful properties for this usecase:
   * A small allocation is usually just a matter of flipping a few bits in the bitmask.
   * A reallocation on the latest allocation is just a matter of flipping bits in the bitmask.
   * A reallocation that fits within a quantum is a no-op.
   
 */

#include <stdlib.h>
#include <string.h>

/* Allocation chunk size, in powers of 2 */
#define MEMPOOL_QUANTUM_POWER 		8

#define MEMPOOL_BLOCK_QUANTUMS		(8 * sizeof(size_t))
#define MEMPOOL_BLOCKSIZE			(MEMPOOL_BLOCK_QUANTUMS) * (1<<(MEMPOOL_QUANTUM_POWER))

/* Define DISABLE_MEMPOOL to disable the memory pool entirely and rely on malloc/free.
   This will lower performance by 60-100% in many cases, especially in the cases of 
   many small documents in one large stream and huge documents with small read buffers.
   It may, however, reduce memory footprint (not tested). The option was introduced to
   allow experimenting with allocators, such as tcmalloc or special cases for embedded
   targets with even tighter memory constraints. But for now, the memory pool is doing
   a decent job. */
/* #define DISABLE_MEMPOOL */

static size_t MEMPOOL_BLOCK_QUANTIFY(size_t x) {
	size_t bs = (1<<MEMPOOL_QUANTUM_POWER) * MEMPOOL_BLOCK_QUANTUMS;
	int n = (int)((x + bs - 1) / bs);
	return n * MEMPOOL_BLOCKSIZE;
}

#define MEMPOOL_BITMASK(ct, offset) \
	( ((ct == 8 * sizeof(size_t)) ? (size_t)-1 : \
	((((size_t)1) << (size_t)(ct)) - (size_t)1)) \
	 << (size_t)(offset))
	
#define MEMPOOL_QUANTUMS(size) \
	((((size_t)(size)) + ((size_t)1 << MEMPOOL_QUANTUM_POWER) - 1) >> MEMPOOL_QUANTUM_POWER)

struct mempool
{
    unsigned char* data;
    unsigned long bitmask;
    struct mempool* next;
};

struct mempool* mempool_New(void)
{
#ifdef DISABLE_MEMPOOL
	return NULL;
#else
	struct mempool* pool = malloc(sizeof(struct mempool));
	if(!pool) return NULL;
	pool->data = malloc(MEMPOOL_BLOCKSIZE);
	if(!pool->data) return NULL;
	pool->next = NULL;
	pool->bitmask = 0;
	return pool;
#endif
}

void mempool_Destroy(struct mempool* pool, int check)
{
#ifndef DISABLE_MEMPOOL
	if(pool->next) mempool_Destroy(pool->next, check);
	//if(check && pool->bitmask) abort();
	if(pool->data) free(pool->data);
    free(pool);
#endif
}

void* mempool_Alloc(struct mempool* pool, size_t size)
{
#ifdef DISABLE_MEMPOOL
	return malloc(size);
#else
	int blocksNeeded, bitsRem, bit;
	unsigned long mask;

    if(size <= 1) size = (1 << MEMPOOL_QUANTUM_POWER);
    blocksNeeded = (int)MEMPOOL_QUANTUMS(size);

	if(blocksNeeded > MEMPOOL_BLOCK_QUANTUMS) {
		return malloc(MEMPOOL_BLOCK_QUANTIFY(size));
	}

	mask = (1 << blocksNeeded) - 1;

	bitsRem = MEMPOOL_BLOCK_QUANTUMS - blocksNeeded;
	for(bit = 0; bit <= bitsRem; ++bit) {
		if(((pool->bitmask >> bit) & mask) == 0) {
			pool->bitmask |= mask << bit;
			void* pd = pool->data + (1 << MEMPOOL_QUANTUM_POWER) * bit;
			return pd;
		}
	}
	if(!pool->next) {
		pool->next = mempool_New();
		if(!pool->next) return NULL;
	}
	return mempool_Alloc(pool->next, size);
#endif
}

void mempool_Free(struct mempool* pool, void* ptr, size_t size)
{
#ifdef DISABLE_MEMPOOL
	free(ptr);
#else
	unsigned char* p = ptr, *end = pool->data + MEMPOOL_BLOCKSIZE;
	if(p >= pool->data && p < end) {
		int blocksNeeded, offset;
    	size_t mask;
    	if(size <= 1) size = (1 << MEMPOOL_QUANTUM_POWER);
	    blocksNeeded = (int)((size - 1) >> (MEMPOOL_QUANTUM_POWER)) + 1;
		offset = (int)((p - pool->data) >> MEMPOOL_QUANTUM_POWER);
		mask = MEMPOOL_BITMASK(blocksNeeded, offset);
		pool->bitmask &= ~mask;
	} else if(pool->next) {
		mempool_Free(pool->next, ptr, size);
	} else {
		free(ptr);
	}
#endif
}

#ifndef DISABLE_MEMPOOL

static unsigned char* mempool_ReAlloc_internal(struct mempool* pool, struct mempool* root, unsigned char* ptr, size_t oldSize, size_t newSize)
{
	unsigned char* end = pool->data + MEMPOOL_BLOCKSIZE;
	if(ptr >= pool->data && ptr < end) {
		void* newptr;
		int oldNeeded, newNeeded, offset;
		
	    if(oldSize <= 1) oldSize = (1 << MEMPOOL_QUANTUM_POWER);
    	if(newSize <= 1) newSize = (1 << MEMPOOL_QUANTUM_POWER);
	    oldNeeded = (int)MEMPOOL_QUANTUMS(oldSize);
    	newNeeded = (int)MEMPOOL_QUANTUMS(newSize);
	    if(newNeeded <= oldNeeded) return ptr; /* Never shrink */
	    
	    offset = (int)((ptr - pool->data) >> MEMPOOL_QUANTUM_POWER);
	    if(offset + newNeeded <= MEMPOOL_BLOCK_QUANTUMS) {
		    size_t maskDiff = MEMPOOL_BITMASK(newNeeded, offset) & ~MEMPOOL_BITMASK(oldNeeded, offset);
		    if(!(pool->bitmask & maskDiff)) {
		    	pool->bitmask |= maskDiff;
		    	return ptr;
			}
	    }
	    
    	newptr = mempool_Alloc(root, newSize);
	    if(!newptr) return NULL;
    	memcpy(newptr, ptr, oldSize);
	    mempool_Free(pool, ptr, oldSize);
    	return newptr;
	} else if(pool->next) {
		return mempool_ReAlloc_internal(pool->next, root, ptr, oldSize, newSize);
	} else {
		newSize = MEMPOOL_BLOCK_QUANTIFY(newSize);
		oldSize = MEMPOOL_BLOCK_QUANTIFY(oldSize);
		if(newSize == oldSize) return ptr;
		return realloc(ptr, newSize);
	}
}

#endif

void* mempool_ReAlloc(struct mempool* pool, void* ptr, size_t oldSize, size_t newSize)
{
#ifdef DISABLE_MEMPOOL
	return realloc(ptr, newSize);
#else
	return mempool_ReAlloc_internal(pool, pool, (unsigned char*)ptr, oldSize, newSize);
#endif
}
