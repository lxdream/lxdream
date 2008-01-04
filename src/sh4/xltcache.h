/**
 * $Id$
 * 
 * Translation cache support (architecture independent)
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

#include "dream.h"
#include "mem.h"

typedef struct xlat_cache_block {
    int active;  /* 0 = deleted, 1 = normal. 2 = accessed (temp-space only) */
    uint32_t size;
    void **lut_entry; /* For deletion */
    unsigned char code[0];
} *xlat_cache_block_t;

/**
 * Initialize the translation cache
 */
void xlat_cache_init(void);

/**
 * Returns the next block in the new cache list that can be written to by the
 * translator.
 */
xlat_cache_block_t xlat_start_block(sh4addr_t address);

/**
 * Increases the current block size (only valid between calls to xlat_start_block()
 * and xlat_commit_block()). 
 * @return the new block, which may be different from the old block.
 */
xlat_cache_block_t xlat_extend_block( uint32_t newSize );

/**
 * Commit the current translation block
 * @param addr target address (for the lookup table)
 * @param destsize final size of the translation in bytes.
 * @param srcsize size of the original data that was translated in bytes
 */
void xlat_commit_block( uint32_t destsize, uint32_t srcsize );

/**
 * Dump the disassembly of the specified code block to a stream
 * (primarily for debugging purposes)
 * @param out The stream to write the output to
 * @param code a translated block
 */
void xlat_disasm_block( FILE *out, void *code );


/**
 * Delete (deactivate) the specified block from the cache. Caller is responsible
 * for ensuring that there really is a block there.
 */
void xlat_delete_block( xlat_cache_block_t block );

/**
 * Retrieve the entry point for the translated code corresponding to the given
 * SH4 address, or NULL if there is no code for that address.
 */
void *xlat_get_code( sh4addr_t address );

/**
 * Retrieve the entry point for the translated code corresponding to the given
 * SH4 virtual address, or NULL if there is no code for the address. 
 * If the virtual address cannot be resolved, this method will raise a TLB miss 
 * exception, and return NULL.
 */
void *xlat_get_code_by_vma( sh4vma_t address );

/**
 * Retrieve the address of the lookup table entry corresponding to the
 * given SH4 address.
 */
void **xlat_get_lut_entry( sh4addr_t address );

/**
 * Retrieve the size of the code block starting at the specified pointer. If the
 * pointer is not a valid code block, the return value is undefined.
 */
uint32_t xlat_get_block_size( void *ptr );

/**
 * Flush the code cache for the page containing the given address
 */
void xlat_flush_page( sh4addr_t address );

void xlat_invalidate_word( sh4addr_t address );
void xlat_invalidate_long( sh4addr_t address );


/**
 * Invalidate the code cache for a memory region
 */
void xlat_invalidate_block( sh4addr_t address, size_t bytes );

/**
 * Flush the entire code cache. This isn't as cheap as one might like
 */
void xlat_flush_cache();

/**
 * Check the internal integrity of the cache
 */
void xlat_check_integrity();
