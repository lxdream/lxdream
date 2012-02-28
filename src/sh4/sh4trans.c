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
#include "eventq.h"
#include "syscall.h"
#include "clock.h"
#include "dreamcast.h"
#include "sh4/sh4core.h"
#include "sh4/sh4trans.h"
#include "sh4/sh4mmio.h"
#include "sh4/mmu.h"
#include "xlat/xltcache.h"

//#define SINGLESTEP 1

/**
 * Execute a timeslice using translated code only (ie translate/execute loop)
 */
uint32_t sh4_translate_run_slice( uint32_t nanosecs ) 
{
    event_schedule( EVENT_ENDTIMESLICE, nanosecs );
    for(;;) {
        if( sh4r.event_pending <= sh4r.slice_cycle ) {
            sh4_handle_pending_events();
            if( sh4r.slice_cycle >= nanosecs )
                return nanosecs;
        }

        if( IS_SYSCALL(sh4r.pc) ) {
            uint32_t pc = sh4r.pc;
            sh4r.pc = sh4r.pr;
            sh4r.in_delay_slot = 0;
            syscall_invoke( pc );
        }

        void * (*code)() = xlat_get_code_by_vma( sh4r.pc );
        if( code != NULL ) {
            while( sh4r.xlat_sh4_mode != XLAT_BLOCK_MODE(code) ) {
                code = XLAT_BLOCK_CHAIN(code);
                if( code == NULL ) {
                    code = sh4_translate_basic_block( sh4r.pc );
                    break;
                }
            }
        } else {
            code = sh4_translate_basic_block( sh4r.pc );
        }
        code();
    }
}

uint8_t *xlat_output;
xlat_cache_block_t xlat_current_block;
struct xlat_recovery_record xlat_recovery[MAX_RECOVERY_SIZE];
uint32_t xlat_recovery_posn;

void sh4_translate_add_recovery( uint32_t icount )
{
    xlat_recovery[xlat_recovery_posn].xlat_offset = 
        ((uintptr_t)xlat_output) - ((uintptr_t)xlat_current_block->code);
    xlat_recovery[xlat_recovery_posn].sh4_icount = icount;
    xlat_recovery_posn++;
}

/**
 * Translate a linear basic block, ie all instructions from the start address
 * (inclusive) until the next branch/jump instruction or the end of the page
 * is reached.
 * @param start VMA of the block start (which must already be in the icache)
 * @return the address of the translated block
 * eg due to lack of buffer space.
 */
