/**
 * $Id$
 * 
 * SH4->x86 translation module
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

#ifndef lxdream_sh4trans_H
#define lxdream_sh4trans_H 1

#include "xlat/xltcache.h"
#include "dream.h"
#include "mem.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum size of a translated instruction, in bytes. Current worst case seems
 * to be a BF/S followed by one of the long FMOVs.
 */
#define MAX_INSTRUCTION_SIZE 512
/** Maximum size of the translation epilogue (current real size is 116 bytes, so
 * allows a little room
 */
#define EPILOGUE_SIZE 136

/** Maximum number of recovery records for a translated block (2048 based on
 * 1 record per SH4 instruction in a 4K page).
 */
#define MAX_RECOVERY_SIZE 2049

typedef void (*xlat_block_begin_callback_t)();
typedef void (*xlat_block_end_callback_t)();

/**
 */
uint32_t sh4_translate_run_slice( uint32_t nanosecs );

/**
 * Initialize the translation engine (if required). Note xlat cache
 * must already be initialized.
 */
void sh4_translate_init( void);

/**
 * Translate the specified block of code starting from the specified start
 * address until the first branch/jump instruction.
 */
void *sh4_translate_basic_block( sh4addr_t start );

/**
 * Add a recovery record for the current code generation position, with the
 * specified instruction count
 */
void sh4_translate_add_recovery( uint32_t icount );

/**
 * Initialize shadow execution mode
 */
void sh4_shadow_init( void );

extern uint8_t *xlat_output;
extern struct xlat_recovery_record xlat_recovery[MAX_RECOVERY_SIZE];
extern xlat_cache_block_t xlat_current_block;
extern uint32_t xlat_recovery_posn;

/******************************************************************************
 * Code generation - these methods must be provided by the
 * actual code gen (eg sh4x86.c) 
 ******************************************************************************/

#define TARGET_X86 1

void sh4_translate_begin_block( sh4addr_t pc );
uint32_t sh4_translate_instruction( sh4addr_t pc );
void sh4_translate_end_block( sh4addr_t pc );
uint32_t sh4_translate_end_block_size();
void sh4_translate_emit_breakpoint( sh4vma_t pc );
void sh4_translate_crashdump();

typedef void (*unwind_thunk_t)(void);

/**
 * Set instrumentation callbacks
 */
void sh4_translate_set_callbacks( xlat_block_begin_callback_t begin, xlat_block_end_callback_t end );

/**
 * Enable/disable memory optimizations that bypass the mmu
 */
void sh4_translate_set_fastmem( gboolean flag );

/**
 * Enable/disable basic block profiling
 */
void sh4_translate_set_profile_blocks( gboolean flag );

/**
 * Get the boolean flag indicating whether block profiling is on.
 */
gboolean sh4_translate_get_profile_blocks();

/**
 * Set the address spaces for the translated code.
 */
void sh4_translate_set_address_space( struct mem_region_fn **priv, struct mem_region_fn **user );

/**
 * From within the translator, (typically called from MMU exception handling routines)
 * immediately exit the current translation block (performing cleanup as necessary) and
 * return to sh4_translate_run_slice(). Effectively a fast longjmp w/ xlat recovery.
 *
 * Note: The correct working of this method depends on the translator anticipating the
 * exception and generating the appropriate recovery block(s) - currently this means 
 * that it should ONLY be called from within the context of a memory read or write.
 *
 * @param is_completion If TRUE, exit after completing the current instruction (effectively),
 *   otherwise abort the current instruction with no effect. 
 * @param thunk A function to execute after perform xlat recovery, but before returning
 * to run_slice. If NULL, control returns directly.
 * @return This method never returns. 
 */
void sh4_translate_unwind_stack( gboolean is_completion, unwind_thunk_t thunk );

/**
 * Called when doing a break out of the translator - finalizes the system state up to
 * the end of the current instruction.
 */
void sh4_translate_exit_recover( );

/**
 * Called when doing a break out of the translator following a taken exception - 
 * finalizes the system state up to the start of the current instruction.
 */
void sh4_translate_exception_exit_recover( );

/**
 * From within the translator, exit the current block at the end of the 
 * current instruction, flush the translation cache (completely) 
 * @return TRUE to perform a vm-exit/continue after the flush
 */
gboolean sh4_translate_flush_cache( void );

/**
 * Given a block's use_list, remove all direct links to the block.
 */
void sh4_translate_unlink_block( void *use_list );

/**
 * Support function called from the translator when a breakpoint is hit.
 * Either returns immediately (to skip the breakpoint), or aborts the current
 * cycle and never returns.
 */
void FASTCALL sh4_translate_breakpoint_hit( sh4vma_t pc );

/**
 * Disassemble the given translated code block, and it's source SH4 code block
 * side-by-side. The current native pc will be marked if non-null.
 */
void sh4_translate_disasm_block( FILE *out, void *code, sh4addr_t source_start, void *native_pc );

/**
 * Dump the top N blocks in the SH4 translation cache
 */
void sh4_translate_dump_cache_by_activity( unsigned int topN );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_sh4trans_H */
