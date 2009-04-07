/**
 * $Id: xir.h 931 2008-10-31 02:57:59Z nkeynes $
 * 
 * x86/x86-64 target support
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

#include "xlat/xir.h"
#include "xlat/xlat.h"
#include "xlat/x86/x86op.h"

static char *x86_reg_names[] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
                                "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
                                "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
                                "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" };

void x86_target_lower( xir_basic_block_t xbb, xir_op_t begin, xir_op_t end );
uint32_t x86_target_get_code_size( xir_op_t begin, xir_op_t end );
uint32_t x86_target_codegen( target_data_t td, xir_op_t begin, xir_op_t end ); 

struct xlat_target_machine x86_target_machine = { "x86", x86_reg_names,
    NULL, x86_target_lower, x86_target_get_code_size, x86_target_codegen    
};



/******************************************************************************
 *  Target lowering - Replace higher level/unsupported operations             *
 *  with equivalent x86 sequences. Note that we don't lower conditional ops   *
 *  here - that's left to final codegen as we can't represent local branches  *
 *  in XIR                                                                    *
 *****************************************************************************/

#define MEM_FUNC_OFFSET(name) offsetof( struct mem_region_fn, name )

/**
 * Construct an XLAT operation and append it to the code block. For 64-bit
 * code this may need to be a load/xlat sequence as we can't encode a 64-bit
 * displacement.
 */
static inline void xir_append_xlat( xir_basic_block_t xbb, void *address_space, 
                                    int opertype, int operval )
{
    if( sizeof(void *) == 8 ) {
        xir_append_ptr_op2( xbb, OP_MOVQ, address_space, SOURCE_REGISTER_OPERAND, REG_TMP4 );
        xir_append_op2( xbb, OP_XLAT, SOURCE_REGISTER_OPERAND, REG_TMP4, opertype, operval );
    } else {
        xir_append_ptr_op2( xbb, OP_XLAT, address_space, opertype, operval );
    }
}


/* Replace LOAD/STORE with low-level calling sequence eg:
 * mov addr, %eax
 * mov addr, %tmp3
 * slr 12, %tmp3
 * xlat $sh4_address_space, %tmp3
 * call/lut %tmp3, $operation_offset
 * mov %eax, result
 */ 
static void lower_mem_load( xir_basic_block_t xbb, xir_op_t it, void *addr_space, int offset )
{
    xir_op_t start =
        xir_append_op2( xbb, OP_MOV, it->operand[0].type, it->operand[0].value.i, TARGET_REGISTER_OPERAND, REG_ARG1 );
    xir_append_op2( xbb, OP_MOV, it->operand[0].type, it->operand[0].value.i, SOURCE_REGISTER_OPERAND, REG_TMP3 );
    xir_append_op2( xbb, OP_SLR, INT_IMM_OPERAND, 12, SOURCE_REGISTER_OPERAND, REG_TMP3 );
    xir_append_xlat( xbb, addr_space, SOURCE_REGISTER_OPERAND, REG_TMP3 );
    xir_insert_block(start,xbb->ir_ptr-1, it);
    if( XOP_WRITES_OP2(it) ) {
        xir_insert_op( xir_append_op2( xbb, OP_MOV, TARGET_REGISTER_OPERAND, REG_RESULT1, it->operand[1].type, it->operand[1].value.i ), it->next );
    }
    /* Replace original op with CALLLUT */
    it->opcode = OP_CALLLUT;
    it->operand[0].type = SOURCE_REGISTER_OPERAND;
    it->operand[0].value.i = REG_TMP3;
    it->operand[1].type = INT_IMM_OPERAND;
    it->operand[1].value.i = offset;    
}

