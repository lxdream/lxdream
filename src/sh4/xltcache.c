/**
 * $Id$
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

#include <sys/types.h>
#include <sys/mman.h>
#include <assert.h>

#include "dreamcast.h"
#include "sh4/sh4core.h"
#include "sh4/xltcache.h"
#include "x86dasm/x86dasm.h"

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

#ifdef XLAT_GENERATIONAL_CACHE
xlat_cache_block_t xlat_temp_cache;
xlat_cache_block_t xlat_temp_cache_ptr;
xlat_cache_block_t xlat_old_cache;
xlat_cache_block_t xlat_old_cache_ptr;
#endif

static void **xlat_lut[XLAT_LUT_PAGES];
static gboolean xlat_initialized = FALSE;

void xlat_cache_init(void) 
{
    if( !xlat_initialized ) {
        xlat_initialized = TRUE;
        xlat_new_cache = mmap( NULL, XLAT_NEW_CACHE_SIZE, PROT_EXEC|PROT_READ|PROT_WRITE,
                MAP_PRIVATE|MAP_ANON, -1, 0 );
        xlat_new_cache_ptr = xlat_new_cache;
        xlat_new_create_ptr = xlat_new_cache;
#ifdef XLAT_GENERATIONAL_CACHE
        xlat_temp_cache = mmap( NULL, XLAT_TEMP_CACHE_SIZE, PROT_EXEC|PROT_READ|PROT_WRITE,
                MAP_PRIVATE|MAP_ANON, -1, 0 );
        xlat_old_cache = mmap( NULL, XLAT_OLD_CACHE_SIZE, PROT_EXEC|PROT_READ|PROT_WRITE,
                MAP_PRIVATE|MAP_ANON, -1, 0 );
        xlat_temp_cache_ptr = xlat_temp_cache;
        xlat_old_cache_ptr = xlat_old_cache;
#endif
//        xlat_lut = mmap( NULL, XLAT_LUT_PAGES*sizeof(void *), PROT_READ|PROT_WRITE,
//                MAP_PRIVATE|MAP_ANON, -1, 0);
        memset( xlat_lut, 0, XLAT_LUT_PAGES*sizeof(void *) );
    }
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
#ifdef XLAT_GENERATIONAL_CACHE
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
#endif
    for( i=0; i<XLAT_LUT_PAGES; i++ ) {
        if( xlat_lut[i] != NULL ) {
            memset( xlat_lut[i], 0, XLAT_LUT_PAGE_SIZE );
        }
    }
}

static void xlat_flush_page_by_lut( void **page )
{
    int i;
    for( i=0; i<XLAT_LUT_PAGE_ENTRIES; i++ ) {
        if( IS_ENTRY_POINT(page[i]) ) {
            XLAT_BLOCK_FOR_CODE(page[i])->active = 0;
        }
        page[i] = NULL;
    }
}

void FASTCALL xlat_invalidate_word( sh4addr_t addr )
{
    void **page = xlat_lut[XLAT_LUT_PAGE(addr)];
    if( page != NULL ) {
        int entry = XLAT_LUT_ENTRY(addr);
        if( page[entry] != NULL ) {
            xlat_flush_page_by_lut(page);
        }
    }
}

void FASTCALL xlat_invalidate_long( sh4addr_t addr )
{
    void **page = xlat_lut[XLAT_LUT_PAGE(addr)];
    if( page != NULL ) {
        int entry = XLAT_LUT_ENTRY(addr);
        if( *(uint64_t *)&page[entry] != 0 ) {
            xlat_flush_page_by_lut(page);
        }
    }
}

void FASTCALL xlat_invalidate_block( sh4addr_t address, size_t size )
{
    int i;
    int entry_count = size >> 1; // words;
    uint32_t page_no = XLAT_LUT_PAGE(address);
    int entry = XLAT_LUT_ENTRY(address);
    do {
        void **page = xlat_lut[page_no];
        int page_entries = XLAT_LUT_PAGE_ENTRIES - entry;
        if( entry_count < page_entries ) {
            page_entries = entry_count;
        }
        if( page != NULL ) {
            if( page_entries == XLAT_LUT_PAGE_ENTRIES ) {
                /* Overwriting the entire page anyway */
                xlat_flush_page_by_lut(page);
            } else {
                for( i=entry; i<entry+page_entries; i++ ) {
                    if( page[i] != NULL ) {
                        xlat_flush_page_by_lut(page);
                        break;
                    }
                }
            }
            entry_count -= page_entries;
        }
        page_no ++;
        entry_count -= page_entries;
        entry = 0;
    } while( entry_count > 0 );
}

