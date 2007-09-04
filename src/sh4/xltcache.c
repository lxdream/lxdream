/**
 * $Id: xltcache.c,v 1.2 2007-09-04 08:32:44 nkeynes Exp $
 * 
 * Translation cache management. This part is architecture independent.
 *
 * Copyright (c) 2005 Nathan Keynes.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sh4/xltcache.h"
#include "dreamcast.h"
#include <sys/mman.h>
#include <assert.h>

#define XLAT_LUT_PAGE_BITS 12
#define XLAT_LUT_TOTAL_BITS 28
#define XLAT_LUT_PAGE(addr) (((addr)>>13) & 0xFFFF)
#define XLAT_LUT_ENTRY(addr) (((addr)&0x1FFE) >> 1)

#define XLAT_LUT_PAGES (1<<(XLAT_LUT_TOTAL_BITS-XLAT_LUT_PAGE_BITS))
#define XLAT_LUT_PAGE_ENTRIES (1<<XLAT_LUT_PAGE_BITS)
#define XLAT_LUT_PAGE_SIZE (XLAT_LUT_PAGE_ENTRIES * sizeof(void *))

#define XLAT_LUT_ENTRY_EMPTY (void *)0
#define XLAT_LUT_ENTRY_USED  (void *)1

#define NEXT(block) ( (xlat_cache_block_t)&((block)->code[(block)->size]))
#define BLOCK_FOR_CODE(code) (((xlat_cache_block_t)code)-1)
#define IS_ENTRY_POINT(ent) (ent > XLAT_LUT_ENTRY_USED)
#define IS_ENTRY_USED(ent) (ent != XLAT_LUT_ENTRY_EMPTY)

#define MIN_BLOCK_SIZE 32
#define MIN_TOTAL_SIZE (sizeof(struct xlat_cache_block)+MIN_BLOCK_SIZE)

#define BLOCK_INACTIVE 0
#define BLOCK_ACTIVE 1
#define BLOCK_USED 2

xlat_cache_block_t xlat_new_cache;
xlat_cache_block_t xlat_new_cache_ptr;
xlat_cache_block_t xlat_new_create_ptr;
xlat_cache_block_t xlat_temp_cache;
xlat_cache_block_t xlat_temp_cache_ptr;
xlat_cache_block_t xlat_old_cache;
xlat_cache_block_t xlat_old_cache_ptr;
static void ***xlat_lut;
static void **xlat_lut2; /* second-tier page info */

