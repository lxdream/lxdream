/**
 * $Id$
 * 
 * Provides the implementation for the AMD64 ABI (eg prologue, epilogue, and
 * calling conventions)
 *
 * Copyright (c) 2007 Nathan Keynes.
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

#define REG_ARG1 REG_RDI
#define REG_ARG2 REG_RSI
#define REG_ARG3 REG_RDX
#define REG_RESULT1 REG_RAX
#define MAX_REG_ARG 3  /* There's more, but we don't use more than 3 here anyway */

static inline void decode_address( uintptr_t base, int addr_reg )
{
    MOVL_r32_r32( addr_reg, REG_ECX );
    SHRL_imm_r32( 12, REG_ECX ); 
    MOVP_immptr_rptr( base, REG_RDI );
    MOVP_sib_rptr( 3, REG_RCX, REG_RDI, 0, REG_RCX );
}

/**
 * Note: clobbers ECX to make the indirect call - this isn't usually
 * a problem since the callee will generally clobber it anyway.
 * Size: 12 bytes
 */
static inline void CALL_ptr( void *ptr )
{
    MOVP_immptr_rptr( (uintptr_t)ptr, REG_ECX );
    CALL_r32(REG_ECX);
}

static inline void CALL1_ptr_r32( void *ptr, int arg1 )
{
    if( arg1 != REG_ARG1 ) {
        MOVQ_r64_r64( arg1, REG_ARG1 );
    }
    CALL_ptr(ptr);
}

static inline void CALL1_r32disp_r32( int preg, uint32_t disp, int arg1 )
{
    if( arg1 != REG_ARG1 ) {
        MOVQ_r64_r64( arg1, REG_ARG1 );
    }
    CALL_r32disp(preg, disp);    
}

static inline void CALL2_ptr_r32_r32( void *ptr, int arg1, int arg2 )
{
    if( arg2 != REG_ARG2 ) {
        MOVQ_r64_r64( arg2, REG_ARG2 );
    }
    if( arg1 != REG_ARG1 ) {
        MOVQ_r64_r64( arg1, REG_ARG1 );
    }
    CALL_ptr(ptr);
}

static inline void CALL2_r32disp_r32_r32( int preg, uint32_t disp, int arg1, int arg2 )
{
    if( arg2 != REG_ARG2 ) {
        MOVQ_r64_r64( arg2, REG_ARG2 );
    }
    if( arg1 != REG_ARG1 ) {
        MOVQ_r64_r64( arg1, REG_ARG1 );
    }
    CALL_r32disp(preg, disp);    
}

static inline void CALL3_r32disp_r32_r32_r32( int preg, uint32_t disp, int arg1, int arg2, int arg3 )
{
    if( arg3 != REG_ARG3 ) {
        MOVQ_r64_r64( arg3, REG_ARG3 );
    }
    if( arg2 != REG_ARG2 ) {
        MOVQ_r64_r64( arg2, REG_ARG2 );
    }
    if( arg1 != REG_ARG1 ) {
        MOVQ_r64_r64( arg1, REG_ARG1 );
    }
    CALL_r32disp(preg, disp);    
}

#define PROLOGUE_SIZE 15

/**
 * Emit the 'start of block' assembly. Sets up the stack frame and save
 * SI/DI as required
 */
static inline void emit_prologue( )
{
    PUSH_r32(REG_RBP);
    SUBQ_imms_r64( 16, REG_RSP ); 
    MOVP_immptr_rptr( ((uint8_t *)&sh4r) + 128, REG_EBP );
}

static inline void emit_epilogue( )
{
    ADDQ_imms_r64( 16, REG_RSP );
    POP_r32(REG_RBP);
}
