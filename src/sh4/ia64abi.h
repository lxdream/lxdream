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

#ifndef lxdream_ia64abi_H
#define lxdream_ia64abi_H 1

#include <unwind.h>

#define load_ptr( reg, ptr ) load_imm64( reg, (uint64_t)ptr );

static inline void decode_address( int addr_reg )
{
    uintptr_t base = (sh4r.xlat_sh4_mode&SR_MD) ? (uintptr_t)sh4_address_space : (uintptr_t)sh4_user_address_space;
    MOVL_r32_r32( addr_reg, REG_RCX ); 
    SHRL_imm_r32( 12, REG_RCX ); 
    MOVP_immptr_rptr( base, REG_RDI );
    MOVP_sib_rptr(3, REG_RCX, REG_RDI, 0, REG_RCX);
}

/**
 * Note: clobbers EAX to make the indirect call - this isn't usually
 * a problem since the callee will usually clobber it anyway.
 * Size: 12 bytes
 */
#define CALL_FUNC0_SIZE 12
static inline void call_func0( void *ptr )
{
    MOVQ_imm64_r64((uint64_t)ptr, REG_RAX);
    CALL_r32(REG_RAX);
}

static inline void call_func1( void *ptr, int arg1 )
{
    MOVQ_r64_r64(arg1, REG_RDI);
    call_func0(ptr);
}

static inline void call_func1_exc( void *ptr, int arg1, int pc )
{
    MOVQ_r64_r64(arg1, REG_RDI);
    MOVP_immptr_rptr(0, REG_RSI);
    sh4_x86_add_backpatch( xlat_output, pc, -2 );
    call_func0(ptr);
}

static inline void call_func1_r32disp8( int preg, uint32_t disp8, int arg1 )
{
    MOVQ_r64_r64(arg1, REG_RDI);
    CALL_r32disp(preg, disp8);    
}

static inline void call_func1_r32disp8_exc( int preg, uint32_t disp8, int arg1, int pc )
{
    MOVQ_r64_r64(arg1, REG_RDI);
    MOVP_immptr_rptr(0, REG_RSI);
    sh4_x86_add_backpatch( xlat_output, pc, -2 );
    CALL_r32disp(preg, disp8);
}

static inline void call_func2( void *ptr, int arg1, int arg2 )
{
    MOVQ_r64_r64(arg1, REG_RDI);
    MOVQ_r64_r64(arg2, REG_RSI);
    call_func0(ptr);
}

static inline void call_func2_r32disp8( int preg, uint32_t disp8, int arg1, int arg2 )
{
    MOVQ_r64_r64(arg1, REG_RDI);
    MOVQ_r64_r64(arg2, REG_RSI);
    CALL_r32disp(preg, disp8);    
}

static inline void call_func2_r32disp8_exc( int preg, uint32_t disp8, int arg1, int arg2, int pc )
{
    MOVQ_r64_r64(arg1, REG_RDI);
    MOVQ_r64_r64(arg2, REG_RSI);
    MOVP_immptr_rptr(0, REG_RDX);
    sh4_x86_add_backpatch( xlat_output, pc, -2 );
    CALL_r32disp(preg, disp8);
}



/**
 * Emit the 'start of block' assembly. Sets up the stack frame and save
 * SI/DI as required
 */
void enter_block( ) 
{
    PUSH_r32(REG_RBP);
    load_ptr( REG_RBP, ((uint8_t *)&sh4r) + 128 );
    // Minimum aligned allocation is 16 bytes
    SUBQ_imms_r64( 16, REG_RSP );
}

static inline void exit_block( )
{
    ADDQ_imms_r64( 16, REG_RSP );
    POP_r32(REG_RBP);
    RET();
}

/**
 * Exit the block with sh4r.pc already written
 */