void xlat_cache_init() 
{
    xlat_new_cache = mmap( NULL, XLAT_NEW_CACHE_SIZE, PROT_EXEC|PROT_READ|PROT_WRITE,
			   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );
    xlat_temp_cache = mmap( NULL, XLAT_TEMP_CACHE_SIZE, PROT_EXEC|PROT_READ|PROT_WRITE,
			   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );
    xlat_old_cache = mmap( NULL, XLAT_OLD_CACHE_SIZE, PROT_EXEC|PROT_READ|PROT_WRITE,
			   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );
    xlat_new_cache_ptr = xlat_new_cache;
    xlat_temp_cache_ptr = xlat_temp_cache;
    xlat_old_cache_ptr = xlat_old_cache;
    xlat_new_create_ptr = xlat_new_cache;
    
    xlat_lut = mmap( NULL, XLAT_LUT_PAGES*sizeof(void *), PROT_READ|PROT_WRITE,
		     MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    memset( xlat_lut, 0, XLAT_LUT_PAGES*sizeof(void *) );

    xlat_flush_cache();
}

/**
 * Reset the cache structure to its default state
 */
void xlat_flush_cache() 
{
    xlat_cache_block_t tmp;
    int i;
    xlat_new_cache_ptr = xlat_new_cache;
    xlat_new_cache_ptr->active = 0;
    xlat_new_cache_ptr->size = XLAT_NEW_CACHE_SIZE - 2*sizeof(struct xlat_cache_block);
    tmp = NEXT(xlat_new_cache_ptr);
    tmp->active = 1;
    tmp->size = 0;
    xlat_temp_cache_ptr = xlat_temp_cache;
    xlat_temp_cache_ptr->active = 0;
    xlat_temp_cache_ptr->size = XLAT_TEMP_CACHE_SIZE - 2*sizeof(struct xlat_cache_block);
    tmp = NEXT(xlat_temp_cache_ptr);
    tmp->active = 1;
    tmp->size = 0;
    xlat_old_cache_ptr = xlat_old_cache;
    xlat_old_cache_ptr->active = 0;
    xlat_old_cache_ptr->size = XLAT_OLD_CACHE_SIZE - 2*sizeof(struct xlat_cache_block);
    tmp = NEXT(xlat_old_cache_ptr);
    tmp->active = 1;
    tmp->size = 0;
    for( i=0; i<XLAT_LUT_PAGES; i++ ) {
	if( xlat_lut[i] != NULL ) {
	    memset( xlat_lut[i], 0, XLAT_LUT_PAGE_SIZE );
	}
    }
}

void xlat_flush_page( sh4addr_t address )
{
    int i;
    void **page = xlat_lut[XLAT_LUT_PAGE(address)];
    for( i=0; i<XLAT_LUT_PAGE_ENTRIES; i++ ) {
	if( IS_ENTRY_POINT(page[i]) ) {
	    BLOCK_FOR_CODE(page[i])->active = 0;
	}
	page[i] = NULL;
    }
}

void *xlat_get_code( sh4addr_t address )
{
    void **page = xlat_lut[XLAT_LUT_PAGE(address)];
    if( page == NULL ) {
	return NULL;
    }
    return page[XLAT_LUT_ENTRY(address)];
}

uint32_t xlat_get_block_size( void *block )
{
    xlat_cache_block_t xlt = (xlat_cache_block_t)(((char *)block)-sizeof(struct xlat_cache_block));
    return xlt->size;
}

/**
 * Cut the specified block so that it has the given size, with the remaining data
 * forming a new free block. If the free block would be less than the minimum size,
 * the cut is not performed.
 * @return the next block after the (possibly cut) block.
 */
static inline xlat_cache_block_t xlat_cut_block( xlat_cache_block_t block, int cutsize )
{
    if( block->size > cutsize + MIN_TOTAL_SIZE ) {
	int oldsize = block->size;
	block->size = cutsize;
	xlat_cache_block_t next = NEXT(block);
	next->active = 0;
	next->size = oldsize - cutsize - sizeof(struct xlat_cache_block);
	return next;
    } else {
	return NEXT(block);
    }
}

/**
 * Promote a block in temp space (or elsewhere for that matter) to old space.
 *
 * @param block to promote.
 */
static void xlat_promote_to_old_space( xlat_cache_block_t block )
{
    int allocation = -sizeof(struct xlat_cache_block);
    int size = block->size;
    xlat_cache_block_t curr = xlat_old_cache_ptr;
    xlat_cache_block_t start_block = curr;
    do {
	allocation += curr->size + sizeof(struct xlat_cache_block);
	curr = NEXT(curr);
	if( allocation > size ) {
	    break; /* done */
	}
	if( curr->size == 0 ) { /* End-of-cache Sentinel */
	    /* Leave what we just released as free space and start again from the
	     * top of the cache
	     */
	    start_block->active = 0;
	    start_block->size = allocation;
	    allocation = -sizeof(struct xlat_cache_block);
	    start_block = curr = xlat_old_cache;
	}
    } while(1);
    start_block->active = 1;
    start_block->size = allocation;
    start_block->lut_entry = block->lut_entry;
    *block->lut_entry = &start_block->code;
    memcpy( start_block->code, block->code, block->size );
    xlat_old_cache_ptr = xlat_cut_block(start_block, size );
    if( xlat_old_cache_ptr->size == 0 ) {
	xlat_old_cache_ptr = xlat_old_cache;
    }
}

/**
 * Similarly to the above method, promotes a block to temp space.
 * TODO: Try to combine these - they're nearly identical
 */
void xlat_promote_to_temp_space( xlat_cache_block_t block )
{
    int size = block->size;
    int allocation = -sizeof(struct xlat_cache_block);
    xlat_cache_block_t curr = xlat_temp_cache_ptr;
    xlat_cache_block_t start_block = curr;
    do {
	if( curr->active == BLOCK_USED ) {
	    xlat_promote_to_old_space( curr );
	}
	allocation += curr->size + sizeof(struct xlat_cache_block);
	curr = NEXT(curr);
	if( allocation > size ) {
	    break; /* done */
	}
	if( curr->size == 0 ) { /* End-of-cache Sentinel */
	    /* Leave what we just released as free space and start again from the
	     * top of the cache
	     */
	    start_block->active = 0;
	    start_block->size = allocation;
	    allocation = -sizeof(struct xlat_cache_block);
	    start_block = curr = xlat_temp_cache;
	}
    } while(1);
    start_block->active = 1;
    start_block->size = allocation;
    start_block->lut_entry = block->lut_entry;
    *block->lut_entry = &start_block->code;
    memcpy( start_block->code, block->code, block->size );
    xlat_temp_cache_ptr = xlat_cut_block(start_block, size );
    if( xlat_temp_cache_ptr->size == 0 ) {
	xlat_temp_cache_ptr = xlat_temp_cache;
    }
    
}

/**
 * Returns the next block in the new cache list that can be written to by the
 * translator. If the next block is active, it is evicted first.
 */
xlat_cache_block_t xlat_start_block( sh4addr_t address )
{
    if( xlat_new_cache_ptr->size == 0 ) {
	xlat_new_cache_ptr = xlat_new_cache;
    }

    if( xlat_new_cache_ptr->active ) {
	xlat_promote_to_temp_space( xlat_new_cache_ptr );
    }
    xlat_new_create_ptr = xlat_new_cache_ptr;
    xlat_new_create_ptr->active = 1;
    xlat_new_cache_ptr = NEXT(xlat_new_cache_ptr);

    /* Add the LUT entry for the block */
    if( xlat_lut[XLAT_LUT_PAGE(address)] == NULL ) {
	xlat_lut[XLAT_LUT_PAGE(address)] =
	    mmap( NULL, XLAT_LUT_PAGE_SIZE, PROT_READ|PROT_WRITE,
		  MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );
	memset( xlat_lut[XLAT_LUT_PAGE(address)], 0, XLAT_LUT_PAGE_SIZE );
    }

    if( IS_ENTRY_POINT(xlat_lut[XLAT_LUT_PAGE(address)][XLAT_LUT_ENTRY(address)]) ) {
	xlat_cache_block_t oldblock = BLOCK_FOR_CODE(xlat_lut[XLAT_LUT_PAGE(address)][XLAT_LUT_ENTRY(address)]);
	oldblock->active = 0;
    }

    xlat_lut[XLAT_LUT_PAGE(address)][XLAT_LUT_ENTRY(address)] = 
	&xlat_new_create_ptr->code;
    xlat_new_create_ptr->lut_entry = xlat_lut[XLAT_LUT_PAGE(address)] + XLAT_LUT_ENTRY(address);
    
    return xlat_new_create_ptr;
}

xlat_cache_block_t xlat_extend_block()
{
    if( xlat_new_cache_ptr->size == 0 ) {
	/* Migrate to the front of the cache to keep it contiguous */
	xlat_new_create_ptr->active = 0;
	char *olddata = xlat_new_create_ptr->code;
	int oldsize = xlat_new_create_ptr->size;
	int size = oldsize + MIN_BLOCK_SIZE; /* minimum expansion */
	void **lut_entry = xlat_new_create_ptr->lut_entry;
	int allocation = -sizeof(struct xlat_cache_block);
	xlat_new_cache_ptr = xlat_new_cache;
	do {
	    if( xlat_new_cache_ptr->active ) {
		xlat_promote_to_temp_space( xlat_new_cache_ptr );
	    }
	    allocation += xlat_new_cache_ptr->size + sizeof(struct xlat_cache_block);
	    xlat_new_cache_ptr = NEXT(xlat_new_cache_ptr);
	} while( allocation < size );
	xlat_new_create_ptr = xlat_new_cache;
	xlat_new_create_ptr->active = 1;
	xlat_new_create_ptr->size = allocation;
	xlat_new_create_ptr->lut_entry = lut_entry;
	*lut_entry = &xlat_new_create_ptr->code;
	memmove( xlat_new_create_ptr->code, olddata, oldsize );
    } else {
	if( xlat_new_cache_ptr->active ) {
	    xlat_promote_to_temp_space( xlat_new_cache_ptr );
	}
	xlat_new_create_ptr->size += xlat_new_cache_ptr->size + sizeof(struct xlat_cache_block);
	xlat_new_cache_ptr = NEXT(xlat_new_cache_ptr);
    }
    return xlat_new_create_ptr;

}

void xlat_commit_block( uint32_t destsize, uint32_t srcsize )
{
    void **ptr = xlat_new_create_ptr->lut_entry;
    void **endptr = ptr + (srcsize>>2);
    while( ptr < endptr ) {
	if( *ptr == NULL ) {
	    *ptr = XLAT_LUT_ENTRY_USED;
	}
	ptr++;
    }

    xlat_new_cache_ptr = xlat_cut_block( xlat_new_create_ptr, destsize );
}

void xlat_delete_block( xlat_cache_block_t block ) 
{
    block->active = 0;
    *block->lut_entry = NULL;
}

void xlat_check_cache_integrity( xlat_cache_block_t cache, xlat_cache_block_t ptr, int size )
{
    int foundptr = 0;
    xlat_cache_block_t tail = 
	(xlat_cache_block_t)(((char *)cache) + size - sizeof(struct xlat_cache_block));

    assert( tail->active == 1 );
    assert( tail->size == 0 ); 
    while( cache < tail ) {
	assert( cache->active >= 0 && cache->active <= 2 );
	assert( cache->size >= 0 && cache->size < size );
	if( cache == ptr ) {
	    foundptr = 1;
	}
	cache = NEXT(cache);
    }
    assert( cache == tail );
    assert( foundptr == 1 );
}

void xlat_check_integrity( )
{
    xlat_check_cache_integrity( xlat_new_cache, xlat_new_cache_ptr, XLAT_NEW_CACHE_SIZE );
    xlat_check_cache_integrity( xlat_temp_cache, xlat_temp_cache_ptr, XLAT_TEMP_CACHE_SIZE );
    xlat_check_cache_integrity( xlat_old_cache, xlat_old_cache_ptr, XLAT_OLD_CACHE_SIZE );
}
