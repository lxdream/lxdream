/**
 * $Id$
 * 
 * Provides the implementation for the ia32 ABI variant 
 * (eg prologue, epilogue, and calling conventions). Stack frame is
 * aligned on 16-byte boundaries for the benefit of OS X (which 
 * requires it).
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

#ifndef lxdream_ia32mac_H
#define lxdream_ia32mac_H 1

#define load_ptr( reg, ptr ) load_imm32( reg, (uint32_t)ptr );

static inline decode_address( int addr_reg )
{
    uintptr_t base = (sh4r.xlat_sh4_mode&SR_MD) ? (uintptr_t)sh4_address_space : (uintptr_t)sh4_user_address_space;
    MOV_r32_r32( addr_reg, R_ECX ); 
    SHR_imm8_r32( 12, R_ECX ); 
    MOV_r32disp32x4_r32( R_ECX, base, R_ECX );
}

/**
 * Note: clobbers EAX to make the indirect call - this isn't usually
 * a problem since the callee will usually clobber it anyway.
 */
static inline void call_func0( void *ptr )
{
    load_imm32(R_ECX, (uint32_t)ptr);
    CALL_r32(R_ECX);
}

#ifdef HAVE_FASTCALL
static inline void call_func1( void *ptr, int arg1 )
{
    if( arg1 != R_EAX ) {
        MOV_r32_r32( arg1, R_EAX );
    }
    load_imm32(R_ECX, (uint32_t)ptr);
    CALL_r32(R_ECX);
}

static inline void call_func1_r32( int addr_reg, int arg1 )
{
    if( arg1 != R_EAX ) {
        MOV_r32_r32( arg1, R_EAX );
    }
    CALL_r32(addr_reg);
}

static inline void call_func1_r32disp8( int preg, uint32_t disp8, int arg1 )
{
    if( arg1 != R_EAX ) {
        MOV_r32_r32( arg1, R_EAX );
    }
    CALL_r32disp8(preg, disp8);
}

static inline void call_func1_r32disp8_exc( int preg, uint32_t disp8, int arg1, int pc )
{
    if( arg1 != R_EAX ) {
        MOV_r32_r32( arg1, R_EAX );
    }
    load_exc_backpatch(R_EDX);
    CALL_r32disp8(preg, disp8);
}

static inline void call_func2( void *ptr, int arg1, int arg2 )
{
    if( arg2 != R_EDX ) {
        MOV_r32_r32( arg2, R_EDX );
    }
    if( arg1 != R_EAX ) {
        MOV_r32_r32( arg1, R_EAX );
    }
    load_imm32(R_ECX, (uint32_t)ptr);
    CALL_r32(R_ECX);
}

static inline void call_func2_r32( int addr_reg, int arg1, int arg2 )
{
    if( arg2 != R_EDX ) {
        MOV_r32_r32( arg2, R_EDX );
    }
    if( arg1 != R_EAX ) {
        MOV_r32_r32( arg1, R_EAX );
    }
    CALL_r32(addr_reg);
}

static inline void call_func2_r32disp8( int preg, uint32_t disp8, int arg1, int arg2 )
{
    if( arg2 != R_EDX ) {
        MOV_r32_r32( arg2, R_EDX );
    }
    if( arg1 != R_EAX ) {
        MOV_r32_r32( arg1, R_EAX );
    }
    CALL_r32disp8(preg, disp8);
}

static inline void call_func2_r32disp8_exc( int preg, uint32_t disp8, int arg1, int arg2, int pc )
{
    if( arg2 != R_EDX ) {
        MOV_r32_r32( arg2, R_EDX );
    }
    if( arg1 != R_EAX ) {
        MOV_r32_r32( arg1, R_EAX );
    }
    MOV_backpatch_esp8( 0 );
    CALL_r32disp8(preg, disp8);
}



static inline void call_func1_exc( void *ptr, int arg1, int pc )
{
    if( arg1 != R_EAX ) {
        MOV_r32_r32( arg1, R_EAX );
    }
    load_exc_backpatch(R_EDX);
    load_imm32(R_ECX, (uint32_t)ptr);
    CALL_r32(R_ECX);
}   

static inline void call_func2_exc( void *ptr, int arg1, int arg2, int pc )
{
    if( arg2 != R_EDX ) {
        MOV_r32_r32( arg2, R_EDX );
    }
    if( arg1 != R_EAX ) {
        MOV_r32_r32( arg1, R_EAX );
    }
    MOV_backpatch_esp8(0);
    load_imm32(R_ECX, (uint32_t)ptr);
    CALL_r32(R_ECX);
}

#else
static inline void call_func1( void *ptr, int arg1 )
{
    SUB_imm8s_r32( 12, R_ESP );
    PUSH_r32(arg1);
    load_imm32(R_ECX, (uint32_t)ptr);
    CALL_r32(R_ECX);
    ADD_imm8s_r32( 16, R_ESP );
}

