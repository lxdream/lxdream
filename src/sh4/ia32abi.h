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

#ifndef __lxdream_ia32abi_H
#define __lxdream_ia32abi_H 1

#define load_ptr( reg, ptr ) load_imm32( reg, (uint32_t)ptr );

/**
 * Note: clobbers EAX to make the indirect call - this isn't usually
 * a problem since the callee will usually clobber it anyway.
 */
#define CALL_FUNC0_SIZE 7
static inline void call_func0( void *ptr )
{
    load_imm32(R_EAX, (uint32_t)ptr);
    CALL_r32(R_EAX);
}

#define CALL_FUNC1_SIZE 11
static inline void call_func1( void *ptr, int arg1 )
{
    PUSH_r32(arg1);
    call_func0(ptr);
    ADD_imm8s_r32( 4, R_ESP );
}

#define CALL_FUNC2_SIZE 12
static inline void call_func2( void *ptr, int arg1, int arg2 )
{
    PUSH_r32(arg2);
    PUSH_r32(arg1);
    call_func0(ptr);
    ADD_imm8s_r32( 8, R_ESP );
}

/**
 * Write a double (64-bit) value into memory, with the first word in arg2a, and
 * the second in arg2b
 * NB: 30 bytes
 */
#define MEM_WRITE_DOUBLE_SIZE 30
static inline void MEM_WRITE_DOUBLE( int addr, int arg2a, int arg2b )
{
    ADD_imm8s_r32( 4, addr );
    PUSH_r32(arg2b);
    PUSH_r32(addr);
    ADD_imm8s_r32( -4, addr );
    PUSH_r32(arg2a);
    PUSH_r32(addr);
    call_func0(sh4_write_long);
    ADD_imm8s_r32( 8, R_ESP );
    call_func0(sh4_write_long);
    ADD_imm8s_r32( 8, R_ESP );
}

/**
 * Read a double (64-bit) value from memory, writing the first word into arg2a
 * and the second into arg2b. The addr must not be in EAX
 * NB: 27 bytes
 */
#define MEM_READ_DOUBLE_SIZE 27
static inline void MEM_READ_DOUBLE( int addr, int arg2a, int arg2b )
{
    PUSH_r32(addr);
    call_func0(sh4_read_long);
    POP_r32(addr);
    PUSH_r32(R_EAX);
    ADD_imm8s_r32( 4, addr );
    PUSH_r32(addr);
    call_func0(sh4_read_long);
    ADD_imm8s_r32( 4, R_ESP );
    MOV_r32_r32( R_EAX, arg2b );
    POP_r32(arg2a);
}

#define EXIT_BLOCK_SIZE 29


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
    sh4_x86.block_start_pc = pc;
    sh4_x86.tlb_on = MMIO_READ(MMU,MMUCR)&MMUCR_AT;
    sh4_x86.tstate = TSTATE_NONE;
#ifdef STACK_ALIGN
	sh4_x86.stack_posn = 8;
#endif
}

/**
 * Exit the block with sh4r.pc already written
 * Bytes: 15
 */
void exit_block_pcset( sh4addr_t pc )
{
    load_imm32( R_ECX, ((pc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADD_r32_sh4r( R_ECX, REG_OFFSET(slice_cycle) );    // 6
    load_spreg( R_EAX, REG_OFFSET(pc) );
    if( sh4_x86.tlb_on ) {
	call_func1(xlat_get_code_by_vma,R_EAX);
    } else {
	call_func1(xlat_get_code,R_EAX);
    } 
    POP_r32(R_EBP);
    RET();
}

/**
 * Exit the block to an absolute PC
 */
void exit_block( sh4addr_t pc, sh4addr_t endpc )
{
    load_imm32( R_ECX, pc );                            // 5
    store_spreg( R_ECX, REG_OFFSET(pc) );               // 3
    MOV_moff32_EAX( xlat_get_lut_entry(pc) ); // 5
    AND_imm8s_r32( 0xFC, R_EAX ); // 3
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
	exit_block( pc, pc );
    }
    if( sh4_x86.backpatch_posn != 0 ) {
	unsigned int i;
	// Raise exception
	uint8_t *end_ptr = xlat_output;
	load_spreg( R_ECX, REG_OFFSET(pc) );
	ADD_r32_r32( R_EDX, R_ECX );
	ADD_r32_r32( R_EDX, R_ECX );
	store_spreg( R_ECX, REG_OFFSET(pc) );
	MOV_moff32_EAX( &sh4_cpu_period );
	MUL_r32( R_EDX );
	ADD_r32_sh4r( R_EAX, REG_OFFSET(slice_cycle) );

	call_func0( sh4_raise_exception );
	ADD_imm8s_r32( 4, R_ESP );
	load_spreg( R_EAX, REG_OFFSET(pc) );
	if( sh4_x86.tlb_on ) {
	    call_func1(xlat_get_code_by_vma,R_EAX);
	} else {
	    call_func1(xlat_get_code,R_EAX);
	}
	POP_r32(R_EBP);
	RET();

	// Exception already raised - just cleanup
	uint8_t *preexc_ptr = xlat_output;
	load_imm32( R_ECX, sh4_x86.block_start_pc );
	ADD_r32_r32( R_EDX, R_ECX );
	ADD_r32_r32( R_EDX, R_ECX );
	store_spreg( R_ECX, REG_OFFSET(spc) );
	MOV_moff32_EAX( &sh4_cpu_period );
	MUL_r32( R_EDX );
	ADD_r32_sh4r( R_EAX, REG_OFFSET(slice_cycle) );
	load_spreg( R_EAX, REG_OFFSET(pc) );
	if( sh4_x86.tlb_on ) {
	    call_func1(xlat_get_code_by_vma,R_EAX);
	} else {
	    call_func1(xlat_get_code,R_EAX);
	}
	POP_r32(R_EBP);
	RET();

	for( i=0; i< sh4_x86.backpatch_posn; i++ ) {
	    *sh4_x86.backpatch_list[i].fixup_addr =
		xlat_output - ((uint8_t *)sh4_x86.backpatch_list[i].fixup_addr) - 4;
	    if( sh4_x86.backpatch_list[i].exc_code == -1 ) {
		load_imm32( R_EDX, sh4_x86.backpatch_list[i].fixup_icount );
		int rel = preexc_ptr - xlat_output;
		JMP_rel(rel);
	    } else {
		PUSH_imm32( sh4_x86.backpatch_list[i].exc_code );
		load_imm32( R_EDX, sh4_x86.backpatch_list[i].fixup_icount );
		int rel = end_ptr - xlat_output;
		JMP_rel(rel);
	    }
	}
    }
}

#endif


