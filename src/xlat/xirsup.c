/**
 * $Id: xirsup.c 931 2008-10-31 02:57:59Z nkeynes $
 *
 * XIR support functions and transformations for the convenience of other
 * passes/targets. 
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

/**************************** Shuffle ****************************/
/**
 * Shuffle is a high-level instruction that rearranges bytes in an operand
 * according to an immediate pattern. This can be encoded directly on x86
 * using SSE/MMX registers, otherwise it needs to be lowered first. 
 */

/**
 * Apply a shuffle directly to the given operand, and return the result
 */ 
uint32_t xir_shuffle_imm32( uint32_t shuffle, uint32_t operand )
{
    int i=0,j;
    uint32_t tmp = shuffle;
    uint32_t result = 0;
    for( i=0; i<4; i++ ) {
        j = (tmp & 0x0F)-1;
        tmp >>= 4;
        if( j >= 0 && j < 4 ) {
            j = (operand >> ((3-j)<<3)) & 0xFF;
            result |= (j << (i<<3));
        }
    }
    return result;
}

/**
 * Apply a shuffle transitively to the operation (which must also be a shuffle).
 * For example, given the sequence
 *   op1: shuffle 0x1243, r12
 *   op2: shuffle 0x3412, r12
 * xir_trans_shuffle( 0x1243, op2 ) can be used to replace op2 wih
 *        shuffle 0x4312, r12
 */
void xir_shuffle_op( uint32_t shuffle, xir_op_t it )
{
    int i=0,j;
    uint32_t in1 = shuffle;
    uint32_t in2 = it->operand[0].value.i;
    uint32_t result = 0;
    for( i=0; i<4; i++ ) {
        j = (in2 & 0x0F)-1;
        in2 >>= 4;
        if( j >= 0 && j < 4 ) {
            j = (in1 >> ((3-j)<<2)) & 0x0F;
            result |= (j << (i<<2));
        }
    }
    it->operand[0].value.i = result;
}

/**
 * Return the cost of lowering the specified shuffle as the number of instructions
 * involved.
 */ 
int xir_shuffle_lower_size( xir_op_t it )
{
    int mask_for_shift[7] = {0,0,0,0,0,0,0}; /* -3 .. 0 .. +3 */
    int arg = it->operand[0].value.i, i;
    int icount=0, found = 0;
    
    if( arg == 0x1234 ) {
        return 0;
    }
    
    /* Figure out the shift (in bytes) for each sub-byte and construct the mask/shift array */
    for( i=0; i<4; i++ ) {
        int val = (arg&0x0F);
        if( val >= 1 && val <= 4 ) { 
            int shift = val - (4-i);
            mask_for_shift[shift+3] |= ( (0xFF) << (i<<3) );
        }
        arg >>= 4;
    }

    for( i=-3; i<4; i++ ) {
        if( mask_for_shift[i+3] != 0 ) {
            uint32_t maxmask = 0xFFFFFFFF;
            if( i < 0 ) {
                icount++;
                maxmask >>= ((-i)<<3);
            } else if( i > 0 ) {
                icount++;
                maxmask <<= (i<<3);
            }
            if( mask_for_shift[i+3] != maxmask ) {
                icount++;
            }
            if( found != 0 ) {
                icount += 2;
            }
            found++;
        }
    }
    return icount;       
}

/**
 * Transform a shuffle instruction into an equivalent sequence of shifts, and
 * logical operations.
 */
xir_op_t xir_shuffle_lower( xir_basic_block_t xbb, xir_op_t it, int tmp1, int tmp2 )
{
    int mask_for_shift[7] = {0,0,0,0,0,0,0}; /* -3 .. 0 .. +3 */
    int arg = it->operand[0].value.i, i, first=3, last=-3;

    if( arg == 0x1234 ) { /* Identity - NOP */
        it->opcode = OP_NOP;
        it->operand[0].form = NO_OPERAND;
        it->operand[1].form = NO_OPERAND;
        return it;
    }
    
    
    /* Figure out the shift (in bytes) for each sub-byte and construct the mask/shift array */
    for( i=0; i<4; i++ ) {
        int val = (arg&0x0F);
        if( val >= 1 && val <= 4 ) { 
            int shift = val - (4-i);
            mask_for_shift[shift+3] |= ( (0xFF) << (i<<3) );
            if( shift > last ) { 
                last = shift;
            }
            if( shift < first ) {
                first = shift;
            }
        }
        arg >>= 4;
    }

    int shifterform = it->operand[1].form, shifterval = it->operand[1].value.i;
    xir_op_t seq = xbb->ir_ptr;

    for( i=first; i<=last; i++ ) {
        if( mask_for_shift[i+3] != 0 ) {
            uint32_t maxmask = 0xFFFFFFFF;
            if( first != i ) {
                shifterform = TEMP_OPERAND;
                if( last == i ) {
                    shifterval = tmp1;
                } else {
                    shifterval = tmp2;
                    xir_append_op2( xbb, OP_MOV, TEMP_OPERAND, tmp1, shifterform, shifterval );
                }
            }
            if( i < 0 ) {
                xir_append_op2( xbb, OP_SLR, IMMEDIATE_OPERAND, (-i)<<3, shifterform, shifterval );
                maxmask >>= ((-i)<<3);
            } else if( i > 0 ) {
                xir_append_op2( xbb, OP_SLL, IMMEDIATE_OPERAND, i<<3, shifterform, shifterval );
                maxmask <<= (i<<3);
            }
            if( mask_for_shift[i+3] != maxmask ) {
                xir_append_op2( xbb, OP_AND, IMMEDIATE_OPERAND, mask_for_shift[i+3], shifterform, shifterval );
            }
            if( first != i ) {
                xir_append_op2( xbb, OP_OR, shifterform, shifterval, it->operand[1].form, it->operand[1].value.i );
            }
        }
    }
    
    /* Replace original shuffle with either a temp move or a nop */
    if( first != last ) {
        it->opcode = OP_MOV;
        it->operand[0].form = it->operand[1].form;
        it->operand[0].value.i = it->operand[1].value.i;
        it->operand[1].form = TEMP_OPERAND;
        it->operand[1].value.i = tmp1;
    } else {
        it->opcode = OP_NOP;
        it->operand[0].form = NO_OPERAND;
        it->operand[1].form = NO_OPERAND;
    }

    /* Finally insert the new sequence after the original op */
    if( xbb->ir_ptr != seq ) {
        xir_op_t last = xbb->ir_ptr-1;
        last->next = it->next;
        it->next = seq;
        seq->prev = it;
        if( last->next != 0 ) {
            last->next->prev = last;
        }
        return last;
    } else {
        return it;
    }
}