/**
 * $Id$
 * 
 * SH4 translation core module. This part handles the non-target-specific
 * section of the translation.
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
#include <assert.h>
#include <setjmp.h>
#include "eventq.h"
#include "syscall.h"
#include "clock.h"
#include "sh4/sh4core.h"
#include "sh4/sh4trans.h"
#include "sh4/xltcache.h"


static jmp_buf xlat_jmp_buf;
/**
 * Execute a timeslice using translated code only (ie translate/execute loop)
 * Note this version does not support breakpoints
 */
uint32_t sh4_xlat_run_slice( uint32_t nanosecs ) 
{
    sh4r.slice_cycle = 0;

    if( sh4r.sh4_state != SH4_STATE_RUNNING ) {
	if( sh4r.event_pending < nanosecs ) {
	    sh4r.sh4_state = SH4_STATE_RUNNING;
	    sh4r.slice_cycle = sh4r.event_pending;
	}
    }

    int jmp = setjmp(xlat_jmp_buf);

    void * (*code)() = NULL;
    while( sh4r.slice_cycle < nanosecs ) {
	if( sh4r.event_pending <= sh4r.slice_cycle ) {
	    if( sh4r.event_types & PENDING_EVENT ) {
		event_execute();
	    }
	    /* Eventq execute may (quite likely) deliver an immediate IRQ */
	    if( sh4r.event_types & PENDING_IRQ ) {
		sh4_accept_interrupt();
		code = NULL;
	    }
	}
	
	if( code == NULL ) {
	    if( sh4r.pc > 0xFFFFFF00 ) {
		syscall_invoke( sh4r.pc );
		sh4r.in_delay_slot = 0;
		sh4r.pc = sh4r.pr;
	    }

	    code = xlat_get_code_by_vma( sh4r.pc );
	    if( code == NULL ) {
		code = sh4_translate_basic_block( sh4r.pc );
	    }
	}
	code = code();
    }

    if( sh4r.sh4_state != SH4_STATE_STANDBY ) {
	TMU_run_slice( nanosecs );
	SCIF_run_slice( nanosecs );
    }
    return nanosecs;
}

uint8_t *xlat_output;
struct xlat_recovery_record xlat_recovery[MAX_RECOVERY_SIZE];
uint32_t xlat_recovery_posn;

/**
 * Translate a linear basic block, ie all instructions from the start address
 * (inclusive) until the next branch/jump instruction or the end of the page
 * is reached.
 * @return the address of the translated block
 * eg due to lack of buffer space.
 */
void * sh4_translate_basic_block( sh4addr_t start )
{
    sh4addr_t pc = start;
    sh4addr_t lastpc = (pc&0xFFFFF000)+0x1000;
    int done;
    xlat_cache_block_t block = xlat_start_block( start );
    xlat_output = (uint8_t *)block->code;
    xlat_recovery_posn = 0;
    uint8_t *eob = xlat_output + block->size;
    sh4_translate_begin_block(pc);

    do {
	if( eob - xlat_output < MAX_INSTRUCTION_SIZE ) {
	    uint8_t *oldstart = block->code;
	    block = xlat_extend_block( xlat_output - oldstart + MAX_INSTRUCTION_SIZE );
	    xlat_output = block->code + (xlat_output - oldstart);
	    eob = block->code + block->size;
	}
	done = sh4_translate_instruction( pc ); 
	assert( xlat_output <= eob );
	pc += 2;
	if ( pc >= lastpc ) {
	    done = 2;
	}
    } while( !done );
    pc += (done - 2);
    if( eob - xlat_output < EPILOGUE_SIZE ) {
	uint8_t *oldstart = block->code;
	block = xlat_extend_block( xlat_output - oldstart + EPILOGUE_SIZE );
	xlat_output = block->code + (xlat_output - oldstart);
    }	
    sh4_translate_end_block(pc);

    /* Write the recovery records onto the end of the code block */
    uint32_t recovery_size = sizeof(struct xlat_recovery_record)*xlat_recovery_posn;
    uint32_t finalsize = xlat_output - block->code + recovery_size;
    if( finalsize > block->size ) {
	uint8_t *oldstart = block->code;
	block = xlat_extend_block( finalsize );
	xlat_output = block->code + (xlat_output - oldstart);
    }
    memcpy( xlat_output, xlat_recovery, recovery_size);
    block->recover_table = (xlat_recovery_record_t)xlat_output;
    block->recover_table_size = xlat_recovery_posn;
    xlat_commit_block( finalsize, pc-start );
    return block->code;
}

