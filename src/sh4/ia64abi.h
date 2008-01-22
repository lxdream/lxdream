/**
 * $Id$
 * 
 * Provides the implementation for the ia32 ABI (eg prologue, epilogue, and
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

#ifndef __lxdream_x86_64abi_H
#define __lxdream_x86_64abi_H 1

#include <unwind.h>

#define load_ptr( reg, ptr ) load_imm64( reg, (uint64_t)ptr );
    
/**
 * Note: clobbers EAX to make the indirect call - this isn't usually
 * a problem since the callee will usually clobber it anyway.
 * Size: 12 bytes
 */
#define CALL_FUNC0_SIZE 12
static inline void call_func0( void *ptr )
{
    load_imm64(R_EAX, (uint64_t)ptr);
    CALL_r32(R_EAX);
}

#define CALL_FUNC1_SIZE 14
static inline void call_func1( void *ptr, int arg1 )
{
    MOV_r32_r32(arg1, R_EDI);
    call_func0(ptr);
}

#define CALL_FUNC2_SIZE 16
static inline void call_func2( void *ptr, int arg1, int arg2 )
{
    MOV_r32_r32(arg1, R_EDI);
    MOV_r32_r32(arg2, R_ESI);
    call_func0(ptr);
}

#define MEM_WRITE_DOUBLE_SIZE 35
/**
 * Write a double (64-bit) value into memory, with the first word in arg2a, and
 * the second in arg2b
 */
static inline void MEM_WRITE_DOUBLE( int addr, int arg2a, int arg2b )
{
    PUSH_r32(arg2b);
    PUSH_r32(addr);
    call_func2(sh4_write_long, addr, arg2a);
    POP_r32(R_EDI);
    POP_r32(R_ESI);
    ADD_imm8s_r32(4, R_EDI);
    call_func0(sh4_write_long);
}

#define MEM_READ_DOUBLE_SIZE 43
/**
 * Read a double (64-bit) value from memory, writing the first word into arg2a
 * and the second into arg2b. The addr must not be in EAX
 */
static inline void MEM_READ_DOUBLE( int addr, int arg2a, int arg2b )
{
    REXW(); SUB_imm8s_r32( 8, R_ESP );
    PUSH_r32(addr);
    call_func1(sh4_read_long, addr);
    POP_r32(R_EDI);
    PUSH_r32(R_EAX);
    ADD_imm8s_r32(4, R_EDI);
    call_func0(sh4_read_long);
    MOV_r32_r32(R_EAX, arg2b);
    POP_r32(arg2a);
    REXW(); ADD_imm8s_r32( 8, R_ESP );
}


/**
 * Emit the 'start of block' assembly. Sets up the stack frame and save
 * SI/DI as required
 */
void sh4_translate_begin_block( sh4addr_t pc ) 
{
    PUSH_r32(R_EBP);
    /* mov &sh4r, ebp */
    load_ptr( R_EBP, &sh4r );
    
    sh4_x86.in_delay_slot = FALSE;
    sh4_x86.priv_checked = FALSE;
    sh4_x86.fpuen_checked = FALSE;
    sh4_x86.branch_taken = FALSE;
    sh4_x86.backpatch_posn = 0;
    sh4_x86.recovery_posn = 0;
    sh4_x86.block_start_pc = pc;
    sh4_x86.tlb_on = IS_MMU_ENABLED();
    sh4_x86.tstate = TSTATE_NONE;
}

/**
 * Exit the block with sh4r.pc already written
 */
