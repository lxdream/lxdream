/**
 * $Id: xir.h 931 2008-10-31 02:57:59Z nkeynes $
 * 
 * x86/x86-64 final code generation
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

typedef enum {
    SSE_NONE = 0,
    SSE_1,
    SSE_2,
    SSE_3,
    SSE_3_1, /* AKA SSSE3 */ 
    SSE_4_1,
    SSE_4_2
} sse_version_t;

/* 32-bit register groups:
 *   General regs 0..7
 *     - EAX, EDX - arguments, volatile
 *     - ECX - volatile
 *     - EBX, ESI, EDI - non-volatile
 *     - ESP, EBP - blocked out for system use.
 *   XMM regs 16..23
 *     - Floating or integer, all volatile
 *   MMX regs 32..39
 *     - integer, all volatile
 * OR (if SSE is unsupported)
 *   x87 regs 32..39
 *     - floating point, all volatile, stack allocator
 */

/*
 * 64-bit register groups:
 *   General regs 0..15
 *     - EDI, ESI - arguments, volatile
 *     - EAX, ECX, EDX, ... - volatile
 *     - EBX, ... non-volatile
 *     - ESP, EBP - blocked for system use (r13?)
 *   XMM regs 16..31
 *     - Floating or integer, all volatile
 *   MMX regs 32..39
 *     - integer, all volatile
 * OR
 *   x87 regs 32..39
 *     - floating point, all volatile, stack allocator
 */




struct x86_target_info_struct {
    sse_version_t sse_version;
} x86_target_info;


/**
 * Initialize x86_target_info - detect supported features from cpuid
 */
void x86_target_init()
{
    uint32_t feature1, feature2;
    
    __asm__ __volatile__(
        "mov $0x01, %%eax\n\t"
        "cpuid\n\t" : "=c" (feature1), "=d" (feature2) : : "eax", "ebx");
    
    /* Walk through from oldest to newest - while it's normally the case 
     * that all older extensions are supported, you're not supposed to
     * depend on that assumption. So instead we stop as soon as we find
     * a missing feature bit. */
    if( (feature2 & 0x02000000) == 0 ) {
        x86_target_info.sse_version = SSE_NONE;
    } else if( (feature2 & 0x04000000) == 0 ) {
        x86_target_info.sse_version = SSE_1;
    } else if( (feature1 & 0x00000001) == 0 ) { /* SSE3 bit */
        x86_target_info.sse_version = SSE_2;
    } else if( (feature1 & 0x00000100) == 0 ) { /* SSSE3 bit */
        x86_target_info.sse_version = SSE_3;
    } else if( (feature1 & 0x00080000) == 0 ) { /* SSE4.1 bit */
        x86_target_info.sse_version = SSE_3_1;
    } else if( (feature1 & 0x00100000) == 0 ) { /* SSE4.2 bit */
        x86_target_info.sse_version = SSE_4_1;
    } else {
        x86_target_info.sse_version = SSE_4_2;
    }
}

#define IS_X86_64() (sizeof(void *)==8)
#define IS_XMM_REG(op,n) (XOP_REG(op,n) >= MIN_XMM_REGISTER && XOP_REG(op,n) <= MAX_AMD64_XMM_REGISTER)

#define NONE NO_OPERAND
#define SRC SOURCE_OPERAND
#define DST DEST_OPERAND
#define TMP TEMP_OPERAND
#define IMM IMMEDIATE_OPERAND

#define MAX_X86_GENERAL_REGISTER  7
#define MAX_AMD64_GENERAL_REGISTER 15
#define MIN_XMM_REGISTER 16
#define MAX_X86_XMM_REGISTER 23
#define MAX_AMD64_XMM_REGISTER 31

#define SRCADDR(op,n) (XOP_REG(op,n) - 128)
#define TMPADDR(op,n) (XOP_REG(op,n))  /* FIXME */

#define ILLOP(op) FATAL("Illegal x86 opcode %s %d %d\n", XIR_OPCODE_TABLE[op->opcode], op->operand[0].form, op->operand[1].form) 