/**
 * Translate a linear basic block to a temporary buffer, execute it, and return
 * the result of the execution. The translation is discarded.
 */
void *sh4_translate_and_run( sh4addr_t start )
{
    unsigned char buf[65536];

    sh4addr_t pc = start;
    int done;
    xlat_output = buf;
    uint8_t *eob = xlat_output + sizeof(buf);

    sh4_translate_begin_block(pc);

    while( (done = sh4_translate_instruction( pc )) == 0 ) {
	assert( (eob - xlat_output) >= MAX_INSTRUCTION_SIZE );
	pc += 2;
    }
    pc+=2;
    sh4_translate_end_block(pc);

    void * (*code)() = (void *)buf;
    return code();
}

/**
 * "Execute" the supplied recovery record. Currently this only updates
 * sh4r.pc and sh4r.slice_cycle according to the currently executing
 * instruction. In future this may be more sophisticated (ie will
 * call into generated code).
 */
void sh4_translate_run_recovery( xlat_recovery_record_t recovery )
{
    sh4r.slice_cycle += (recovery->sh4_icount * sh4_cpu_period);
    sh4r.pc += (recovery->sh4_icount<<1);
}

void sh4_translate_unwind_stack( gboolean abort_after, unwind_thunk_t thunk )
{
    void *pc = xlat_get_native_pc();
    if( pc == NULL ) {
	// This should never happen - indicative of a bug somewhere.
	FATAL("Attempted to unwind stack, but translator is not running or stack is corrupt");
    }
    void *code = xlat_get_code( sh4r.pc );
    xlat_recovery_record_t recover = xlat_get_recovery(code, pc, TRUE);
    if( recover != NULL ) {
	// Can be null if there is no recovery necessary
	sh4_translate_run_recovery(recover);
    }
    if( thunk != NULL ) {
	thunk();
    }
    // finally longjmp back into sh4_xlat_run_slice
    longjmp(xlat_jmp_buf, 1);
} 

/**
 * Exit the current block at the end of the current instruction, flush the
 * translation cache (completely) and return control to sh4_xlat_run_slice.
 *
 * As a special case, if the current instruction is actually the last 
 * instruction in the block (ie it's in a delay slot), this function 
 * returns to allow normal completion of the translation block. Otherwise
 * this function never returns.
 *
 * Must only be invoked (indirectly) from within translated code.
 */
void sh4_translate_flush_cache()
{
    void *pc = xlat_get_native_pc();
    if( pc == NULL ) {
	// This should never happen - indicative of a bug somewhere.
	FATAL("Attempted to unwind stack, but translator is not running or stack is corrupt");
    }
    void *code = xlat_get_code( sh4r.pc );
    xlat_recovery_record_t recover = xlat_get_recovery(code, pc, TRUE);
    if( recover != NULL ) {
	// Can be null if there is no recovery necessary
	sh4_translate_run_recovery(recover);
	xlat_flush_cache();
	longjmp(xlat_jmp_buf, 1);
    } else {
	xlat_flush_cache();
	return;
    }
}

void *xlat_get_code_by_vma( sh4vma_t vma )
{
    void *result = NULL;

    if( !IS_IN_ICACHE(vma) ) {
	if( !mmu_update_icache(sh4r.pc) ) {
	    // fault - off to the fault handler
	    if( !mmu_update_icache(sh4r.pc) ) {
		// double fault - halt
		dreamcast_stop();
		ERROR( "Double fault - halting" );
		return NULL;
	    }
	}
    }
    if( sh4_icache.page_vma != -1 ) {
	result = xlat_get_code( GET_ICACHE_PHYS(vma) );
    }

    return result;
}