static inline void call_func2( void *ptr, int arg1, int arg2 )
{
    SUB_imm8s_r32( 8, R_ESP );
    PUSH_r32(arg2);
    PUSH_r32(arg1);
    load_imm32(R_ECX, (uint32_t)ptr);
    CALL_r32(R_ECX);
    ADD_imm8s_r32( 16, R_ESP );
}

#endif

/**
 * Emit the 'start of block' assembly. Sets up the stack frame and save
 * SI/DI as required
 * Allocates 8 bytes for local variables, which also has the convenient
 * side-effect of aligning the stack.
 */
void enter_block( ) 
{
    PUSH_r32(R_EBP);
    load_ptr( R_EBP, ((uint8_t *)&sh4r) + 128 );
    SUB_imm8s_r32( 8, R_ESP ); 
}

static inline void exit_block( )
{
    ADD_imm8s_r32( 8, R_ESP );
    POP_r32(R_EBP);
    RET();
}

/**
 * Exit the block with sh4r.new_pc written with the target pc
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
    exit_block();
}

/**
 * Exit the block with sh4r.new_pc written with the target pc
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
    exit_block();
}


/**
 * Exit the block to an absolute PC
 */
void exit_block_abs( sh4addr_t pc, sh4addr_t endpc )
{
    load_imm32( R_ECX, pc );                            // 5
    store_spreg( R_ECX, REG_OFFSET(pc) );               // 3
    if( IS_IN_ICACHE(pc) ) {
        MOV_moff32_EAX( xlat_get_lut_entry(GET_ICACHE_PHYS(pc)) ); // 5
        AND_imm8s_r32( 0xFC, R_EAX ); // 3
    } else if( sh4_x86.tlb_on ) {
        call_func1(xlat_get_code_by_vma,R_ECX);
    } else {
        call_func1(xlat_get_code,R_ECX);
    }
    load_imm32( R_ECX, ((endpc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADD_r32_sh4r( R_ECX, REG_OFFSET(slice_cycle) );     // 6
    exit_block();
}

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
        AND_imm8s_r32( 0xFC, R_EAX ); // 3
    } else if( sh4_x86.tlb_on ) {
        call_func1(xlat_get_code_by_vma,R_ECX);
    } else {
        call_func1(xlat_get_code,R_ECX);
    }
    load_imm32( R_ECX, ((endpc - sh4_x86.block_start_pc)>>1)*sh4_cpu_period ); // 5
    ADD_r32_sh4r( R_ECX, REG_OFFSET(slice_cycle) );     // 6
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
        MOV_r32_r32( R_EDX, R_ECX );
        ADD_r32_r32( R_EDX, R_ECX );
        ADD_r32_sh4r( R_ECX, R_PC );
        MOV_moff32_EAX( &sh4_cpu_period );
        MUL_r32( R_EDX );
        ADD_r32_sh4r( R_EAX, REG_OFFSET(slice_cycle) );

        POP_r32(R_EAX);
        call_func1( sh4_raise_exception, R_EAX );
        load_spreg( R_EAX, R_PC );
        if( sh4_x86.tlb_on ) {
            call_func1(xlat_get_code_by_vma,R_EAX);
        } else {
            call_func1(xlat_get_code,R_EAX);
        }
        exit_block();

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
        exit_block();

        for( i=0; i< sh4_x86.backpatch_posn; i++ ) {
            uint32_t *fixup_addr = (uint32_t *)&xlat_current_block->code[sh4_x86.backpatch_list[i].fixup_offset];
            if( sh4_x86.backpatch_list[i].exc_code < 0 ) {
                if( sh4_x86.backpatch_list[i].exc_code == -2 ) {
                    *fixup_addr = (uint32_t)xlat_output;
                } else {
                    *fixup_addr += xlat_output - (uint8_t *)&xlat_current_block->code[sh4_x86.backpatch_list[i].fixup_offset] - 4;
                }
                load_imm32( R_EDX, sh4_x86.backpatch_list[i].fixup_icount );
                int rel = preexc_ptr - xlat_output;
                JMP_rel(rel);
            } else {
                *fixup_addr += xlat_output - (uint8_t *)&xlat_current_block->code[sh4_x86.backpatch_list[i].fixup_offset] - 4;
                PUSH_imm32( sh4_x86.backpatch_list[i].exc_code );
                load_imm32( R_EDX, sh4_x86.backpatch_list[i].fixup_icount );
                int rel = end_ptr - xlat_output;
                JMP_rel(rel);
            }
        }
    }
}


/**
 * The unwind methods only work if we compiled with DWARF2 frame information
 * (ie -fexceptions), otherwise we have to use the direct frame scan.
 */
#ifdef HAVE_EXCEPTIONS
#include <unwind.h>

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
#else 
void *xlat_get_native_pc( void *code, uint32_t code_size )
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
        : "r" (((uint8_t *)&sh4r) + 128 )
        : "eax", "ecx", "edx" );
    return result;
}
#endif

#endif /* !lxdream_ia32mac.h */