void exit_block_pcset( sh4addr_t pc )
{
    load_imm32( REG_ECX, ((pc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADDL_r32_rbpdisp( REG_ECX, REG_OFFSET(slice_cycle) );    // 6
    load_spreg( REG_RAX, R_PC );
    if( sh4_x86.tlb_on ) {
        call_func1(xlat_get_code_by_vma,REG_RAX);
    } else {
        call_func1(xlat_get_code,REG_RAX);
    }
    exit_block();
}

/**
 * Exit the block with sh4r.new_pc written with the target address
 */
void exit_block_newpcset( sh4addr_t pc )
{
    load_imm32( REG_ECX, ((pc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADDL_r32_rbpdisp( REG_ECX, REG_OFFSET(slice_cycle) );    // 6
    load_spreg( REG_RAX, R_NEW_PC );
    store_spreg( REG_RAX, R_PC );
    if( sh4_x86.tlb_on ) {
        call_func1(xlat_get_code_by_vma,REG_RAX);
    } else {
        call_func1(xlat_get_code,REG_RAX);
    }
    exit_block();
}

#define EXIT_BLOCK_SIZE(pc) (25 + (IS_IN_ICACHE(pc)?10:CALL_FUNC1_SIZE))
/**
 * Exit the block to an absolute PC
 */
void exit_block_abs( sh4addr_t pc, sh4addr_t endpc )
{
    load_imm32( REG_RCX, pc );                            // 5
    store_spreg( REG_RCX, REG_OFFSET(pc) );               // 3
    if( IS_IN_ICACHE(pc) ) {
        MOVP_moffptr_rax( xlat_get_lut_entry(pc) );
        ANDQ_imms_r64( 0xFFFFFFFC, REG_RAX ); // 4
    } else if( sh4_x86.tlb_on ) {
        call_func1(xlat_get_code_by_vma, REG_RCX);
    } else {
        call_func1(xlat_get_code,REG_RCX);
    }
    load_imm32( REG_ECX, ((endpc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADDL_r32_rbpdisp( REG_ECX, REG_OFFSET(slice_cycle) );     // 6
    exit_block();
}


#define EXIT_BLOCK_REL_SIZE(pc)  (28 + (IS_IN_ICACHE(pc)?10:CALL_FUNC1_SIZE))

/**
 * Exit the block to a relative PC
 */
void exit_block_rel( sh4addr_t pc, sh4addr_t endpc )
{
    load_imm32( REG_ECX, pc - sh4_x86.block_start_pc );   // 5
    ADDL_rbpdisp_r32( R_PC, REG_ECX );
    store_spreg( REG_ECX, REG_OFFSET(pc) );               // 3
    if( IS_IN_ICACHE(pc) ) {
        MOVP_moffptr_rax( xlat_get_lut_entry(GET_ICACHE_PHYS(pc)) ); // 5
        ANDQ_imms_r64( 0xFFFFFFFC, REG_RAX ); // 4
    } else if( sh4_x86.tlb_on ) {
        call_func1(xlat_get_code_by_vma,REG_RCX);
    } else {
        call_func1(xlat_get_code,REG_RCX);
    }
    load_imm32( REG_ECX, ((endpc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADDL_r32_rbpdisp( REG_ECX, REG_OFFSET(slice_cycle) );     // 6
    exit_block();
}

/**
 * Exit unconditionally with a general exception
 */
void exit_block_exc( int code, sh4addr_t pc )
{
    load_imm32( REG_ECX, pc - sh4_x86.block_start_pc );   // 5
    ADDL_r32_rbpdisp( REG_ECX, R_PC );
    load_imm32( REG_ECX, ((pc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADDL_r32_rbpdisp( REG_ECX, REG_OFFSET(slice_cycle) );     // 6
    load_imm32( REG_RAX, code );
    call_func1( sh4_raise_exception, REG_RAX );
    
    load_spreg( REG_RAX, R_PC );
    if( sh4_x86.tlb_on ) {
        call_func1(xlat_get_code_by_vma,REG_RAX);
    } else {
        call_func1(xlat_get_code,REG_RAX);
    }

    exit_block();
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
        MOVL_r32_r32( REG_RDX, REG_RCX );
        ADDL_r32_r32( REG_RDX, REG_RCX );
        ADDL_r32_rbpdisp( REG_RCX, R_PC );
        MOVL_moffptr_eax( &sh4_cpu_period );
        MULL_r32( REG_RDX );
        ADDL_r32_rbpdisp( REG_RAX, REG_OFFSET(slice_cycle) );

        call_func0( sh4_raise_exception );
        load_spreg( REG_RAX, R_PC );
        if( sh4_x86.tlb_on ) {
            call_func1(xlat_get_code_by_vma,REG_RAX);
        } else {
            call_func1(xlat_get_code,REG_RAX);
        }
        exit_block();
        
        // Exception already raised - just cleanup
        uint8_t *preexc_ptr = xlat_output;
        MOVL_r32_r32( REG_EDX, REG_ECX );
        ADDL_r32_r32( REG_EDX, REG_ECX );
        ADDL_r32_rbpdisp( REG_ECX, R_SPC );
        MOVL_moffptr_eax( &sh4_cpu_period );
        MULL_r32( REG_EDX );
        ADDL_r32_rbpdisp( REG_EAX, REG_OFFSET(slice_cycle) );
        load_spreg( REG_RDI, R_PC );
        if( sh4_x86.tlb_on ) {
            call_func0(xlat_get_code_by_vma);
        } else {
            call_func0(xlat_get_code);
        }
        exit_block();

        for( i=0; i< sh4_x86.backpatch_posn; i++ ) {
            uint32_t *fixup_addr = (uint32_t *)&xlat_current_block->code[sh4_x86.backpatch_list[i].fixup_offset];
            if( sh4_x86.backpatch_list[i].exc_code < 0 ) {
                if( sh4_x86.backpatch_list[i].exc_code == -2 ) {
                    *((uintptr_t *)fixup_addr) = (uintptr_t)xlat_output; 
                } else {
                    *fixup_addr = xlat_output - (uint8_t *)&xlat_current_block->code[sh4_x86.backpatch_list[i].fixup_offset] - 4;
                }
                load_imm32( REG_RDX, sh4_x86.backpatch_list[i].fixup_icount );
                int rel = preexc_ptr - xlat_output;
                JMP_prerel(rel);
            } else {
                *fixup_addr = xlat_output - (uint8_t *)&xlat_current_block->code[sh4_x86.backpatch_list[i].fixup_offset] - 4;
                load_imm32( REG_RDI, sh4_x86.backpatch_list[i].exc_code );
                load_imm32( REG_RDX, sh4_x86.backpatch_list[i].fixup_icount );
                int rel = end_ptr - xlat_output;
                JMP_prerel(rel);
            }
        }
    }
}

struct UnwindInfo {
    uintptr_t block_start;
    uintptr_t block_end;
    void *pc;
};

_Unwind_Reason_Code xlat_check_frame( struct _Unwind_Context *context, void *arg )
{
    struct UnwindInfo *info = arg;
    void *pc = (void *)_Unwind_GetIP(context);
    if( ((uintptr_t)pc) >= info->block_start && ((uintptr_t)pc) < info->block_end ) {
        info->pc = pc;
        return _URC_NORMAL_STOP;
    }
   
    return _URC_NO_REASON;
}

void *xlat_get_native_pc( void *code, uint32_t code_size )
{
    struct _Unwind_Exception exc;
    struct UnwindInfo info;

    info.pc = NULL;
    info.block_start = (uintptr_t)code;
    info.block_end = info.block_start + code_size;
    void *result = NULL;
    _Unwind_Backtrace( xlat_check_frame, &info );
    return info.pc;
}

#endif /* !lxdream_ia64abi_H */
