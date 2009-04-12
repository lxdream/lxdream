/**
 * $Id: regalloc.h 931 2008-10-31 02:57:59Z nkeynes $
 * 
 * Register allocation based on simple linear scan
 *
 * Copyright (c) 2008 Nathan Keynes.
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

#include <stdlib.h>
#include <string.h>
#include "lxdream.h"
#include "xir.h"


/**
 * Promote all source registers to temporaries, in preparation for regalloc.
 * In case of partial aliasing, we flush back to memory before reloading (this
 * needs to be improved)
 */
void xir_promote_source_registers( xir_basic_block_t xbb, xir_op_t start, xir_op_t end )
{
    struct temp_reg {
        gboolean is_dirty;
        xir_op_t last_access;
    };

    struct temp_reg all_temp_regs[MAX_TEMP_REGISTER+1];
    
    
    /* -1 if byte is not allocated to a temporary, otherwise temp id */
    int16_t source_regs[MAX_SOURCE_REGISTER+1];
    
    memset( source_regs, -1, sizeof(source_regs) );
    memset( all_temp_regs, 0, sizeof(all_temp_regs) );
    
    for( xir_op_t it = start; it != NULL; it = it->next ) {
        if( XOP_IS_SRC(it,0) ) {
            int r = XOP_REG(it,0);
            int s = XOP_OPSIZE(it,0);
            int t = source_regs[r];
            if( t == -1 ) {
                t = xir_alloc_temp_reg( xbb, XOP_OPTYPE(it,0), r );
                source_regs[r] = t;
                if( XOP_READS_OP1(it) ) {
                    xir_insert_op( XOP2ST( OP_MOV, r, t ), it );
                }
            }
            it->operand[0].form = TEMP_OPERAND;
            it->operand[0].value.i = t;
            all_temp_regs[t].last_access = it;
            all_temp_regs[t].is_dirty |= XOP_WRITES_OP1(it);
        }
        
        if( XOP_IS_SRC(it,1) ) {
            int r = XOP_REG(it,1);
            int s = XOP_OPSIZE(it,1);
            int t = source_regs[r];
            if( t == -1 ) {
                t = xir_alloc_temp_reg( xbb, XOP_OPTYPE(it,1), r );
                source_regs[r] = t;
                if( XOP_READS_OP2(it) ) {
                    xir_insert_op( XOP2ST( OP_MOV, r, t ), it );
                }
            }
            it->operand[1].form = TEMP_OPERAND;
            it->operand[1].value.i = t;
            all_temp_regs[t].last_access = it;
            all_temp_regs[t].is_dirty |= XOP_WRITES_OP2(it);
        }
     
        if( it == end )
            break;
    }
    
    for( int i=0; i<xbb->next_temp_reg; i++ ) {
        if( all_temp_regs[i].last_access != NULL && all_temp_regs[i].is_dirty ) {
            xir_insert_op( XOP2TS( OP_MOV, i, xbb->temp_regs[i].home_register ), 
                           all_temp_regs[i].last_access->next );
        }
    }
}


/**
 * General constraints:
 *   Temporaries can't be spilled, must be held in a target register
 *   
 * 3 categories of target register:
 *    Argument (ie EAX/EDX for ia32): assign call arguments to appropriate regs
 *        first. Any available lifetime rolls over to the general volatile regs
 *    Volatile (caller-saved): Assign variables with lifetime between calls first
 *    Non-volatile (callee-saved): Everything else with cost-balancing (each
 *        register used costs the equivalent of a spill/unspill pair)
 * 
 * For x86, argument regs are EAX + EDX, ECX is volatile, and ESI/EDI/EBX are non-volatile.
 * For x86-64, arguments regs are EDI + ESI, EBX, R8-R11 are volatile, and R12-R15 are non-volatile.
 * (ESP+EBP are reserved either way)
 */