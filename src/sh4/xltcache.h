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

#ifndef lxdream_xltcache_H
#define lxdream_xltcache_H 1

/**
 * For now, recovery is purely a matter of mapping native pc => sh4 pc,
 * and updating sh4r.pc & sh4r.slice_cycles accordingly. In future more
 * detailed recovery may be required if the translator optimizes more
 * agressively.
 *
 * The recovery table contains (at least) one entry per abortable instruction,
 * 
 */
typedef struct xlat_recovery_record {
    uint32_t xlat_offset;    // native (translated) pc 
    uint32_t sh4_icount;     // instruction number of the corresponding SH4 instruction
                             // (0 = first instruction, 1 = second instruction, ... )
} *xlat_recovery_record_t;

struct xlat_cache_block {
    int active;  /* 0 = deleted, 1 = normal. 2 = accessed (temp-space only) */
    uint32_t size;
    void **lut_entry; /* For deletion */
    uint32_t xlat_sh4_mode; /* comparison with sh4r.xlat_sh4_mode */
    uint32_t recover_table_offset; // Offset from code[0] of the recovery table;
    uint32_t recover_table_size;
    unsigned char code[0];
} __attribute__((packed));

typedef struct xlat_cache_block *xlat_cache_block_t;

#define XLAT_BLOCK_FOR_CODE(code) (((xlat_cache_block_t)code)-1)

#define XLAT_BLOCK_MODE(code) (XLAT_BLOCK_FOR_CODE(code)->xlat_sh4_mode) 

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
void * FASTCALL xlat_get_code( sh4addr_t address );

/**
 * Retrieve the post-instruction recovery record corresponding to the given
 * native address, or NULL if there is no recovery code for the address.
 * @param code The code block containing the recovery table.
 * @param native_pc A pointer that must be within the currently executing 
 * @param with_terminal If false, return NULL instead of the final block record. 
 * return the first record before or equal to the given pc.
 * translation block.
 */
struct xlat_recovery_record *xlat_get_post_recovery( void *code, void *native_pc, gboolean with_terminal );

/**
 * Retrieve the pre-instruction recovery record corresponding to the given
 * native address, or NULL if there is no recovery code for the address.
 * @param code The code block containing the recovery table.
 * @param native_pc A pointer that must be within the currently executing 
 * @param with_terminal If false, return NULL instead of the final block record. 
 * return the first record before or equal to the given pc.
 * translation block.
 */
struct xlat_recovery_record *xlat_get_pre_recovery( void *code, void *native_pc );


/**
 * Retrieve the entry point for the translated code corresponding to the given
 * SH4 virtual address, or NULL if there is no code for the address. 
 * If the virtual address cannot be resolved, this method will raise a TLB miss 
 * exception, and return NULL.
 */
void * FASTCALL xlat_get_code_by_vma( sh4vma_t address );

/**
 * Retrieve the address of the lookup table entry corresponding to the
 * given SH4 address.
 */
void ** FASTCALL xlat_get_lut_entry( sh4addr_t address );

/**
 * Retrieve the current host address of the running translated code block.
 * @return the host PC, or null if there is no currently executing translated
 * block (or the stack is corrupted)
 * Note: the implementation of this method is host (and calling-convention) specific.
 * @param block_start start of the block the PC should be in
 * @param block_size size of the code block in bytes.
 */
void *xlat_get_native_pc( void *block_start, uint32_t block_size );

/**
 * Retrieve the size of the block starting at the specified pointer. If the
 * pointer is not a valid code block, the return value is undefined.
 */
uint32_t FASTCALL xlat_get_block_size( void *ptr );

/**
 * Retrieve the size of the code in the block starting at the specified 
 * pointer. Effectively this is xlat_get_block_size() minus the size of
 * the recovery table. If the pointer is not a valid code block, the 
 * return value is undefined.
 */
uint32_t FASTCALL xlat_get_code_size( void *ptr );

/**
 * Flush the code cache for the page containing the given address
 */
void FASTCALL xlat_flush_page( sh4addr_t address );

void FASTCALL xlat_invalidate_word( sh4addr_t address );
void FASTCALL xlat_invalidate_long( sh4addr_t address );


/**
 * Invalidate the code cache for a memory region
 */
void FASTCALL xlat_invalidate_block( sh4addr_t address, size_t bytes );

/**
 * Flush the entire code cache. This isn't as cheap as one might like
 */
void xlat_flush_cache();

/**
 * Check the internal integrity of the cache
 */
void xlat_check_integrity();

#endif /* lxdream_xltcache_H */