void FASTCALL xlat_flush_page( sh4addr_t address )
{
    void **page = xlat_lut[XLAT_LUT_PAGE(address)];
    if( page != NULL ) {
        xlat_flush_page_by_lut(page);
    }
}

void * FASTCALL xlat_get_code( sh4addr_t address )
{
    void *result = NULL;
    void **page = xlat_lut[XLAT_LUT_PAGE(address)];
    if( page != NULL ) {
        result = (void *)(((uintptr_t)(page[XLAT_LUT_ENTRY(address)])) & (~((uintptr_t)0x03)));
    }
    return result;
}

xlat_recovery_record_t xlat_get_pre_recovery( void *code, void *native_pc )
{
    if( code != NULL ) {
        uintptr_t pc_offset = ((uint8_t *)native_pc) - ((uint8_t *)code);
        xlat_cache_block_t block = XLAT_BLOCK_FOR_CODE(code);
        uint32_t count = block->recover_table_size;
        xlat_recovery_record_t records = (xlat_recovery_record_t)(&block->code[block->recover_table_offset]);
        uint32_t posn;
        for( posn = 1; posn < count; posn++ ) {
        	if( records[posn].xlat_offset >= pc_offset ) {
        		return &records[posn-1];
        	}
        }
        return &records[count-1];
    }
    return NULL;	
}

void ** FASTCALL xlat_get_lut_entry( sh4addr_t address )
{
    void **page = xlat_lut[XLAT_LUT_PAGE(address)];

    /* Add the LUT entry for the block */
    if( page == NULL ) {
        xlat_lut[XLAT_LUT_PAGE(address)] = page =
            mmap( NULL, XLAT_LUT_PAGE_SIZE, PROT_READ|PROT_WRITE,
                    MAP_PRIVATE|MAP_ANON, -1, 0 );
        memset( page, 0, XLAT_LUT_PAGE_SIZE );
    }

    return &page[XLAT_LUT_ENTRY(address)];
}



uint32_t FASTCALL xlat_get_block_size( void *block )
{
    xlat_cache_block_t xlt = (xlat_cache_block_t)(((char *)block)-sizeof(struct xlat_cache_block));
    return xlt->size;
}

uint32_t FASTCALL xlat_get_code_size( void *block )
{
    xlat_cache_block_t xlt = (xlat_cache_block_t)(((char *)block)-sizeof(struct xlat_cache_block));
    if( xlt->recover_table_offset == 0 ) {
        return xlt->size;
    } else {
        return xlt->recover_table_offset;
    }
}

/**
 * Cut the specified block so that it has the given size, with the remaining data
 * forming a new free block. If the free block would be less than the minimum size,
 * the cut is not performed.
 * @return the next block after the (possibly cut) block.
 */
