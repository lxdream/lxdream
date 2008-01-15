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
#define CALL_FUNC0_SIZE 13
static inline void call_func0( void *ptr )
{
    int adj = (-sh4_x86.stack_posn)&0x0F;
    SUB_imm8s_r32( adj, R_ESP );
    load_imm32(R_EAX, (uint32_t)ptr);
    CALL_r32(R_EAX);
    ADD_imm8s_r32( adj, R_ESP );
}

#define CALL_FUNC1_SIZE 14
static inline void call_func1( void *ptr, int arg1 )
{
    int adj = (-4-sh4_x86.stack_posn)&0x0F;
    SUB_imm8s_r32( adj, R_ESP );
    PUSH_r32(arg1);
    load_imm32(R_EAX, (uint32_t)ptr);
    CALL_r32(R_EAX);
    ADD_imm8s_r32( adj+4, R_ESP );
    sh4_x86.stack_posn -= 4;
}

#define CALL_FUNC2_SIZE 15
static inline void call_func2( void *ptr, int arg1, int arg2 )
{
    int adj = (-8-sh4_x86.stack_posn)&0x0F;
    SUB_imm8s_r32( adj, R_ESP );
    PUSH_r32(arg2);
    PUSH_r32(arg1);
    load_imm32(R_EAX, (uint32_t)ptr);
    CALL_r32(R_EAX);
    ADD_imm8s_r32( adj+8, R_ESP );
    sh4_x86.stack_posn -= 8;
}

/**
 * Write a double (64-bit) value into memory, with the first word in arg2a, and
 * the second in arg2b
 * NB: 30 bytes
 */
#define MEM_WRITE_DOUBLE_SIZE 36
static inline void MEM_WRITE_DOUBLE( int addr, int arg2a, int arg2b )
{
    int adj = (-8-sh4_x86.stack_posn)&0x0F;
    SUB_imm8s_r32( adj, R_ESP );
    ADD_imm8s_r32( 4, addr );
    PUSH_r32(arg2b);
    PUSH_r32(addr);
    ADD_imm8s_r32( -4, addr );
    SUB_imm8s_r32( 8, R_ESP );
    PUSH_r32(arg2a);
    PUSH_r32(addr);
    load_imm32(R_EAX, (uint32_t)sh4_write_long);
    CALL_r32(R_EAX);
    ADD_imm8s_r32( 16, R_ESP );
    load_imm32(R_EAX, (uint32_t)sh4_write_long);
    CALL_r32(R_EAX);
    ADD_imm8s_r32( adj+8, R_ESP );
    sh4_x86.stack_posn -= 16;
}

/**
 * Read a double (64-bit) value from memory, writing the first word into arg2a
 * and the second into arg2b. The addr must not be in EAX
 * NB: 27 bytes
 */
#define MEM_READ_DOUBLE_SIZE 36
static inline void MEM_READ_DOUBLE( int addr, int arg2a, int arg2b )
{
    int adj = (-4-sh4_x86.stack_posn)&0x0F;
    int adj2 = (-8-sh4_x86.stack_posn)&0x0F;
    SUB_imm8s_r32( adj, R_ESP );
    PUSH_r32(addr);
    load_imm32(R_EAX, (uint32_t)sh4_read_long);
    CALL_r32(R_EAX);
    POP_r32(R_ECX);
    SUB_imm8s_r32( adj2-adj, R_ESP );
    PUSH_r32(R_EAX);
    ADD_imm8s_r32( 4, R_ECX );
    PUSH_r32(R_ECX);
    load_imm32(R_EAX, (uint32_t)sh4_read_long);
    CALL_r32(R_EAX);
    ADD_imm8s_r32( 4, R_ESP );
    MOV_r32_r32( R_EAX, arg2b );
    POP_r32(arg2a);
    ADD_imm8s_r32( adj2, R_ESP );
    sh4_x86.stack_posn -= 4;
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
    sh4_x86.tstate = TSTATE_NONE;
    sh4_x86.tlb_on = IS_MMU_ENABLED();
    sh4_x86.stack_posn = 8;
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

#define EXIT_BLOCK_SIZE(pc)  (24 + (IS_IN_ICACHE(pc)?5:CALL_FUNC1_SIZE))


/**
 * Exit the block to an absolute PC
 */
void exit_block( sh4addr_t pc, sh4addr_t endpc )
{
    load_imm32( R_ECX, pc );                            // 5
    store_spreg( R_ECX, REG_OFFSET(pc) );               // 3
    if( IS_IN_ICACHE(pc) ) {
	MOV_moff32_EAX( xlat_get_lut_entry(GET_ICACHE_PHYS(pc)) ); // 5
    } else if( sh4_x86.tlb_on ) {
	call_func1(xlat_get_code_by_vma,R_ECX);
    } else {
	call_func1(xlat_get_code,R_ECX);
    }
    AND_imm8s_r32( 0xFC, R_EAX ); // 3
    load_imm32( R_ECX, ((endpc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADD_r32_sh4r( R_ECX, REG_OFFSET(slice_cycle) );     // 6
    POP_r32(R_EBP);
    RET();
}

#define EXIT_BLOCK_REL_SIZE(pc)  (27 + (IS_IN_ICACHE(pc)?5:CALL_FUNC1_SIZE))

/**
 * Exit the block to a relative PC
 */
void exit_block_rel( sh4addr_t pc, sh4addr_t endpc )
{
    load_imm32( R_ECX, pc - sh4_x86.block_start_pc );   // 5
    ADD_sh4r_r32( R_PC, R_ECX );
    store_spreg( R_ECX, REG_OFFSET(pc) );               // 3
    if( IS_IN_ICACHE(pc) ) {
	MOV_moff32_EAX( xlat_get_lut_entry(GET_ICACHE_PHYS(pc)) ); // 5
    } else if( sh4_x86.tlb_on ) {
	call_func1(xlat_get_code_by_vma,R_ECX);
    } else {
	call_func1(xlat_get_code,R_ECX);
    }
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

        POP_r32(R_EDX);
        call_func1( sh4_raise_exception, R_EDX );
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
	load_spreg( R_EAX, R_PC );
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

void *xlat_get_native_pc()
{
    void *result = NULL;
    asm(
	"mov %%ebp, %%eax\n\t"
	"mov $0x8, %%ecx\n\t"
	"mov %1, %%edx\n"
"frame_loop: test %%eax, %%eax\n\t"
	"je frame_not_found\n\t"
	"cmp (%%eax), %%edx\n\t"
	"je frame_found\n\t"
	"sub $0x1, %%ecx\n\t"
	"je frame_not_found\n\t"
	"movl (%%eax), %%eax\n\t"
	"jmp frame_loop\n"
"frame_found: movl 0x4(%%eax), %0\n"
"frame_not_found:"
	: "=r" (result)
	: "r" (&sh4r)
	: "eax", "ecx", "edx" );
    return result;
}


#endif


