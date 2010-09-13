/**
 * $Id$
 * 
 * Provides the implementation for the ia32 ABI variant 
 * (eg prologue, epilogue, and calling conventions). Stack frame is
 * aligned on 16-byte boundaries for the benefit of OS X (which 
 * requires it).
 * 
 * Note: These should only be included from x86op.h
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

#define REG_ARG1 REG_EAX
#define REG_ARG2 REG_EDX
#define REG_RESULT1 REG_EAX
#define MAX_REG_ARG 2

static inline void decode_address( uintptr_t base, int addr_reg )
{
    MOVL_r32_r32( addr_reg, REG_ECX );
    SHRL_imm_r32( 12, REG_ECX ); 
    MOVP_sib_rptr( 2, REG_ECX, -1, base, REG_ECX );
}

/**
 * Note: clobbers ECX to make the indirect call - this isn't usually
 * a problem since the callee will generally clobber it anyway.
 */
static inline void CALL_ptr( void *ptr )
{
    MOVP_immptr_rptr( (uintptr_t)ptr, REG_ECX );
    CALL_r32(REG_ECX);
}

#ifdef HAVE_FASTCALL
static inline void CALL1_ptr_r32( void *ptr, int arg1 )
{
    if( arg1 != REG_ARG1 ) {
        MOVL_r32_r32( arg1, REG_ARG1 );
    }
    CALL_ptr(ptr);
}

static inline void CALL1_r32disp_r32( int preg, uint32_t disp, int arg1 )
{
    if( arg1 != REG_ARG1 ) {
        MOVL_r32_r32( arg1, REG_ARG1 );
    }
    CALL_r32disp(preg, disp);
}

static inline void CALL2_ptr_r32_r32( void *ptr, int arg1, int arg2 )
{
    if( arg2 != REG_ARG2 ) {
        MOVL_r32_r32( arg2, REG_ARG2 );
    }
    if( arg1 != REG_ARG1 ) {
        MOVL_r32_r32( arg1, REG_ARG1 );
    }
    CALL_ptr(ptr);
}

static inline void CALL2_r32disp_r32_r32( int preg, uint32_t disp, int arg1, int arg2 )
{
    if( arg2 != REG_ARG2 ) {
        MOVL_r32_r32( arg2, REG_ARG2 );
    }
    if( arg1 != REG_ARG1 ) {
        MOVL_r32_r32( arg1, REG_ARG1 );
    }
    CALL_r32disp(preg, disp);
}

#define CALL3_r32disp_r32_r32_r32(preg,disp,arg1,arg2,arg3) CALL2_r32disp_r32_r32(preg,disp,arg1,arg2)

#else
static inline void CALL1_ptr( void *ptr, int arg1 )
{
    SUBL_imms_r32( 12, REG_ESP );
    PUSH_r32(arg1);
    CALL_ptr(ptr);
    ADDL_imms_r32( 16, REG_ESP );
}

static inline void CALL1_r32disp_r32( int preg, uint32_t disp, int arg1 )
{
    SUBL_imms_r32( 12, REG_ESP );
    PUSH_r32(arg1);
    CALL_r32disp(preg, disp);
    ADDL_imms_r32( 16, REG_ESP );
}

static inline void CALL2_ptr_r32_r32( void *ptr, int arg1, int arg2 )
{
    SUBL_imms_r32( 8, REG_ESP );
    PUSH_r32(arg2);
    PUSH_r32(arg1);
    CALL_ptr(ptr);
    ADDL_imms_r32( 16, REG_ESP );
}

static inline void CALL2_r32disp_r32_r32( int preg, uint32_t disp, int arg1, int arg2 )
{
    SUBL_imms_r32( 8, REG_ESP );
    PUSH_r32(arg2);
    PUSH_r32(arg1);
    CALL_r32disp(preg, disp);
    ADDL_imms_r32( 16, REG_ESP );
}

static inline void CALL3_r32disp_r32_r32_r32( int preg, uint32_t disp, int arg1, int arg2, int arg3 )
{
    SUBL_imms_r32( 8, REG_ESP );
    PUSH_r32(arg2);
    PUSH_r32(arg1);
    MOVL_rspdisp_r32( 16, REG_EAX );
    MOVL_r32_rspdisp( R_EAX, 8 );
    CALL_r32disp(preg,disp);
    ADDL_imms_r32( 16, REG_ESP );
}

#endif

#define PROLOGUE_SIZE 9

/**
 * Emit the 'start of block' assembly. Sets up the stack frame and save
 * SI/DI as required
 * Allocates 8 bytes for local variables, which also has the convenient
 * side-effect of aligning the stack.
 */
static inline void emit_prologue( )
{
    PUSH_r32(REG_EBP);
    SUBL_imms_r32( 8, REG_ESP ); 
    MOVP_immptr_rptr( ((uint8_t *)&sh4r) + 128, REG_EBP );
}

static inline void emit_epilogue( )
{
    ADDL_imms_r32( 8, REG_ESP );
    POP_r32(REG_EBP);
}

