/**
 * $Id: livevar.h 931 2008-10-31 02:57:59Z nkeynes $
 * 
 * Live variable analysis
 *
 * Copyright (c) 2009 Nathan Keynes.
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

#include "xlat/xir.h"
#include "xlat/xiropt.h"


gboolean live_range_calculate( xir_op_t start, xir_op_t end, 
                               struct live_range *live_ranges, unsigned int live_ranges_size )
{
    struct live_range *current[MAX_REGISTERS];
    struct live_range *range_next = live_ranges;
    struct live_range *range_end = live_ranges + live_ranges_size;
    xir_offset_t position = 0;
    xir_offset_t last_exc = 0;
    xir_op_t it;
    
    memset( current, 0, sizeof(current) );
    
    while( it != end ) {
        int reg0 = -1;
        int reg1 = -1;
        
        if( it->exc != NULL ) {
            // Track when the last possible exception was
            last_exc = position;
        }
        
        if( XOP_READS_REG1(it) ) { // Update live-range for op0
            reg0 = XOP_REG1(it);
            if( current[reg0] == NULL ) {
                current[reg0] = range_next++;
                if( current[reg0] == range_end )
                    return FALSE;
                current[reg0]->start = it;
                current[reg0]->offset = position;
                current[reg0]->writeback = FALSE; // register is already coherent
            }
            current[reg0]->end = it;
            current[reg0]->length = position - current[reg0]->offset;            
        } 

        if( XOP_READS_REG2(it) ) {
            reg1 = XOP_REG2(it);
            if( current[reg1] == NULL ) {
                current[reg1] = range_next++;
                if( current[reg1] == range_end )
                    return FALSE;
                current[reg1]->start = it;
                current[reg1]->offset = position;
            }
            current[reg1]->end = it;
            current[reg1]->length = position - current[reg1]->offset;
        } // op1 is Use-only
        
        if( XOP_WRITES_REG1(it) ) {
        }
        
        if( XOP_WRITES_REG2(it)  ) {
            int reg = XOP_REG2(it);
            if( last_exc < current[reg].end ) {
                // Value is dead and doesn't need to be spilled.
                current[reg].writeback = FALSE;
            }
            // Kill last range for op1 if we're not using it. Otherwise
            // this is just a continuation.
            current[reg] = range_next++;
            if( current[reg] == range_end )
                return FALSE;
            current[reg]->start = it;
            current[reg]->offset = position;
            current[reg]->end = it;
            current[reg]->length = 0;
            current[reg]->writeback = TRUE;
        }

        it = it->next;
        position++;
    }
    return TRUE;
}