static void lower_mem_store( xir_basic_block_t xbb, xir_op_t it, void *addr_space, int offset )
{
    xir_op_t start = 
        xir_append_op2( xbb, OP_MOV, it->operand[0].type, it->operand[0].value.i, TARGET_REGISTER_OPERAND, REG_ARG1 ); 
    xir_append_op2( xbb, OP_MOV, it->operand[1].type, it->operand[1].value.i, TARGET_REGISTER_OPERAND, REG_ARG2 ); 
    xir_append_op2( xbb, OP_MOV, it->operand[0].type, it->operand[0].value.i, SOURCE_REGISTER_OPERAND, REG_TMP3 );
    xir_append_op2( xbb, OP_SLR, INT_IMM_OPERAND, 12, SOURCE_REGISTER_OPERAND, REG_TMP3 );
    xir_append_xlat( xbb, addr_space, SOURCE_REGISTER_OPERAND, REG_TMP3 );
    xir_insert_block(start,xbb->ir_ptr-1, it);
    /* Replace original op with CALLLUT */
    it->opcode = OP_CALLLUT;
    it->operand[0].type = SOURCE_REGISTER_OPERAND;
    it->operand[0].value.i = REG_TMP3;
    it->operand[1].type = INT_IMM_OPERAND;
    it->operand[1].value.i = offset;    
}

static void lower_mem_loadq( xir_basic_block_t xbb, xir_op_t it, void *addr_space )
{
    int resulttype = it->operand[1].type;
    uint32_t resultval = it->operand[1].value.i;
    
    /* First block */
    xir_op_t start =
        xir_append_op2( xbb, OP_MOV, it->operand[0].type, it->operand[0].value.i, TARGET_REGISTER_OPERAND, REG_ARG1 );
    xir_append_op2( xbb, OP_MOV, it->operand[0].type, it->operand[0].value.i, SOURCE_REGISTER_OPERAND, REG_TMP3 );
    xir_append_op2( xbb, OP_SLR, INT_IMM_OPERAND, 12, SOURCE_REGISTER_OPERAND, REG_TMP3 );
    xir_append_xlat( xbb, addr_space, SOURCE_REGISTER_OPERAND, REG_TMP3 );
    xir_insert_block(start,xbb->ir_ptr-1, it);
    /* Replace original op with CALLLUT */
    it->opcode = OP_CALLLUT;
    it->operand[0].type = SOURCE_REGISTER_OPERAND;
    it->operand[0].value.i = REG_TMP3;
    it->operand[1].type = INT_IMM_OPERAND;
    it->operand[1].value.i = MEM_FUNC_OFFSET(read_long);    

    /* Second block */
    start = xir_append_op2( xbb, OP_MOV, TARGET_REGISTER_OPERAND, REG_RESULT1, resulttype, resultval+1 );
    xir_append_op2( xbb, OP_ADD, INT_IMM_OPERAND, 4, SOURCE_REGISTER_OPERAND, REG_ARG1 );
    xir_op_t fin = xir_append_op2( xbb, OP_CALLLUT, SOURCE_REGISTER_OPERAND, REG_TMP3, INT_IMM_OPERAND, MEM_FUNC_OFFSET(read_long) );
    xir_append_op2( xbb, OP_MOV, TARGET_REGISTER_OPERAND, REG_RESULT1, resulttype, resultval );
    fin->exc = it->exc;
    xir_insert_block(start, xbb->ir_ptr-1, it->next);
}

