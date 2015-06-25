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
 
 #include <stdlib.h>
#include <string.h>

/* Allocation chunk size, in powers of 2 */
#define MEMPOOL_QUANTUM_POWER 		8

#define MEMPOOL_BLOCK_QUANTUMS		(8 * sizeof(unsigned long))
#define MEMPOOL_BLOCKSIZE			(MEMPOOL_BLOCK_QUANTUMS) * (1<<(MEMPOOL_QUANTUM_POWER))

struct mempool
{
    unsigned char* data;
    unsigned long bitmask;
    struct mempool* next;
};

struct mempool* mempool_New()
{
	struct mempool* pool = malloc(sizeof(struct mempool));
	if(!pool) return NULL;
	pool->data = malloc(MEMPOOL_BLOCKSIZE);
	if(!pool->data) return NULL;
	pool->next = NULL;
	pool->bitmask = 0;
	return pool;
}

void mempool_Destroy(struct mempool* pool, int check)
{
	if(pool->next) mempool_Destroy(pool->next, check);
	if(check && pool->bitmask) abort();
	if(pool->data) free(pool->data);
    free(pool);
}

void* mempool_Alloc(struct mempool* pool, size_t size)
{
	int blocksNeeded, bitsRem;
	unsigned long mask;
	
    if(size <= 1) size = (1 << MEMPOOL_QUANTUM_POWER);
    blocksNeeded = (int)((size - 1) >> (MEMPOOL_QUANTUM_POWER)) + 1;
    
	if(blocksNeeded > MEMPOOL_BLOCK_QUANTUMS) return malloc(size);
	
	mask = (1 << blocksNeeded) - 1;
    
	bitsRem = MEMPOOL_BLOCK_QUANTUMS - blocksNeeded;
	for(int bit = 0; bit <= MEMPOOL_BLOCK_QUANTUMS; ++bit) {
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
}

void mempool_Free(struct mempool* pool, void* ptr, size_t size)
{
	unsigned char* p = ptr, *end = pool->data + MEMPOOL_BLOCKSIZE;
	if(p >= pool->data && p < end) {
    	unsigned long mask;
    	if(size <= 1) size = (1 << MEMPOOL_QUANTUM_POWER);
	    int blocksNeeded = (int)((size - 1) >> (MEMPOOL_QUANTUM_POWER)) + 1;
		mask = (1 << blocksNeeded) - 1;
		int offset = (int)((p - pool->data) >> MEMPOOL_QUANTUM_POWER);
		pool->bitmask &= ~(mask << offset);
	} else if(pool->next) {
		mempool_Free(pool->next, ptr, size);
	} else {
		free(ptr);
	}
}

void* mempool_ReAlloc(struct mempool* pool, void* ptr, size_t oldSize, size_t newSize)
{
	void* newptr;
	int oldNeeded, newNeeded;
    if(oldSize <= 1) oldSize = (1 << MEMPOOL_QUANTUM_POWER);
    if(newSize <= 1) newSize = (1 << MEMPOOL_QUANTUM_POWER);
    oldNeeded = (int)((oldSize - 1) >> (MEMPOOL_QUANTUM_POWER - 1));
    newNeeded = (int)((newSize - 1) >> (MEMPOOL_QUANTUM_POWER - 1));
    if(newNeeded <= oldNeeded) return ptr;
    
    newptr = mempool_Alloc(pool, newSize);
    if(!newptr) return NULL;
    memcpy(newptr, ptr, oldSize);
    mempool_Free(pool, ptr, oldSize);
    return newptr;
}