void exit_block_pcset( sh4addr_t pc )
{
    load_imm32( R_ECX, ((pc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADD_r32_sh4r( R_ECX, REG_OFFSET(slice_cycle) );    // 6
    load_spreg( R_EAX, R_PC );
    if( sh4_x86.tlb_on ) {
	call_func1(xlat_get_code_by_vma,R_EAX);
    } else {
	call_func1(xlat_get_code,R_EAX);
    }
    POP_r32(R_EBP);
    RET();
}

/**
 * Exit the block with sh4r.new_pc written with the target address
 */
void exit_block_newpcset( sh4addr_t pc )
{
    load_imm32( R_ECX, ((pc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADD_r32_sh4r( R_ECX, REG_OFFSET(slice_cycle) );    // 6
    load_spreg( R_EAX, R_NEW_PC );
    store_spreg( R_EAX, R_PC );
    if( sh4_x86.tlb_on ) {
	call_func1(xlat_get_code_by_vma,R_EAX);
    } else {
	call_func1(xlat_get_code,R_EAX);
    }
    POP_r32(R_EBP);
    RET();
}

#define EXIT_BLOCK_SIZE(pc) (25 + (IS_IN_ICACHE(pc)?10:CALL_FUNC1_SIZE))
/**
 * Exit the block to an absolute PC
 */
void exit_block( sh4addr_t pc, sh4addr_t endpc )
{
    load_imm32( R_ECX, pc );                            // 5
    store_spreg( R_ECX, REG_OFFSET(pc) );               // 3
    if( IS_IN_ICACHE(pc) ) {
	REXW(); MOV_moff32_EAX( xlat_get_lut_entry(pc) );
    } else if( sh4_x86.tlb_on ) {
	call_func1(xlat_get_code_by_vma, R_ECX);
    } else {
	call_func1(xlat_get_code,R_ECX);
    }
    REXW(); AND_imm8s_r32( 0xFC, R_EAX ); // 4
    load_imm32( R_ECX, ((endpc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADD_r32_sh4r( R_ECX, REG_OFFSET(slice_cycle) );     // 6
    POP_r32(R_EBP);
    RET();
}


#define EXIT_BLOCK_REL_SIZE(pc)  (28 + (IS_IN_ICACHE(pc)?10:CALL_FUNC1_SIZE))

/**
 * Exit the block to a relative PC
 */
void exit_block_rel( sh4addr_t pc, sh4addr_t endpc )
{
    load_imm32( R_ECX, pc - sh4_x86.block_start_pc );   // 5
    ADD_sh4r_r32( R_PC, R_ECX );
    store_spreg( R_ECX, REG_OFFSET(pc) );               // 3
    if( IS_IN_ICACHE(pc) ) {
	REXW(); MOV_moff32_EAX( xlat_get_lut_entry(GET_ICACHE_PHYS(pc)) ); // 5
    } else if( sh4_x86.tlb_on ) {
	call_func1(xlat_get_code_by_vma,R_ECX);
    } else {
	call_func1(xlat_get_code,R_ECX);
    }
    REXW(); AND_imm8s_r32( 0xFC, R_EAX ); // 4
    load_imm32( R_ECX, ((endpc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADD_r32_sh4r( R_ECX, REG_OFFSET(slice_cycle) );     // 6
    POP_r32(R_EBP);
    RET();
}

/**
 * Write the block trailer (exception handling block)
 */
void sh4_translate_end_block( sh4addr_t pc ) {
    if( sh4_x86.branch_taken == FALSE ) {
	// Didn't exit unconditionally already, so write the termination here
	exit_block_rel( pc, pc );
    }
    if( sh4_x86.backpatch_posn != 0 ) {
	unsigned int i;
	// Raise exception
	uint8_t *end_ptr = xlat_output;
	MOV_r32_r32( R_EDX, R_ECX );
	ADD_r32_r32( R_EDX, R_ECX );
	ADD_r32_sh4r( R_ECX, R_PC );
	MOV_moff32_EAX( &sh4_cpu_period );
	MUL_r32( R_EDX );
	ADD_r32_sh4r( R_EAX, REG_OFFSET(slice_cycle) );

	call_func0( sh4_raise_exception );
	load_spreg( R_EAX, R_PC );
	if( sh4_x86.tlb_on ) {
	    call_func1(xlat_get_code_by_vma,R_EAX);
	} else {
	    call_func1(xlat_get_code,R_EAX);
	}
	POP_r32(R_EBP);
	RET();

	// Exception already raised - just cleanup
	uint8_t *preexc_ptr = xlat_output;
	MOV_r32_r32( R_EDX, R_ECX );
	ADD_r32_r32( R_EDX, R_ECX );
	ADD_r32_sh4r( R_ECX, R_SPC );
	MOV_moff32_EAX( &sh4_cpu_period );
	MUL_r32( R_EDX );
	ADD_r32_sh4r( R_EAX, REG_OFFSET(slice_cycle) );
	load_spreg( R_EDI, R_PC );
	if( sh4_x86.tlb_on ) {
	    call_func0(xlat_get_code_by_vma);
	} else {
	    call_func0(xlat_get_code);
	}
	POP_r32(R_EBP);
	RET();

	for( i=0; i< sh4_x86.backpatch_posn; i++ ) {
	    *sh4_x86.backpatch_list[i].fixup_addr =
		xlat_output - ((uint8_t *)sh4_x86.backpatch_list[i].fixup_addr) - 4;
	    if( sh4_x86.backpatch_list[i].exc_code < 0 ) {
		load_imm32( R_EDX, sh4_x86.backpatch_list[i].fixup_icount );
		int stack_adj = -1 - sh4_x86.backpatch_list[i].exc_code;
		if( stack_adj > 0 ) { 
		    ADD_imm8s_r32( stack_adj*4, R_ESP );
		}
		int rel = preexc_ptr - xlat_output;
		JMP_rel(rel);
	    } else {
		load_imm32( R_EDI, sh4_x86.backpatch_list[i].exc_code );
		load_imm32( R_EDX, sh4_x86.backpatch_list[i].fixup_icount );
		int rel = end_ptr - xlat_output;
		JMP_rel(rel);
	    }
	}
    }
}

_Unwind_Reason_Code xlat_check_frame( struct _Unwind_Context *context, void *arg )
{
    void *rbp = (void *)_Unwind_GetGR(context, 6);
    if( rbp == (void *)&sh4r ) { 
        void **result = (void **)arg;
        *result = (void *)_Unwind_GetIP(context);
        return _URC_NORMAL_STOP;
    }
    
    return _URC_NO_REASON;
}

void *xlat_get_native_pc()
{
    struct _Unwind_Exception exc;
    
    void *result = NULL;
    _Unwind_Backtrace( xlat_check_frame, &result );
    return result;
}

#endif