static inline xlat_cache_block_t xlat_cut_block( xlat_cache_block_t block, int cutsize )
{
    cutsize = (cutsize + 3) & 0xFFFFFFFC; // force word alignment
    assert( cutsize <= block->size );
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

#ifdef XLAT_GENERATIONAL_CACHE
/**
 * Promote a block in temp space (or elsewhere for that matter) to old space.
 *
 * @param block to promote.
 */
static void xlat_promote_to_old_space( xlat_cache_block_t block )
{
    int allocation = (int)-sizeof(struct xlat_cache_block);
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
            allocation = (int)-sizeof(struct xlat_cache_block);
            start_block = curr = xlat_old_cache;
        }
    } while(1);
    start_block->active = 1;
    start_block->size = allocation;
    start_block->lut_entry = block->lut_entry;
    start_block->fpscr_mask = block->fpscr_mask;
    start_block->fpscr = block->fpscr;
    start_block->recover_table_offset = block->recover_table_offset;
    start_block->recover_table_size = block->recover_table_size;
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
    int allocation = (int)-sizeof(struct xlat_cache_block);
    xlat_cache_block_t curr = xlat_temp_cache_ptr;
    xlat_cache_block_t start_block = curr;
    do {
        if( curr->active == BLOCK_USED ) {
            xlat_promote_to_old_space( curr );
        } else if( curr->active == BLOCK_ACTIVE ) {
            // Active but not used, release block
            *((uintptr_t *)curr->lut_entry) &= ((uintptr_t)0x03);
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
            allocation = (int)-sizeof(struct xlat_cache_block);
            start_block = curr = xlat_temp_cache;
        }
    } while(1);
    start_block->active = 1;
    start_block->size = allocation;
    start_block->lut_entry = block->lut_entry;
    start_block->fpscr_mask = block->fpscr_mask;
    start_block->fpscr = block->fpscr;
    start_block->recover_table_offset = block->recover_table_offset;
    start_block->recover_table_size = block->recover_table_size;
    *block->lut_entry = &start_block->code;
    memcpy( start_block->code, block->code, block->size );
    xlat_temp_cache_ptr = xlat_cut_block(start_block, size );
    if( xlat_temp_cache_ptr->size == 0 ) {
        xlat_temp_cache_ptr = xlat_temp_cache;
    }

}
#else 
void xlat_promote_to_temp_space( xlat_cache_block_t block )
{
    *block->lut_entry = 0;
}
#endif

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
                    MAP_PRIVATE|MAP_ANON, -1, 0 );
        memset( xlat_lut[XLAT_LUT_PAGE(address)], 0, XLAT_LUT_PAGE_SIZE );
    }

    if( IS_ENTRY_POINT(xlat_lut[XLAT_LUT_PAGE(address)][XLAT_LUT_ENTRY(address)]) ) {
        xlat_cache_block_t oldblock = XLAT_BLOCK_FOR_CODE(xlat_lut[XLAT_LUT_PAGE(address)][XLAT_LUT_ENTRY(address)]);
        oldblock->active = 0;
    }

    xlat_lut[XLAT_LUT_PAGE(address)][XLAT_LUT_ENTRY(address)] = 
        &xlat_new_create_ptr->code;
    xlat_new_create_ptr->lut_entry = xlat_lut[XLAT_LUT_PAGE(address)] + XLAT_LUT_ENTRY(address);

    return xlat_new_create_ptr;
}

xlat_cache_block_t xlat_extend_block( uint32_t newSize )
{
    while( xlat_new_create_ptr->size < newSize ) {
        if( xlat_new_cache_ptr->size == 0 ) {
            /* Migrate to the front of the cache to keep it contiguous */
            xlat_new_create_ptr->active = 0;
            sh4ptr_t olddata = xlat_new_create_ptr->code;
            int oldsize = xlat_new_create_ptr->size;
            int size = oldsize + MIN_BLOCK_SIZE; /* minimum expansion */
            void **lut_entry = xlat_new_create_ptr->lut_entry;
            int allocation = (int)-sizeof(struct xlat_cache_block);
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
    assert( foundptr == 1 || tail == ptr );
}

void xlat_check_integrity( )
{
    xlat_check_cache_integrity( xlat_new_cache, xlat_new_cache_ptr, XLAT_NEW_CACHE_SIZE );
#ifdef XLAT_GENERATIONAL_CACHE
    xlat_check_cache_integrity( xlat_temp_cache, xlat_temp_cache_ptr, XLAT_TEMP_CACHE_SIZE );
    xlat_check_cache_integrity( xlat_old_cache, xlat_old_cache_ptr, XLAT_OLD_CACHE_SIZE );
#endif
}