// Convenience macros
#define X86L_IMMS_REG(opname, op) \
    if( XOP_IS_FORM(op,IMM,DST) ) { opname##_imms_r32(XOP_INT(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,IMM,SRC) ) { opname##_imms_r32disp(XOP_INT(op,0),REG_RBP,SRCADDR(op,1)); } \
    else if( XOP_IS_FORM(op,IMM,TMP) ) { opname##_imms_r32disp(XOP_INT(op,0),REG_RSP,TMPADDR(op,1)); } \
    else { ILLOP(op); }

#define X86L_REG_DST(opname,op) \
    if( XOP_IS_FORM(op,DST,DST) ) { opname##_r32_r32(XOP_REG(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,SRC,DST) ) { opname##_r32disp_r32(REG_RBP, SRCADDR(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,TMP,DST) ) { opname##_r32disp_r32(REG_RSP, TMPADDR(op,0),XOP_REG(op,1)); } \
    else { ILLOP(op); }

#define X86F_REG_DST(opname,op ) \
    if( XOP_IS_FORM(op,DST,DST) ) { opname##_xmm_xmm(XOP_REG(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,SRC,DST) ) { opname##_r32disp_xmm(REG_RBP, SRCADDR(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,TMP,DST) ) { opname##_r32disp_xmm(REG_RSP, TMPADDR(op,0),XOP_REG(op,1)); } \
    else { ILLOP(op); }
    
#define X86L_REG_REG(opname,op) \
    if( XOP_IS_FORM(op,DST,DST) ) { opname##_r32_r32(XOP_REG(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,SRC,DST) ) { opname##_r32disp_r32(REG_RBP, SRCADDR(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,DST,SRC) ) { opname##_r32_r32disp(XOP_REG(op,0),REG_RBP, SRCADDR(op,1)); } \
    else if( XOP_IS_FORM(op,TMP,DST) ) { opname##_r32disp_r32(REG_RSP, TMPADDR(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,DST,TMP) ) { opname##_r32_r32disp(XOP_REG(op,0),REG_RSP, TMPADDR(op,1)); } \
    else { ILLOP(op); }

#define X86L_REG(opname,op) \
    if( XOP_IS_DST(op,0) ) { opname##_r32(XOP_REG(op,0)); } \
    else if( XOP_IS_SRC(op,0) ) { opname##_r32disp(REG_RBP,SRCADDR(op,0)); } \
    else if( XOP_IS_TMP(op,0) ) { opname##_r32disp(REG_RSP,TMPADDR(op,0)); } \
    else { ILLOP(op); }

#define X86L_CL_REG(opname,op) \
    if( XOP_IS_FORM(op,DST,DST) && XOP_REG(op,0) == REG_CL ) { opname##_cl_r32(XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,DST,SRC) && XOP_REG(op,0) == REG_CL ) { opname##_cl_r32disp(REG_RBP, SRCADDR(op,1)); } \
    else if( XOP_IS_FORM(op,DST,TMP) && XOP_REG(op,0) == REG_CL ) { opname##_cl_r32disp(REG_RSP, TMPADDR(op,1)); } \
    else { ILLOP(op); }

#define X86L_IMMCL_REG(opname,op) \
    if( XOP_IS_FORM(op,IMM,DST) ) { opname##_imm_r32(XOP_INT(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,IMM,SRC) ) { opname##_imm_r32disp(XOP_INT(op,0),REG_RBP, SRCADDR(op,1)); } \
    else if( XOP_IS_FORM(op,IMM,TMP) ) { opname##_imm_r32disp(XOP_INT(op,0),REG_RSP, TMPADDR(op,1)); } \
    else if( XOP_IS_FORM(op,DST,DST) && XOP_REG(op,0) == REG_CL ) { opname##_cl_r32(XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,DST,SRC) && XOP_REG(op,0) == REG_CL ) { opname##_cl_r32disp(REG_RBP, SRCADDR(op,1)); } \
    else if( XOP_IS_FORM(op,DST,TMP) && XOP_REG(op,0) == REG_CL ) { opname##_cl_r32disp(REG_RSP, TMPADDR(op,1)); } \
    else { ILLOP(op); }

// Standard ALU forms - imms,reg or reg,reg
#define X86L_ALU_REG(opname,op) \
    if( XOP_IS_FORM(op,IMM,DST) ) { opname##_imms_r32(XOP_INT(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,IMM,SRC) ) { opname##_imms_r32disp(XOP_INT(op,0),REG_RBP, SRCADDR(op,1)); } \
    else if( XOP_IS_FORM(op,IMM,TMP) ) { opname##_imms_r32disp(XOP_INT(op,0),REG_RSP, TMPADDR(op,1)); } \
    else if( XOP_IS_FORM(op,DST,DST) ) { opname##_r32_r32(XOP_REG(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,SRC,DST) ) { opname##_r32disp_r32(REG_RBP, SRCADDR(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,DST,SRC) ) { opname##_r32_r32disp(XOP_REG(op,0),REG_RBP, SRCADDR(op,1)); } \
    else if( XOP_IS_FORM(op,TMP,DST) ) { opname##_r32disp_r32(REG_RSP, TMPADDR(op,0),XOP_REG(op,1)); } \
    else if( XOP_IS_FORM(op,DST,TMP) ) { opname##_r32_r32disp(XOP_REG(op,0),REG_RSP, TMPADDR(op,1)); } \
    else { ILLOP(op); }

uint32_t x86_target_get_code_size( xir_op_t begin, xir_op_t end )
{
    return -1;
}


/**
 * Note: Assumes that the IR is x86-legal (ie doesn't contain any unencodeable instructions).
 */
uint32_t x86_target_codegen( target_data_t td, xir_op_t begin, xir_op_t end )
{
    int ss;
    xir_op_t it;
    
    /* Prologue */
    
    for( it=begin; it != NULL; it = it->next ) {
        switch( it->opcode ) {
        case OP_ENTER:
        case OP_BARRIER:
        case OP_NOP:
            /* No code to generate */
            break;
        case OP_MOV:
            if( XOP_IS_FORM(it, IMM, DST) ) {
                MOVL_imm32_r32( XOP_INT(it,0), XOP_REG2(it) );
            } else if( XOP_IS_FORM(it, IMM, SRC) ) {
                MOVL_imm32_r32disp( XOP_INT(it,0), REG_RBP, SRCADDR(it,1) );
            } else if( XOP_IS_FORM(it, IMM, TMP) ) {
                MOVL_imm32_r32disp( XOP_INT(it,0), REG_RSP, TMPADDR(it,1) );
            } else if( XOP_IS_FORM(it, DST, SRC) ) {
                if( IS_XMM_REG(it,0) ) {
                    MOVSS_xmm_r32disp( XOP_REG1(it), REG_RBP, SRCADDR(it,1) );
                } else {
                    MOVL_r32_r32disp( XOP_REG1(it), REG_RBP, SRCADDR(it,1) );
                }
            } else if( XOP_IS_FORM(it, DST, DST) ) {
                if( IS_XMM_REG(it,0) ) {
                    if( IS_XMM_REG(it,1) ) {
                        MOVSS_xmm_xmm( XOP_REG1(it), XOP_REG2(it) );
                    } else {
                        MOVL_xmm_r32( XOP_REG1(it), XOP_REG2(it) );
                    }
                } else if( IS_XMM_REG(it,1) ) {
                    MOVL_r32_xmm( XOP_REG1(it), XOP_REG2(it) );
                } else {
                    MOVL_r32_r32( XOP_REG1(it), XOP_REG2(it) );
                }
            } else if( XOP_IS_FORM(it, SRC, DST) ) {
                if( IS_XMM_REG(it,1) ) {
                    MOVSS_r32disp_xmm( REG_RBP, SRCADDR(it,0), XOP_REG2(it) );
                } else {
                    MOVL_r32disp_r32( REG_RBP, SRCADDR(it,0), XOP_REG2(it) );
                }
            } else {
                ILLOP(it);
            }
            break;
        case OP_MOVQ:
            if( XOP_IS_FORM(it, IMM, SRC) ) {
                ILLOP(it);
            } else if( XOP_IS_FORM(it, IMM, DST) ) {
                if( IS_XMM_REG(it,0) ) {
                    if( XOP_INT(it,0) == 0 ) {
                        XORPD_xmm_xmm( XOP_REG2(it), XOP_REG2(it) );
                    }
                } else {
                    MOVQ_imm64_r64( XOP_INT(it,0), XOP_REG2(it) );
                }
            } else if( XOP_IS_FORM(it, DST, SRC) ) {
                if( IS_XMM_REG(it,0) ) {
                    MOVSD_xmm_r32disp( XOP_REG1(it), REG_RBP, SRCADDR(it,1) );
                } else {
                    MOVQ_r64_r64disp( XOP_REG1(it), REG_RBP, SRCADDR(it,1) );
                }
            } else if( XOP_IS_FORM(it, DST, DST) ) {
                if( IS_XMM_REG(it,0) ) {
                    if( IS_XMM_REG(it,1) ) {
                        MOVSD_xmm_xmm( XOP_REG1(it), XOP_REG2(it) );
                    } else {
                        MOVQ_xmm_r64( XOP_REG1(it), XOP_REG2(it) );
                    }
                } else if( IS_XMM_REG(it,1) ) {
                    MOVQ_r64_xmm( XOP_REG1(it), XOP_REG2(it) );
                } else {
                    MOVQ_r64_r64( XOP_REG1(it), XOP_REG2(it) );
                }
            } else if( XOP_IS_FORM(it, SRC, DST) ) {
                if( IS_XMM_REG(it,1) ) {
                    MOVSD_r32disp_xmm( REG_RBP, SRCADDR(it,0), XOP_REG2(it) );
                } else {
                    MOVQ_r64disp_r64( REG_RBP, SRCADDR(it,0), XOP_REG2(it) );
                }
            } else {
                ILLOP(it);
            }
            break;
        case OP_MOVSX8:
            if( XOP_IS_FORM(it, DST, DST) ) {
                MOVSXL_r8_r32( XOP_REG1(it), XOP_REG2(it) );
            } else if( XOP_IS_FORM(it, SRC, DST) ) {
                MOVSXL_r32disp8_r32( REG_RBP, SRCADDR(it,0), XOP_REG2(it) );
            } else {
                ILLOP(it);
            }
            break;
        case OP_MOVSX16:
            if( XOP_IS_FORM(it, DST, DST) ) {
                MOVSXL_r16_r32( XOP_REG1(it), XOP_REG2(it) );
            } else if( XOP_IS_FORM(it, SRC, DST) ) {
                MOVSXL_r32disp16_r32( REG_RBP, SRCADDR(it,0), XOP_REG2(it) );
            } else {
                ILLOP(it);
            }
            break;
        case OP_MOVZX8:
            if( XOP_IS_FORM(it, DST, DST) ) {
                MOVZXL_r8_r32( XOP_REG1(it), XOP_REG2(it) );
            } else if( XOP_IS_FORM(it, SRC, DST) ) {
                MOVZXL_r32disp8_r32( REG_RBP, SRCADDR(it,0), XOP_REG2(it) );
            } else {
                ILLOP(it);
            }
            break;
        case OP_MOVZX16:
            if( XOP_IS_FORM(it, DST, DST) ) {
                MOVZXL_r16_r32( XOP_REG1(it), XOP_REG2(it) );
            } else if( XOP_IS_FORM(it, SRC, DST) ) {
                MOVZXL_r32disp16_r32( REG_RBP, SRCADDR(it,0), XOP_REG2(it) );
            } else {
                ILLOP(it);
            }
            break;
        case OP_ADD: 
        case OP_ADDS: X86L_ALU_REG(ADDL,it);  break;
        case OP_ADDCS: X86L_ALU_REG(ADCL,it); break;
        case OP_AND:  X86L_ALU_REG(ANDL,it); break;
        case OP_CMP:
            X86L_ALU_REG(CMPL,it); break;
        case OP_DEC:
            if( XOP_IS_FORM(it,DST,NONE) ) {
                DECL_r32(XOP_REG(it,0));
            } else if( XOP_IS_FORM(it,SRC,NONE) ) {
                DECL_r32disp( REG_RBP, SRCADDR(it,0) );
            } else {
                ILLOP(it);
            }
            break;
        case OP_MUL: 
            X86L_REG_DST(IMULL,it); 
            break;
        case OP_NEG:  X86L_REG(NEGL,it); break;
        case OP_NOT:  X86L_REG(NOTL,it); break;
        case OP_OR:   X86L_ALU_REG(ORL,it); break;
        case OP_RCL:  X86L_IMMCL_REG(RCLL,it); break;
        case OP_RCR:  X86L_IMMCL_REG(RCRL,it); break;
        case OP_ROL:  X86L_IMMCL_REG(ROLL,it); break;
        case OP_ROR:  X86L_IMMCL_REG(RORL,it); break;
        case OP_SAR: 
        case OP_SARS: X86L_IMMCL_REG(SARL,it); break;
        case OP_SUBBS: X86L_ALU_REG(SBBL,it); break;
        case OP_SLL: 
        case OP_SLLS: X86L_IMMCL_REG(SHLL,it); break;
        case OP_SLR: 
        case OP_SLRS: X86L_IMMCL_REG(SHRL,it); break;
        case OP_SUB: 
        case OP_SUBS: X86L_ALU_REG(SUBL,it); break;
        case OP_SHUFFLE:
            if( XOP_IS_FORM(it,IMM,DST) ) {
                if( XOP_INT(it,0) == 0x4321 ) {
                    BSWAPL_r32( XOP_REG(it,1) );
                } else if( it->operand[1].value.i == 0x1243 ) {
                    XCHGB_r8_r8( REG_AL, REG_AH ); 
                            /* XCHG al, ah */
                }
            }
            break;
        case OP_TST:  X86L_ALU_REG(TESTL,it); break;
        case OP_XOR:  X86L_ALU_REG(XORL,it); break;
            
            // Float
        case OP_ABSF:
        case OP_ABSD:
            // Why is there no SSE FP ABS instruction?
            break;
        case OP_ADDF: X86F_REG_DST(ADDSS,it); break;
        case OP_ADDD: X86F_REG_DST(ADDSD,it); break;
        case OP_CMPF:
            break;
        case OP_CMPD: // UCOMISD
            break;
        case OP_DIVF: X86F_REG_DST(DIVSS,it); break;
        case OP_DIVD: X86F_REG_DST(DIVSD,it); break;
        case OP_MULF: X86F_REG_DST(MULSS,it); break;
        case OP_MULD: X86F_REG_DST(MULSD,it); break;
        case OP_RSQRTF:X86F_REG_DST(RSQRTSS,it); break;
        case OP_SQRTF: X86F_REG_DST(SQRTSS,it); break;
        case OP_SQRTD: X86F_REG_DST(SQRTSD,it); break;
        case OP_SUBF:  X86F_REG_DST(SUBSS,it); break;
        case OP_SUBD:  X86F_REG_DST(SUBSD,it); break;

        case OP_DOTPRODV:
            MULPS_r32disp_xmm( REG_RBP, SRCADDR(it,0), 4 );
            HADDPS_xmm_xmm( 4, 4 ); 
            HADDPS_xmm_xmm( 4, 4 );
            MOVSS_xmm_r32disp( 4, REG_RBP, SRCADDR(it,0) );
            break;
        case OP_SINCOSF:
        case OP_MATMULV:
            break;
        case OP_FTOD:
            if( XOP_IS_FORM(it,DST,DST) ) {
                CVTSS2SD_xmm_xmm( XOP_REG(it,0), XOP_REG(it,1) );
            } else if( XOP_IS_FORM(it,SRC,DST) ) {
                CVTSS2SD_r32disp_xmm( REG_RBP, SRCADDR(it,0), XOP_REG(it,1) );
            } else {
                ILLOP(it);
            }
            break;
        case OP_DTOF:
            if( XOP_IS_FORM(it,DST,DST) ) {
                CVTSS2SD_xmm_xmm( XOP_REG(it,0), XOP_REG(it,1) );
            } else if( XOP_IS_FORM(it, SRC,DST) ) {
                CVTSS2SD_r32disp_xmm( REG_RBP, SRCADDR(it,0), XOP_REG(it,1) );
            } else {
                ILLOP(it);
            }
            break;
        case OP_ITOD:
            if( XOP_IS_FORM(it,DST,DST) ) {
                CVTSI2SDL_r32_xmm( XOP_REG(it,0), XOP_REG(it,1) );
            } else if( XOP_IS_FORM(it,SRC,DST) ) {
                CVTSI2SDL_r32disp_xmm( REG_RBP, SRCADDR(it,0), XOP_REG(it,1) );
            } else {
                ILLOP(it);
            }
            break;
        case OP_DTOI:
            if( XOP_IS_FORM(it,DST,DST) ) {
                CVTSD2SIL_xmm_r32( XOP_REG(it,0), XOP_REG(it,1) );
            } else if( XOP_IS_FORM(it,SRC,DST) ) {
                CVTSD2SIL_r32disp_r32( REG_RBP, SRCADDR(it,0), XOP_REG(it,1) );
            } else {
                ILLOP(it);
            }
            break;
        case OP_ITOF:
        case OP_FTOI:
            
        case OP_CALL0: 
            if( XOP_IS_IMM(it,0) ) { 
                CALL_imm32( XOP_INT(it,0) );
            } else if( XOP_IS_SRC(it,0) ) {
                CALL_r32( XOP_INT(it,0) );
            } else {
                ILLOP(it);
            }
            break;
        case OP_XLAT:
            if( IS_X86_64() ) {
                ss = 3;
            } else {
                ss = 2;
            }
            if( XOP_IS_FORM(it,IMM,DST) ) {
                MOVP_sib_rptr(ss, XOP_REG(it,1), -1, XOP_INT(it,0), XOP_REG(it,1));
            } else if( XOP_IS_FORM(it,DST,DST) ) {
                MOVP_sib_rptr(ss, XOP_REG(it,1), XOP_REG(it,0), 0, XOP_REG(it,1));
            } else {
                ILLOP(it);
            }
            break;
        case OP_CALLLUT:
            if( XOP_IS_FORM(it,DST,IMM) ) {
                CALL_r32disp(XOP_REG(it,0),XOP_INT(it,1));
            } else if( XOP_IS_FORM(it,DST,DST) ) {
                CALL_sib(0,XOP_REG(it,0),XOP_REG(it,1),0);
            } else if( XOP_IS_FORM(it,IMM,DST) ) {
                CALL_r32disp(XOP_REG(it,1),XOP_INT(it,0));
            } else {
                ILLOP(it);
            }
            break;

            // SH4-specific macro operations
        case OP_RAISEME:
            
        case OP_RAISEMNE:
            
        case OP_CMPSTR:
            break;
        case OP_DIV1: 
            break;
        case OP_SHAD:
            assert( it->operand[0].form == DST && XOP_REG(it,0) == REG_ECX );
            CMPL_imms_r32(0,REG_ECX);
            JNGE_label(shad_shr);
            X86L_CL_REG(SHLL,it);
            JMP_label(shad_end);

            JMP_TARGET(shad_shr);
            if( IS_X86_64() && it->operand[1].form == DST ) {
                /* We can do this a little more simply with a 64-bit shift */
                ORL_imms_r32(0xFFFFFFE0,REG_ECX);
                NEGL_r32(REG_ECX);
                MOVSXQ_r32_r64(XOP_REG(it,1), XOP_REG(it,1)); // sign-extend
                SARQ_cl_r64(XOP_REG(it,1));
            } else {
                NEGL_r32(REG_ECX);
                ANDB_imms_r8( 0x1F, REG_ECX );
                JE_label(emptyshr );
                X86L_CL_REG(SARL,it);
                JMP_label(shad_end2);

                JMP_TARGET(emptyshr);
                if( it->operand[1].form == DST ) {
                    SARL_imm_r32( 31, XOP_REG(it,1) );
                } else if( it->operand[1].form == SRC ) {
                    SARL_imm_r32disp( 32, REG_RBP, SRCADDR(it,1) );
                } else {
                    SARL_imm_r32disp( 32, REG_RSP, TMPADDR(it,1) );
                }
                JMP_TARGET(shad_end2);
            }
            JMP_TARGET(shad_end);
            break;

        case OP_SHLD:
            assert( it->operand[0].form == DST && XOP_REG(it,0) == REG_ECX );
            CMPL_imms_r32(0,REG_ECX);
            JNGE_label(shld_shr);
            X86L_CL_REG(SHLL,it);
            JMP_label(shld_end);

            JMP_TARGET(shld_shr);
            if( IS_X86_64() && it->operand[1].form == DST ) {
                /* We can do this a little more simply with a 64-bit shift */
                ORL_imms_r32(0xFFFFFFE0,REG_ECX);
                NEGL_r32(REG_ECX);
                MOVL_r32_r32(XOP_REG(it,1), XOP_REG(it,1)); // Ensure high bits are 0
                SHRQ_cl_r64(XOP_REG(it,1));
            } else {
                NEGL_r32(REG_ECX);
                ANDB_imms_r8( 0x1F, REG_ECX );
                JE_label(emptyshr );
                X86L_CL_REG(SHRL,it);
                JMP_label(shld_end2);

                JMP_TARGET(emptyshr);
                XORL_r32_r32( REG_EAX, REG_EAX );
                JMP_TARGET(shld_end2);
            }
            JMP_TARGET(shld_end);
            break;

        case OP_MULQ:
        case OP_ADDQSAT32:
        case OP_ADDQSAT48:

            // Should not occur (should be have been lowered in target_lower)
        case OP_NEGF:
        case OP_NEGD:
        case OP_LOADB:
        case OP_LOADBFW:
        case OP_LOADW:
        case OP_LOADL:
        case OP_LOADQ:
        case OP_STOREB:
        case OP_STOREW:
        case OP_STOREL:
        case OP_STOREQ:
        case OP_STORELCA:
        case OP_OCBI:
        case OP_OCBP:
        case OP_OCBWB:
        case OP_PREF:
        default:
            ILLOP(it);
        }
        if( it == end )
            break;
    /* Epilogue */
    }
}
