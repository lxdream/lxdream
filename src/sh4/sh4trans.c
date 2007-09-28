/**
 * $Id: sh4trans.c,v 1.5 2007-09-28 07:27:20 nkeynes Exp $
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
#include "sh4core.h"
#include "sh4trans.h"
#include "xltcache.h"

/**
 * Execute a timeslice using translated code only (ie translate/execute loop)
 * Note this version does not support breakpoints
 */
uint32_t sh4_xlat_run_slice( uint32_t nanosecs ) 
{
    int i;
    sh4r.slice_cycle = 0;

    if( sh4r.sh4_state != SH4_STATE_RUNNING ) {
	if( sh4r.event_pending < nanosecs ) {
	    sh4r.sh4_state = SH4_STATE_RUNNING;
	    sh4r.slice_cycle = sh4r.event_pending;
	}
    }

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
	
	if( code ) { // fast path
	    code = code();
	} else {
	    if( sh4r.pc > 0xFFFFFF00 ) {
		syscall_invoke( sh4r.pc );
		sh4r.in_delay_slot = 0;
		sh4r.pc = sh4r.pr;
	    }

	    code = xlat_get_code(sh4r.pc);
	    if( code == NULL ) {
		code = sh4_translate_basic_block( sh4r.pc );
	    }
	    code = code();
	}
    }

    if( sh4r.sh4_state != SH4_STATE_STANDBY ) {
	TMU_run_slice( nanosecs );
	SCIF_run_slice( nanosecs );
    }
    return nanosecs;
}

uint8_t *xlat_output;

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
    int done;
    xlat_cache_block_t block = xlat_start_block( start );
    xlat_output = (uint8_t *)block->code;
    uint8_t *eob = xlat_output + block->size;
    sh4_translate_begin_block(pc);

    do {
	if( eob - xlat_output < MAX_INSTRUCTION_SIZE ) {
	    uint8_t *oldstart = block->code;
	    block = xlat_extend_block();
	    xlat_output = block->code + (xlat_output - oldstart);
	    eob = block->code + block->size;
	}
	done = sh4_x86_translate_instruction( pc ); 
	pc += 2;
    } while( !done );
    pc += (done - 2);
    sh4_translate_end_block(pc);
    xlat_commit_block( xlat_output - block->code, pc-start );
    return block->code;
}

/**
 * Translate a linear basic block to a temporary buffer, execute it, and return
 * the result of the execution. The translation is discarded.
 */
void *sh4_translate_and_run( sh4addr_t start )
{
    char buf[65536];

    uint32_t pc = start;
    int done;
    xlat_output = buf;
    uint8_t *eob = xlat_output + sizeof(buf);

    sh4_translate_begin_block(pc);

    while( (done = sh4_x86_translate_instruction( pc )) == 0 ) {
	assert( (eob - xlat_output) >= MAX_INSTRUCTION_SIZE );
	pc += 2;
    }
    pc+=2;
    sh4_translate_end_block(pc);

    void * (*code)() = (void *)buf;
    return code();
}
