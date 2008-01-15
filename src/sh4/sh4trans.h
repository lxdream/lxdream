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

#include "sh4/xltcache.h"
#include "dream.h"
#include "mem.h"

/** Maximum size of a translated instruction, in bytes. This includes potentially
 * writing the entire epilogue
 */
#define MAX_INSTRUCTION_SIZE 256
/** Maximum size of the translation epilogue (current real size is 116 bytes, so
 * allows a little room
 */
#define EPILOGUE_SIZE 128

/** Maximum number of recovery records for a translated block (2048 based on
 * 1 record per SH4 instruction in a 4K page).
 */
#define MAX_RECOVERY_SIZE 2048

/**
 * Translation flag - exit the current block but continue (eg exception handling)
 */
#define XLAT_EXIT_CONTINUE 1

/**
 * Translation flag - exit the current block and halt immediately (eg fatal error)
 */
#define XLAT_EXIT_HALT 2

/**
 * Translation flag - exit the current block and halt immediately for a system
 * breakpoint.
 */
#define XLAT_EXIT_BREAKPOINT 3

/**
 * Translation flag - exit the current block and continue after performing a full
 * system reset (dreamcast_reset())
 */
#define XLAT_EXIT_SYSRESET 4

/**
 */
uint32_t sh4_xlat_run_slice( uint32_t nanosecs );

/**
 * Return true if translated code is currently running
 */
gboolean sh4_xlat_is_running();

/**
 * Translate the specified block of code starting from the specified start
 * address until the first branch/jump instruction.
 */
void *sh4_translate_basic_block( sh4addr_t start );


extern uint8_t *xlat_output;
extern struct xlat_recovery_record xlat_recovery[MAX_RECOVERY_SIZE];
extern uint32_t xlat_recovery_posn;

/******************************************************************************
 * Code generation - these methods must be provided by the
 * actual code gen (eg sh4x86.c) 
 ******************************************************************************/

#define TARGET_X86 1
#define TARGET_X86_64 2

void sh4_translate_begin_block( sh4addr_t pc );
uint32_t sh4_translate_instruction( sh4addr_t pc );
void sh4_translate_end_block( sh4addr_t pc );

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
 * From within the translator, immediately exit the current translation block with
 * the specified exit code (one of the XLAT_EXIT_* values).
 */
void sh4_translate_exit( int exit_code );
