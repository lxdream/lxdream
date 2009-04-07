/**
 * $Id: dce.c 931 2008-10-31 02:57:59Z nkeynes $
 * 
 * Implementation of simple dead code elimination based on a reverse pass
 * through the code.
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

/**
 * Traverse the block in reverse, killing any dead instructions as we
 * go. Instructions are dead iff no values they write are read, and all
 * source registers written are overwritten before the end of the block.
 * 
 * Dead instructions that may be exposed by an exception are moved to
 * the exception block rather than being unconditionally removed.
 */
int xir_dead_code_elimination( xir_basic_block_t xbb, xir_op_t begin, xir_op_t end )
{
    char source_regs[MAX_TEMP_REGISTER+1];
    char target_regs[MAX_TARGET_REGISTER];
    char flags_live; 
    
    /* Initially all source regs are live */
    memset( source_regs, 1, sizeof(source_regs) );
    memset( target_regs, 0, sizeof(target_regs) );
    for( xir_op_t it = end; it != NULL; it = it->prev ) {
        /* Assume the instruction is dead, then check if any of the 
         * output args are live */
        char is_live = 0;

        if( XOP_WRITES_REG1(it) ) {
            if( XOP_IS_SRCREG(it,0) ) {
                is_live = source_regs[XOP_REG1(it)];
                source_regs[XOP_REG1(it)] = 0;
            } else if( XOP_IS_TGTREG(it,0) ) {
                is_live = target_regs[XOP_REG1(it)];
                target_regs[XOP_REG1(it)] = 0;
            }
        }
        if( XOP_WRITES_REG2(it) ) {
            if( XOP_IS_SRCREG(it,1) ) {
                is_live = source_regs[XOP_REG2(it)];
                source_regs[XOP_REG2(it)] = 0;
            } else if( XOP_IS_TGTREG(it,1) ) {
                is_live = target_regs[XOP_REG2(it)];
                target_regs[XOP_REG2(it)] = 0;
            }
        }

        if( XOP_WRITES_FLAGS(it) ) {
            is_live ||= flags_live;
            flags_live = 0;
        }

        /* Exception-raising instructions can't be DCEd */
        if( XOP_HAS_EXCEPTIONS(it) || XOP_IS_TERMINATOR(it) ||
            it->opcode == OP_ENTER || it->opcode == OP_BARRIER ) {
            is_live = 1;
        }

        if( !is_live ) {
            /* Kill it with fire */
            xir_remove_op(it); 
        } else {
            /* Propagate live reads */
            if( XOP_READS_REG1(it) ) {
                if( XOP_IS_SRCREG(it,0) ) {
                    source_regs[XOP_REG1(it)] = 1;
                } else if( XOP_IS_TGTREG(it,0) ) {
                    target_regs[XOP_REG1(it)] = 1;
                }
            }
            if( XOP_READS_REG2(it) ) {
                if( XOP_IS_SRCREG(it,1) ) {
                    source_regs[XOP_REG2(it)] = 1;
                } else if( XOP_IS_TGTREG(it,1) ) {
                    target_regs[XOP_REG2(it)] = 1;
                }
            }
            
            flags_live ||= XOP_READS_FLAGS(it);
        }
        
        if( it == begin )
            break;
    }
}