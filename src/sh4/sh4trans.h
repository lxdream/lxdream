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

#include "sh4/xltcache.h"
#include "dream.h"
#include "mem.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum size of a translated instruction, in bytes. Current worst case seems
 * to be a BF/S followed by one of the long FMOVs.
 */
#define MAX_INSTRUCTION_SIZE 384
/** Maximum size of the translation epilogue (current real size is 116 bytes, so
 * allows a little room
 */
#define EPILOGUE_SIZE 128

/** Maximum number of recovery records for a translated block (2048 based on
 * 1 record per SH4 instruction in a 4K page).
 */
#define MAX_RECOVERY_SIZE 2049

/**
 */
uint32_t sh4_xlat_run_slice( uint32_t nanosecs );

/**
 * Return true if translated code is currently running
 */
gboolean sh4_xlat_is_running();

/**
 * Initialize the translation engine (if required). Note xlat cache
 * must already be initialized.
 */
void sh4_xlat_init();

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

extern uint8_t *xlat_output;
extern struct xlat_recovery_record xlat_recovery[MAX_RECOVERY_SIZE];
extern xlat_cache_block_t xlat_current_block;
extern uint32_t xlat_recovery_posn;

/******************************************************************************
 * Code generation - these methods must be provided by the
 * actual code gen (eg sh4x86.c) 
 ******************************************************************************/

#define TARGET_X86 1

void sh4_translate_init( void );
void sh4_translate_begin_block( sh4addr_t pc );
uint32_t sh4_translate_instruction( sh4addr_t pc );
void sh4_translate_end_block( sh4addr_t pc );
uint32_t sh4_translate_end_block_size();
void sh4_translate_emit_breakpoint( sh4vma_t pc );

typedef void (*unwind_thunk_t)(void);

/**
 * From within the translator, (typically called from MMU exception handling routines)
 * immediately exit the current translation block (performing cleanup as necessary) and
 * return to sh4_xlat_run_slice(). Effectively a fast longjmp w/ xlat recovery.
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
 * From within the translator, exit the current block at the end of the 
 * current instruction, flush the translation cache (completely) 
 * @return TRUE to perform a vm-exit/continue after the flush
 */
gboolean sh4_translate_flush_cache( void );

/**
 * Support function called from the translator when a breakpoint is hit.
 * Either returns immediately (to skip the breakpoint), or aborts the current
 * cycle and never returns.
 */
void sh4_translate_breakpoint_hit( sh4vma_t pc );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_sh4trans_H */