void * sh4_translate_basic_block( sh4addr_t start )
{
    sh4addr_t pc = start;
    sh4addr_t lastpc = (pc&0xFFFFF000)+0x1000;
    int done, i;
    xlat_current_block = xlat_start_block( GET_ICACHE_PHYS(start) );
    xlat_output = (uint8_t *)xlat_current_block->code;
    xlat_recovery_posn = 0;
    uint8_t *eob = xlat_output + xlat_current_block->size;

    if( GET_ICACHE_END() < lastpc ) {
        lastpc = GET_ICACHE_END();
    }

    sh4_translate_begin_block(pc);

    do {
        if( eob - xlat_output < MAX_INSTRUCTION_SIZE ) {
            uint8_t *oldstart = xlat_current_block->code;
            xlat_current_block = xlat_extend_block( xlat_output - oldstart + MAX_INSTRUCTION_SIZE );
            xlat_output = xlat_current_block->code + (xlat_output - oldstart);
            eob = xlat_current_block->code + xlat_current_block->size;
        }
        done = sh4_translate_instruction( pc ); 
        assert( xlat_output <= eob );
        pc += 2;
        if ( pc >= lastpc && done == 0 ) {
            done = 2;
        }
#ifdef SINGLESTEP
        if( !done ) done = 2;
#endif
    } while( !done );
    pc += (done - 2);

    // Add end-of-block recovery for post-instruction checks
    sh4_translate_add_recovery( (pc - start)>>1 ); 

    int epilogue_size = sh4_translate_end_block_size();
    uint32_t recovery_size = sizeof(struct xlat_recovery_record)*xlat_recovery_posn;
    uint32_t finalsize = (xlat_output - xlat_current_block->code) + epilogue_size + recovery_size;
    if( xlat_current_block->size < finalsize ) {
        uint8_t *oldstart = xlat_current_block->code;
        xlat_current_block = xlat_extend_block( finalsize );
        xlat_output = xlat_current_block->code + (xlat_output - oldstart);
    }	
    sh4_translate_end_block(pc);
    assert( xlat_output <= (xlat_current_block->code + xlat_current_block->size - recovery_size) );

    /* Write the recovery records onto the end of the code block */
    memcpy( xlat_output, xlat_recovery, recovery_size);
    xlat_current_block->recover_table_offset = xlat_output - (uint8_t *)xlat_current_block->code;
    xlat_current_block->recover_table_size = xlat_recovery_posn;
    xlat_current_block->xlat_sh4_mode = sh4r.xlat_sh4_mode;
    xlat_commit_block( finalsize, start, pc );
    return xlat_current_block->code;
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

/**
 * Same as sh4_translate_run_recovery, but is used to recover from a taken
 * exception - that is, it fixes sh4r.spc rather than sh4r.pc
 */
void sh4_translate_run_exception_recovery( xlat_recovery_record_t recovery )
{
    sh4r.slice_cycle += (recovery->sh4_icount * sh4_cpu_period);
    sh4r.spc += (recovery->sh4_icount<<1);
}    

void sh4_translate_exit_recover( )
{
    void *code = xlat_get_code_by_vma( sh4r.pc );
    if( code != NULL ) {
        uint32_t size = xlat_get_code_size( code );
        void *pc = xlat_get_native_pc( code, size );
        if( pc != NULL ) {
            // could be null if we're not actually running inside the translator
            xlat_recovery_record_t recover = xlat_get_pre_recovery(code, pc);
            if( recover != NULL ) {
                // Can be null if there is no recovery necessary
                sh4_translate_run_recovery(recover);
            }
        }
    }
}

void sh4_translate_exception_exit_recover( )
{
    void *code = xlat_get_code_by_vma( sh4r.spc );
    if( code != NULL ) {
        uint32_t size = xlat_get_code_size( code );
        void *pc = xlat_get_native_pc( code, size );
        if( pc != NULL ) {
            // could be null if we're not actually running inside the translator
            xlat_recovery_record_t recover = xlat_get_pre_recovery(code, pc);
            if( recover != NULL ) {
                // Can be null if there is no recovery necessary
                sh4_translate_run_exception_recovery(recover);
            }
        }
    }
    
}

void FASTCALL sh4_translate_breakpoint_hit(uint32_t pc)
{
    if( sh4_starting && sh4r.slice_cycle == 0 && pc == sh4r.pc ) {
        return;
    }
    sh4_core_exit( CORE_EXIT_BREAKPOINT );
}

void * FASTCALL xlat_get_code_by_vma( sh4vma_t vma )
{
    void *result = NULL;

    if( IS_IN_ICACHE(vma) ) {
        return xlat_get_code( GET_ICACHE_PHYS(vma) );
    }

    if( IS_SYSCALL(vma) ) {
        // lxdream hook
        return NULL;
    }

    if( !mmu_update_icache(vma) ) {
        // fault - off to the fault handler
        if( !mmu_update_icache(sh4r.pc) ) {
            // double fault - halt
            ERROR( "Double fault - halting" );
            sh4_core_exit(CORE_EXIT_HALT);
            return NULL;
        }
    }

    assert( IS_IN_ICACHE(sh4r.pc) );
    result = xlat_get_code( GET_ICACHE_PHYS(sh4r.pc) );
    return result;
}

/**
 * Crashdump translation information.
 *
 * Print out the currently executing block (if any), in source and target
 * assembly.
 *
 * Note: we want to be _really_ careful not to cause a second-level crash
 * at this point (e.g. if the lookup tables are corrupted...)
 */
void sh4_translate_crashdump()
{
    if( !IS_IN_ICACHE(sh4r.pc) ) {
        /** If we're crashing due to an icache lookup failure, we'll probably
         * hit this case - just complain and return.
         */
        fprintf( stderr, "** SH4 PC not in current instruction region **\n" );
        return;
    }
    uint32_t pma = GET_ICACHE_PHYS(sh4r.pc);
    void *code = xlat_get_code( pma );
    if( code == NULL ) {
        fprintf( stderr, "** No translated block for current SH4 PC **\n" );
        return;
    }

    /* Sanity check on the code pointer */
    if( !xlat_is_code_pointer(code) ) {
        fprintf( stderr, "** Possibly corrupt translation cache **\n" );
        return;
    }

    void *native_pc = xlat_get_native_pc( code, xlat_get_code_size(code) );
    sh4_translate_disasm_block( stderr, code, sh4r.pc, native_pc );
}

/**
 * Dual-dump the translated block and original SH4 code for the basic block
 * starting at sh4_pc. If there is no translated block, this prints an error
 * and returns.
 */
void sh4_translate_dump_block( uint32_t sh4_pc )
{
    if( !IS_IN_ICACHE(sh4_pc) ) {
        fprintf( stderr, "** Address %08x not in current instruction region **\n", sh4_pc );
        return;
    }
    uint32_t pma = GET_ICACHE_PHYS(sh4_pc);
    void *code = xlat_get_code( pma );
    if( code == NULL ) {
        fprintf( stderr, "** No translated block for address %08x **\n", sh4_pc );
        return;
    }
    sh4_translate_disasm_block( stderr, code, sh4_pc, NULL );
}

void sh4_translate_dump_cache_by_activity( unsigned int topN )
{
    struct xlat_block_ref blocks[topN];
    topN = xlat_get_cache_blocks_by_activity(blocks, topN);
    unsigned int i;
    for( i=0; i<topN; i++ ) {
        fprintf( stderr, "0x%08X (%p): %d \n", blocks[i].pc, blocks[i].block->code, blocks[i].block->active);
        sh4_translate_disasm_block( stderr, blocks[i].block->code, blocks[i].pc, NULL );
        fprintf( stderr, "\n" );
    }
}