static void lower_mem_storeq( xir_basic_block_t xbb, xir_op_t it, void *addr_space )
{
    int argtype = it->operand[1].type;
    uint32_t argval = it->operand[1].value.i;
    
    /* First block */
    xir_op_t start =
        xir_append_op2( xbb, OP_MOV, it->operand[0].type, it->operand[0].value.i, TARGET_REGISTER_OPERAND, REG_ARG1 );
    xir_append_op2( xbb, OP_MOV, argtype, argval+1, TARGET_REGISTER_OPERAND, REG_ARG2 ); 
    xir_append_op2( xbb, OP_MOV, it->operand[0].type, it->operand[0].value.i, SOURCE_REGISTER_OPERAND, REG_TMP3 );
    xir_append_op2( xbb, OP_SLR, INT_IMM_OPERAND, 12, SOURCE_REGISTER_OPERAND, REG_TMP3 );
    xir_append_xlat( xbb, addr_space, SOURCE_REGISTER_OPERAND, REG_TMP3 );
    xir_insert_block(start,xbb->ir_ptr-1, it);
    /* Replace original op with CALLLUT */
    it->opcode = OP_CALLLUT;
    it->operand[0].type = SOURCE_REGISTER_OPERAND;
    it->operand[0].value.i = REG_TMP3;
    it->operand[1].type = INT_IMM_OPERAND;
    it->operand[1].value.i = MEM_FUNC_OFFSET(read_long);    

    /* Second block */
    xir_append_op2( xbb, OP_MOV, argtype, argval, TARGET_REGISTER_OPERAND, REG_ARG2 ); 
    xir_append_op2( xbb, OP_ADD, INT_IMM_OPERAND, 4, SOURCE_REGISTER_OPERAND, REG_ARG1 );
    xir_op_t fin = xir_append_op2( xbb, OP_CALLLUT, SOURCE_REGISTER_OPERAND, REG_TMP3, INT_IMM_OPERAND, MEM_FUNC_OFFSET(read_long) );
    fin->exc = it->exc;
    xir_insert_block(start, xbb->ir_ptr-1, it->next);    
}


/**
 * Runs a single pass over the block, performing the following transformations:
 *   Load/Store ops -> Mov/call sequences
 *   Flags -> explicit SETcc/loadcc ops where necessary (doesn't try to reorder
 *     at the moment)
 *   Mov operands into target specific registers where the ISA requires it. (eg SAR) 
 * Run in reverse order so we can track liveness of the flags as we go (for ALU 
 * lowering to flags-modifying instructions)
 */
