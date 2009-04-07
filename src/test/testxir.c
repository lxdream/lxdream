/**
 * $Id: testsh4x86.c 988 2009-01-15 11:23:20Z nkeynes $
 *
 * Test XIR internals
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

#include <assert.h>
#include <stdlib.h>
#include "xlat/xir.h"

void test_shuffle()
{
    struct xir_op op[64];
    struct xir_basic_block bb;
    xir_basic_block_t xbb = &bb;
    bb.ir_alloc_begin = bb.ir_begin = bb.ir_end = bb.ir_ptr = &op[0];
    bb.ir_alloc_end = &op[64];
    
    XOP2I( OP_SHUFFLE, 0x1243, REG_TMP1 );
    XOP2I( OP_SHUFFLE, 0x3412, REG_TMP1 );
    XOP2I( OP_SHUFFLE, 0x1243, REG_TMP1 );
    XOP2I( OP_SHUFFLE, 0x3412, REG_TMP1 );
    XOP2I( OP_SHUFFLE, 0x1234, REG_TMP1 );
    XOP2I( OP_SHUFFLE, 0x1111, REG_TMP2 );
    XOP2I( OP_SHUFFLE, 0x0123, REG_TMP1 );
    XOP1I( OP_BR, 0x8C001000 );
    (bb.ir_ptr-1)->next = NULL;
    bb.ir_end = bb.ir_ptr-1;
    
    assert( xir_shuffle_imm32( 0x2134, 0x12345678) == 0x34125678 );
    assert( xir_shuffle_imm32( 0x1243, 0x12345678) == 0x12347856 );
    assert( xir_shuffle_imm32( 0x3412, 0x12345678) == 0x56781234 );
    
    xir_shuffle_op( op[0].operand[0].value.i, &op[1] );
    assert( op[1].operand[0].value.i == 0x4312 ); 
    xir_shuffle_op( op[1].operand[0].value.i, &op[2] );
    assert( op[2].operand[0].value.i == 0x4321 );
    
    assert( xir_shuffle_lower_size( &op[0] ) == 9);
    assert( xir_shuffle_lower_size( &op[1] ) == 8);
    assert( xir_shuffle_lower_size( &op[3] ) == 4);
    assert( xir_shuffle_lower_size( &op[4] ) == 0);
    assert( xir_shuffle_lower_size( &op[5] ) == 12);
    assert( xir_shuffle_lower_size( &op[6] ) == 1);
    xir_shuffle_lower( xbb, &op[0], REG_TMP3, REG_TMP4 ); 
    xir_shuffle_lower( xbb, &op[1], REG_TMP3, REG_TMP4 );
    xir_shuffle_lower( xbb, &op[3], REG_TMP3, REG_TMP4 );
    xir_shuffle_lower( xbb, &op[4], REG_TMP3, REG_TMP4 );
    xir_shuffle_lower( xbb, &op[5], REG_TMP3, REG_TMP4 );
    xir_shuffle_lower( xbb, &op[6], REG_TMP3, REG_TMP4 );
    xir_dump_block( &op[0], NULL );
}




int main( int argc, char *argv[] )
{
    test_shuffle();
    
    return 0;
}