void x86_target_lower( xir_basic_block_t xbb, xir_op_t start, xir_op_t end )
{
    gboolean flags_live = FALSE;
    xir_op_t it;
    for( it=end; it != NULL; it = it->prev ) {
        switch( it->opcode ) {
        
        /* Promote non-flag versions to flag versions where there's no flag-free version
         * (in other words, all ALU ops except ADD, since we can use LEA for a flag-free
         * ADD
         */ 
        case OP_ADDC: case OP_AND: case OP_DIV: case OP_MUL: case OP_MULQ: 
        case OP_NEG: case OP_NOT: case OP_OR: case OP_XOR: case OP_SUB: 
        case OP_SUBB: case OP_SDIV:
            it->opcode++;
            if( flags_live ) {
                xir_insert_op( XOP1( OP_SAVEFLAGS, REG_TMP5 ), it );
                xir_insert_op( XOP1( OP_RESTFLAGS, REG_TMP5 ), it->next );
            }
            break;

        case OP_SAR: case OP_SLL: case OP_SLR: case OP_ROL: case OP_ROR:
            /* Promote to *S form since we don't have a non-flag version */
            it->opcode++;
            if( flags_live ) {
                xir_insert_op( XOP1( OP_SAVEFLAGS, REG_TMP5 ), it );
                xir_insert_op( XOP1( OP_RESTFLAGS, REG_TMP5 ), it->next );
            }
            
        case OP_SARS: case OP_SLLS: case OP_SLRS:
        case OP_RCL: case OP_RCR: case OP_ROLS: case OP_RORS:
            /* Insert mov %reg, %ecx */
            if( it->operand[0].type == SOURCE_REGISTER_OPERAND ) {
                xir_insert_op( xir_append_op2( xbb, OP_MOV, SOURCE_REGISTER_OPERAND, it->operand[0].value.i, TARGET_REGISTER_OPERAND, REG_ECX ), it );
                it->operand[0].type = TARGET_REGISTER_OPERAND;
                it->operand[0].value.i = REG_ECX;
            }
            break;
        case OP_SHLD: case OP_SHAD:
            /* Insert mov %reg, %ecx */
            if( it->operand[0].type == SOURCE_REGISTER_OPERAND ) {
                xir_insert_op( xir_append_op2( xbb, OP_MOV, SOURCE_REGISTER_OPERAND, it->operand[0].value.i, TARGET_REGISTER_OPERAND, REG_ECX ), it );
                it->operand[0].type = TARGET_REGISTER_OPERAND;
                it->operand[0].value.i = REG_ECX;
            } else if( it->operand[0].type == INT_IMM_OPERAND ) {
                /* Simplify down to SAR/SLL/SLR where we have a constant shift */
                if( it->operand[0].value.i == 0 ) {
                    /* No-op */
                    it->opcode = OP_NOP;
                    it->operand[1].type = it->operand[0].type = NO_OPERAND;
                } else if( it->operand[0].value.i > 0 ) {
                    it->opcode = OP_SLL;
                } else if( (it->operand[0].value.i & 0x1F) == 0 ) {
                    if( it->opcode == OP_SHLD ) {
                        it->opcode = OP_MOV;
                        it->operand[0].value.i = 0;
                    } else {
                        it->opcode = OP_SAR;
                        it->operand[0].value.i = 31;
                    }
                } else {
                    if( it->opcode == OP_SHLD ) {
                        it->opcode = OP_SLR;
                    } else {
                        it->opcode = OP_SAR;
                    }
                }
            }
            break;

        case OP_CALL1: /* Reduce to mov reg, %eax; call0 ptr */
            xir_insert_op( xir_append_op2( xbb, OP_MOV, it->operand[1].type, it->operand[1].value.i, TARGET_REGISTER_OPERAND, REG_ARG1 ), it );
            it->opcode = OP_CALL0;
            it->operand[1].type = NO_OPERAND;
            break;
        case OP_CALLR: /* reduce to call0 ptr, mov result, reg */
            xir_insert_op( xir_append_op2( xbb, OP_MOV, TARGET_REGISTER_OPERAND, REG_RESULT1, it->operand[1].type, it->operand[1].value.i), it->next );
            it->opcode = OP_CALL0;
            it->operand[1].type = NO_OPERAND;
            break;
        case OP_LOADB:
            lower_mem_load( xbb, it, xbb->address_space, MEM_FUNC_OFFSET(read_byte) );
            break;
        case OP_LOADW: 
            lower_mem_load( xbb, it, xbb->address_space, MEM_FUNC_OFFSET(read_word) );
            break;
        case OP_LOADL: 
            lower_mem_load( xbb, it, xbb->address_space, MEM_FUNC_OFFSET(read_long) );
            break;
        case OP_LOADBFW: 
            lower_mem_load( xbb, it, xbb->address_space, MEM_FUNC_OFFSET(read_byte_for_write) );
            break;
        case OP_LOADQ:
            lower_mem_loadq( xbb, it, xbb->address_space );
            break;
        case OP_PREF:
            lower_mem_load( xbb, it, xbb->address_space, MEM_FUNC_OFFSET(prefetch) );
            break;            
        case OP_OCBI: 
        case OP_OCBP:
        case OP_OCBWB:
             break;
        case OP_STOREB:
            lower_mem_store( xbb, it, xbb->address_space, MEM_FUNC_OFFSET(write_byte) );
            break;
        case OP_STOREW:
            lower_mem_store( xbb, it, xbb->address_space, MEM_FUNC_OFFSET(write_word) );
            break;
        case OP_STOREL:
            lower_mem_store( xbb, it, xbb->address_space, MEM_FUNC_OFFSET(write_long) );
            break;
        case OP_STORELCA:
            lower_mem_store( xbb, it, xbb->address_space, MEM_FUNC_OFFSET(write_long) );
            break;
        case OP_STOREQ:
            lower_mem_storeq( xbb, it, xbb->address_space );
            break;
            
        case OP_SHUFFLE:
            assert( it->operand[0].type == INT_IMM_OPERAND );
            if( it->operand[0].value.i = 0x2134 ) { /* Swap low bytes */
                /* This is an xchg al,ah, but we need to force the operand into one of the bottom 4 registers */
                xir_insert_op( xir_append_op2( xbb, OP_MOV, it->operand[1].type, it->operand[1].value.i, TARGET_REGISTER_OPERAND, REG_EAX ), it);
                it->operand[1].type = TARGET_REGISTER_OPERAND;
                it->operand[1].value.i = REG_EAX; 
            } else if( it->operand[0].value.i != 0x4321 ) { 
                /* 4321 is a full byteswap (directly supported) - use shift/mask/or 
                 * sequence for anything else. Although we could use PSHUF...
                 */
                it = xir_shuffle_lower( xbb, it, REG_TMP3, REG_TMP4 );
            }
            break;
        case OP_NEGF:
            xir_insert_op( xir_append_op2( xbb, OP_MOV, INT_IMM_OPERAND, 0, SOURCE_REGISTER_OPERAND, REG_TMP4 ), it);
            xir_insert_op( xir_append_op2( xbb, OP_MOV, SOURCE_REGISTER_OPERAND, REG_TMP4,it->operand[0].type, it->operand[0].value.i), it->next ); 
            it->opcode = OP_SUBF;
            it->operand[1].type = SOURCE_REGISTER_OPERAND;
            it->operand[1].value.i = REG_TMP4;
            break;
        case OP_NEGD:
            xir_insert_op( xir_append_op2( xbb, OP_MOVQ, INT_IMM_OPERAND, 0, SOURCE_REGISTER_OPERAND, REG_TMPQ0 ), it);
            xir_insert_op( xir_append_op2( xbb, OP_MOVQ, SOURCE_REGISTER_OPERAND, REG_TMPQ0,it->operand[0].type, it->operand[0].value.i), it->next ); 
            it->opcode = OP_SUBD;
            it->operand[1].type = SOURCE_REGISTER_OPERAND;
            it->operand[1].value.i = REG_TMPQ0;
            break;
        case OP_XLAT:
            /* Insert temp register if translating through a 64-bit pointer */
            if( XOP_IS_PTRIMM(it, 0) && sizeof(void *) == 8 && it->operand[0].value.q >= 0x100000000LL ) {
                xir_insert_op( XOP2P( OP_MOVQ, it->operand[0].value.p, REG_TMP4 ), it );
                it->operand[0].type = SOURCE_REGISTER_OPERAND;
                it->operand[0].value.i = REG_TMP4;
            }
            break;
        }
        
        if( XOP_READS_FLAGS(it) ) {
            flags_live = TRUE;
        } else if( XOP_WRITES_FLAGS(it) ) {
            flags_live = FALSE;
        }
    
        /* Lower pointer operands to INT or QUAD according to address and value size. */
        if( it->operand[0].type == POINTER_OPERAND ) {
            if( sizeof(void *) == 8 && it->operand[0].value.q >= 0x100000000LL ) {
                if( it->opcode == OP_MOV ) {
                    // Promote MOV ptr, reg to MOVQ ptr, reg
                    it->opcode = OP_MOVQ;
                } else if( it->opcode != OP_MOVQ ) {
                    /* 64-bit pointers can't be used as immediate values - break up into 
                     * an immediate load to temporary, followed by the original instruction.
                     * (We only check the first operand as there are no instructions that
                     * permit the second operand to be an immediate pointer.
                     */
                    xir_insert_op( xir_append_op2( xbb, OP_MOVQ, POINTER_OPERAND, it->operand[0].value.q, SOURCE_REGISTER_OPERAND, REG_TMP4 ), it );
                    it->operand[0].type = SOURCE_REGISTER_OPERAND;
                    it->operand[1].value.i = REG_TMP4;
                }
                it->operand[0].type = QUAD_IMM_OPERAND;
            } else {
                if( it->opcode == OP_MOVQ ) {
                    /* Lower a MOVQ of a 32-bit quantity to a MOV, and save the 5 bytes */
                    it->opcode = OP_MOV;
                }
                it->operand[0].type = INT_IMM_OPERAND;
            }
        }
        
        if( it == start )
            break;
    }
    
}
