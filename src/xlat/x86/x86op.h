/**
 * $Id$
 * 
 * x86/x86-64 Instruction generator
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

#ifndef lxdream_x86op_H
#define lxdream_x86op_H

#include <stdint.h>
#include <assert.h>

/******************************** Constants *****************************/

#define REG_NONE -1

/* 64-bit general-purpose regs */
#define REG_RAX 0
#define REG_RCX 1
#define REG_RDX 2
#define REG_RBX 3
#define REG_RSP 4
#define REG_RBP 5
#define REG_RSI 6 
#define REG_RDI 7
#define REG_R8  8
#define REG_R9  9
#define REG_R10 10
#define REG_R11 11
#define REG_R12 12
#define REG_R13 13
#define REG_R14 14
#define REG_R15 15

/* 32-bit general-purpose regs */
#define REG_EAX  0
#define REG_ECX  1
#define REG_EDX  2
#define REG_EBX  3
#define REG_ESP  4
#define REG_EBP  5
#define REG_ESI  6 
#define REG_EDI  7
#define REG_R8D  8
#define REG_R9D  9
#define REG_R10D 10
#define REG_R11D 11
#define REG_R12D 12
#define REG_R13D 13
#define REG_R14D 14
#define REG_R15D 15

/* 8-bit general-purpose regs (no-rex prefix) */
#define REG_AL   0
#define REG_CL   1
#define REG_DL   2
#define REG_BL   3
#define REG_AH   4
#define REG_CH   5
#define REG_DH   6
#define REG_BH   7

/* 8-bit general-purpose regs (rex-prefix) */
#define REG_SPL  4
#define REG_BPL  5
#define REG_SIL  6
#define REG_DIL  7
#define REG_R8L  8
#define REG_R9L  9
#define REG_R10L 10
#define REG_R11L 11
#define REG_R12L 12
#define REG_R13L 13
#define REG_R14L 14
#define REG_R15L 15

/* Condition flag variants */
#define X86_COND_O   0x00  /* OF=1 */
#define X86_COND_NO  0x01  /* OF=0 */
#define X86_COND_B   0x02  /* CF=1 */
#define X86_COND_C   0x02  /* CF=1 */
#define X86_CONF_NAE 0x02  /* CF=1 */
#define X86_COND_AE  0x03  /* CF=0 */
#define X86_COND_NB  0x03  /* CF=0 */
#define X86_COND_NC  0x03  /* CF=0 */
#define X86_COND_E   0x04  /* ZF=1 */
#define X86_COND_Z   0x04  /* ZF=1 */
#define X86_COND_NE  0x05  /* ZF=0 */
#define X86_COND_NZ  0x05  /* ZF=0 */
#define X86_COND_BE  0x06  /* CF=1 || ZF=1 */
#define X86_COND_NA  0x06  /* CF=1 || ZF=1 */
#define X86_COND_A   0x07  /* CF=0 && ZF=0 */
#define X86_COND_NBE 0x07  /* CF=0 && ZF=0 */
#define X86_COND_S   0x08  /* SF=1 */
#define X86_COND_NS  0x09  /* SF=0 */
#define X86_COND_P   0x0A  /* PF=1 */
#define X86_COND_PE  0x0A  /* PF=1 */
#define X86_COND_NP  0x0B  /* PF=0 */
#define X86_COND_PO  0x0B  /* PF=0 */
#define X86_COND_L   0x0C  /* SF!=OF */
#define X86_COND_NGE 0x0C  /* SF!=OF */
#define X86_COND_GE  0x0D  /* SF=OF */
#define X86_COND_NL  0x0D  /* SF=OF */
#define X86_COND_LE  0x0E  /* ZF=1 || SF!=OF */
#define X86_COND_NG  0x0E  /* ZF=1 || SF!=OF */
#define X86_COND_G   0x0F  /* ZF=0 && SF=OF */
#define X86_COND_NLE 0x0F  /* ZF=0 && SF=OF */

/* SSE floating pointer comparison variants */
#define SSE_CMP_EQ    0x00
#define SSE_CMP_LT    0x01
#define SSE_CMP_LE    0x02
#define SSE_CMP_UNORD 0x03
#define SSE_CMP_NE    0x04
#define SSE_CMP_NLT   0x05
#define SSE_CMP_NLE   0x06
#define SSE_CMP_ORD   0x07

/************************** Internal definitions ***************************/
#define PREF_REXB 0x41
#define PREF_REXX 0x42
#define PREF_REXR 0x44
#define PREF_REXW 0x48

/* PREF_REXW if required for pointer operations, otherwise 0 */
#define PREF_PTR     ((sizeof(void *) == 8) ? PREF_REXW : 0) 

extern unsigned char *xlat_output;

#define OP(x) *xlat_output++ = (x)
#define OP16(x) *((uint16_t *)xlat_output) = (x); xlat_output+=2
#define OP32(x) *((uint32_t *)xlat_output) = (x); xlat_output+=4
#define OP64(x) *((uint64_t *)xlat_output) = (x); xlat_output+=8
#define OPPTR(x) *((void **)xlat_output) = ((void *)x); xlat_output+=(sizeof(void*))

/* Primary opcode emitter, eg OPCODE(0x0FBE) for MOVSX */
#define OPCODE(x) if( (x) > 0xFFFF ) { OP((x)>>16); OP(((x)>>8)&0xFF); OP((x)&0xFF); } else if( (x) > 0xFF ) { OP((x)>>8); OP((x)&0xFF); } else { OP(x); }

/* Test if immediate value is representable as a signed 8-bit integer */
#define IS_INT8(imm) ((imm) >= INT8_MIN && (imm) <= INT8_MAX)

/**
 * Encode opcode+reg with no mod/rm (eg MOV imm64, r32)
 */
static void x86_encode_opcodereg( int rexw, uint32_t opcode, int reg )
{
    int rex = rexw;
    reg &= 0x0F;
    if( reg >= 8 ) {
        rex |= PREF_REXB;
        reg -= 8;
    }
    if( rex != 0 ) {
        OP(rex);
    }
    OPCODE(opcode + reg);
}

/**
 * Encode opcode with mod/rm reg-reg operation.
 * @param opcode primary instruction opcode
 * @param rr reg field 
 * @param rb r/m field
 */
static void x86_encode_reg_rm( int rexw, uint32_t opcode, int rr, int rb )
{
    int rex = rexw;
    rr &= 0x0F;
    rb &= 0x0F;
    if( rr >= 8 ) {
        rex |= PREF_REXR;
        rr -= 8;
    }
    if( rb >= 8 ) {
        rex |= PREF_REXB;
        rb -= 8;
    }
    if( rex != 0 ) {
        OP(rex);
    }
    OPCODE(opcode);
    OP(0xC0|(rr<<3)|rb);
}

/**
 * Encode opcode + 32-bit mod/rm memory address. (RIP-relative not supported here)
 * @param rexw REX.W prefix is required, otherwise 0
 * @param rr Reg-field register (required). 
 * @param rb Base (unscaled) register, or -1 for no base register. 
 * @param rx Index (scaled) register, or -1 for no index register
 * @param ss Scale shift (0..3) applied to index register (ignored if no index register)
 * @param disp32 Signed displacement (0 for none)
 */ 
static void x86_encode_modrm( int rexw, uint32_t opcode, int rr, int rb, int rx, int ss, int32_t disp32 )
{
    /* Construct the rex prefix where necessary */
    int rex = rexw;
    rr &= 0x0F;
    if( rr >= 8 ) {
        rex |= PREF_REXR;
        rr -= 8;
    }
    if( rb != -1 ) {
        rb &= 0x0F;
        if( rb >= 8 ) {
            rex |= PREF_REXB;
            rb -= 8;
        }
    }
    if( rx != -1 ) {
        rx &= 0x0F;
        if( rx >= 8 ) {
            rex |= PREF_REXX;
            rx -= 8;
        }
    }
    
    if( rex != 0 ) {
        OP(rex);
    }
    OPCODE(opcode);
    
    if( rx == -1 ) {
        if( rb == -1 ) {
            /* [disp32] displacement only - use SIB form for 64-bit mode safety */
            OP(0x04|(rr<<3));
            OP(0x25);
            OP32(disp32);
        } else if( rb == REG_ESP ) { /* [%esp + disp32] - SIB is mandatory for %esp/%r12 encodings */
            if( disp32 == 0 ) {
                OP(0x04|(rr<<3));
                OP(0x24);
            } else if( IS_INT8(disp32) ) {
                OP(0x44|(rr<<3));
                OP(0x24);
                OP((int8_t)disp32);
            } else {
                OP(0x84|(rr<<3));
                OP(0x24);
                OP32(disp32);
            }
        } else {
            if( disp32 == 0 && rb != REG_EBP ) { /* [%ebp] is encoded as [%ebp+0] */
                OP((rr<<3)|rb);
            } else if( IS_INT8(disp32) ) {
                OP(0x40|(rr<<3)|rb);
                OP((int8_t)disp32);
            } else {
                OP(0x80|(rr<<3)|rb);
                OP32(disp32);
            }
        }
    } else { /* We have a scaled index. Goody */
        assert( ((rx != REG_ESP) || (rex&PREF_REXX)) && "Bug: attempt to index through %esp" ); /* Indexing by %esp is impossible */
        if( rb == -1 ) { /* [disp32 + rx << ss] */
            OP(0x04|(rr<<3));
            OP(0x05|(ss<<6)|(rx<<3));
            OP32(disp32);
        } else if( disp32 == 0 && rb != REG_EBP ) { /* [rb + rx << ss]. (Again, %ebp needs to be %ebp+0) */
            OP(0x04|(rr<<3));
            OP((ss<<6)|(rx<<3)|rb);
        } else if( IS_INT8(disp32) ) {
            OP(0x44|(rr<<3));
            OP((ss<<6)|(rx<<3)|rb);
            OP((int8_t)disp32);
        } else {
            OP(0x84|(rr<<3));
            OP((ss<<6)|(rx<<3)|rb);
            OP32(disp32);
        }
    }
}

/**
 * Encode opcode + RIP-relative mod/rm (64-bit mode only)
 * @param rexw PREF_REXW or 0
 * @param opcode primary instruction opcode
 * @param rr mod/rm reg field
 * @param disp32 RIP-relative displacement
 */
static void x86_encode_modrm_rip(int rexw, uint32_t opcode, int rr, int32_t disp32)
{
    int rex = rexw;
    rr &= 0x0F;
    if( rr >= 8 ) {
        rex |= PREF_REXR;
        rr -= 8;
    }
    if( rex != 0 ) {
        OP(rex);
    }
    OPCODE(opcode);
    OP(0x05|(rr<<3));
    OP32(disp32);
}

/* 32/64-bit op emitters. 64-bit versions include a rex.w prefix. Note that any
 * other prefixes (mandatory or otherwise) need to be emitted prior to these 
 * functions
 */ 
#define x86_encode_opcode64(opcode,reg) x86_encode_opcodereg(PREF_REXW, opcode,reg)
#define x86_encode_opcode32(opcode,reg) x86_encode_opcodereg(0,opcode,reg)
#define x86_encode_r32_rm32(opcode,rr,rb) x86_encode_reg_rm(0,opcode,rr,rb)
#define x86_encode_r64_rm64(opcode,rr,rb) x86_encode_reg_rm(PREF_REXW,opcode,rr,rb)
#define x86_encode_r32_mem32(opcode,rr,rb,rx,ss,disp32) x86_encode_modrm(0,opcode,rr,rb,rx,ss,disp32)
#define x86_encode_r64_mem64(opcode,rr,rb,rx,ss,disp32) x86_encode_modrm(PREF_REXW,opcode,rr,rb,rx,ss,disp32)
#define x86_encode_rptr_memptr(opcode,rr,rb,rx,ss,disp32) x86_encode_modrm(PREF_PTR,opcode,rr,rb,rx,ss,disp32)
#define x86_encode_r32_mem32disp32(opcode,rr,rb,disp32) x86_encode_modrm(0,opcode,rr,rb,-1,0,disp32)
#define x86_encode_r64_mem64disp64(opcode,rr,rb,disp32) x86_encode_modrm(PREF_REXW,opcode,rr,rb,-1,0,disp32)
#define x86_encode_r32_ripdisp32(opcode,rr,disp32) x86_encode_modrm_rip(0,opcode,rr,disp32)
#define x86_encode_r64_ripdisp64(opcode,rr,disp32) x86_encode_modrm_rip(PREF_REXW,opcode,rr,disp32)

/* Convenience versions for the common rbp/rsp relative displacements */
#define x86_encode_r32_rbpdisp32(opcode,rr,disp32) x86_encode_modrm(0,opcode,rr,REG_RBP,-1,0,disp32)
#define x86_encode_r64_rbpdisp64(opcode,rr,disp32) x86_encode_modrm(PREF_REXW,opcode,rr,REG_RBP,-1,0,disp32)
#define x86_encode_r32_rspdisp32(opcode,rr,disp32) x86_encode_modrm(0,opcode,rr,REG_RSP,-1,0,disp32)
#define x86_encode_r64_rspdisp64(opcode,rr,disp32) x86_encode_modrm(PREF_REXW,opcode,rr,REG_RSP,-1,0,disp32)

/* Immediate-selection variants (for instructions with imm8s/imm32 variants) */
#define x86_encode_imms_rm32(opcode8,opcode32,reg,imm,rb) \
    if( IS_INT8(((int32_t)imm)) ) { x86_encode_r32_rm32(opcode8,reg,rb); OP((int8_t)imm); \
                } else { x86_encode_r32_rm32(opcode32,reg,rb); OP32(imm); }
#define x86_encode_imms_rm64(opcode8,opcode32,reg,imm,rb) \
    if( IS_INT8(((int32_t)imm)) ) { x86_encode_r64_rm64(opcode8,reg,rb); OP((int8_t)imm); \
                } else { x86_encode_r64_rm64(opcode32,reg,rb); OP32(imm); }
#define x86_encode_imms_rmptr(opcode8,opcode32,reg,imm,rb) \
    if( IS_INT8(((int32_t)imm)) ) { x86_encode_reg_rm( PREF_PTR, opcode8,reg,rb); OP((int8_t)imm); \
                } else { x86_encode_reg_rm( PREF_PTR, opcode32,reg,rb); OP32(imm); }
#define x86_encode_imms_rbpdisp32(opcode8,opcode32,reg,imm,disp) \
    if( IS_INT8(((int32_t)imm)) ) { x86_encode_r32_rbpdisp32(opcode8,reg,disp); OP((int8_t)imm); \
                } else { x86_encode_r32_rbpdisp32(opcode32,reg,disp); OP32(imm); }
#define x86_encode_imms_r32disp32(opcode8,opcode32,reg,imm,rb,disp) \
    if( IS_INT8(((int32_t)imm)) ) { x86_encode_r32_mem32disp32(opcode8,reg,rb,disp); OP((int8_t)imm); \
                } else { x86_encode_r32_mem32disp32(opcode32,reg,rb,disp); OP32(imm); }
#define x86_encode_imms_rbpdisp64(opcode8,opcode32,reg,imm,disp) \
    if( IS_INT8(((int32_t)imm)) ) { x86_encode_r64_rbpdisp64(opcode8,reg,disp); OP((int8_t)imm); \
                } else { x86_encode_r64_rbpdisp64(opcode32,reg,disp); OP32(imm); }

/*************************** Instruction definitions ***********************/
/* Note this does not try to be an exhaustive definition of the instruction -
 * it generally only has the forms that we actually need here.
 */
/* Core Integer instructions */
#define ADCB_imms_r8(imm,r1)         x86_encode_r32_rm32(0x80, 2, r1); OP(imm)
#define ADCB_r8_r8(r1,r2)            x86_encode_r32_rm32(0x10, r1, r2)
#define ADCL_imms_r32(imm,r1)        x86_encode_imms_rm32(0x83, 0x81, 2, imm, r1)
#define ADCL_imms_rbpdisp(imm,disp)  x86_encode_imms_rbpdisp32(0x83, 0x81, 2, imm, disp)
#define ADCL_r32_r32(r1,r2)          x86_encode_r32_rm32(0x11, r1, r2)
#define ADCL_r32_rbpdisp(r1,disp)    x86_encode_r32_rbpdisp32(0x11, r1, disp)
#define ADCL_rbpdisp_r32(disp,r1)    x86_encode_r32_rbpdisp32(0x13, r1, disp)
#define ADCQ_imms_r64(imm,r1)        x86_encode_imms_rm64(0x83, 0x81, 2, imm, r1)
#define ADCQ_r64_r64(r1,r2)          x86_encode_r64_rm64(0x11, r1, r2)

#define ADDB_imms_r8(imm,r1)         x86_encode_r32_rm32(0x80, 0, r1); OP(imm)
#define ADDB_r8_r8(r1,r2)            x86_encode_r32_rm32(0x00, r1, r2)
#define ADDL_imms_r32(imm,r1)        x86_encode_imms_rm32(0x83, 0x81, 0, imm, r1)
#define ADDL_imms_r32disp(imm,rb,d)  x86_encode_imms_r32disp32(0x83, 0x81, 0, imm, rb, d)
#define ADDL_imms_rbpdisp(imm,disp)  x86_encode_imms_rbpdisp32(0x83, 0x81, 0, imm, disp)
#define ADDL_r32_r32(r1,r2)          x86_encode_r32_rm32(0x01, r1, r2)
#define ADDL_r32_rbpdisp(r1,disp)    x86_encode_r32_rbpdisp32(0x01, r1, disp)
#define ADDL_r32_r32disp(r1,r2,dsp)  x86_encode_r32_mem32disp32(0x01, r1, r2, dsp)
#define ADDL_rbpdisp_r32(disp,r1)    x86_encode_r32_rbpdisp32(0x03, r1, disp)
#define ADDQ_imms_r64(imm,r1)        x86_encode_imms_rm64(0x83, 0x81, 0, imm, r1)
#define ADDQ_r64_r64(r1,r2)          x86_encode_r64_rm64(0x01, r1, r2)

#define ANDB_imms_r8(imm,r1)         x86_encode_r32_rm32(0x80, 4, r1); OP(imm)
#define ANDB_r8_r8(r1,r2)            x86_encode_r32_rm32(0x20, r1, r2)
#define ANDL_imms_r32(imm,r1)        x86_encode_imms_rm32(0x83, 0x81, 4, imm, r1)
#define ANDL_imms_rbpdisp(imm,disp)  x86_encode_imms_rbpdisp32(0x83,0x81,4,imm,disp)
#define ANDL_r32_r32(r1,r2)          x86_encode_r32_rm32(0x21, r1, r2)
#define ANDL_r32_rbpdisp(r1,disp)    x86_encode_r32_rbpdisp32(0x21, r1, disp)
#define ANDL_rbpdisp_r32(disp,r1)    x86_encode_r32_rbpdisp32(0x23, r1, disp)
#define ANDQ_r64_r64(r1,r2)          x86_encode_r64_rm64(0x21, r1, r2)
#define ANDQ_imms_r64(imm,r1)        x86_encode_imms_rm64(0x83, 0x81, 4, imm, r1)
#define ANDP_imms_rptr(imm,r1)       x86_encode_imms_rmptr(0x83, 0x81, 4, imm, r1)       

#define CLC()                        OP(0xF8)
#define CLD()                        OP(0xFC)
#define CMC()                        OP(0xF5)

#define CMOVCCL_cc_r32_r32(cc,r1,r2) x86_encode_r32_rm32(0x0F40+(cc), r2, r1)
#define CMOVCCL_cc_rbpdisp_r32(cc,d,r1) x86_encode_r32_rbpdisp32(0x0F40+(cc), r1, d)

#define CMPB_imms_r8(imm,r1)         x86_encode_r32_rm32(0x80, 7, r1); OP(imm)
#define CMPB_imms_rbpdisp(imm,disp)  x86_encode_r32_rbpdisp32(0x80, 7, disp); OP(imm)
#define CMPB_r8_r8(r1,r2)            x86_encode_r32_rm32(0x38, r1, r2)
#define CMPL_imms_r32(imm,r1)        x86_encode_imms_rm32(0x83, 0x81, 7, imm, r1)
#define CMPL_imms_rbpdisp(imm,disp)  x86_encode_imms_rbpdisp32(0x83, 0x81, 7, imm, disp)
#define CMPL_r32_r32(r1,r2)          x86_encode_r32_rm32(0x39, r1, r2)
#define CMPL_r32_rbpdisp(r1,disp)    x86_encode_r32_rbpdisp32(0x39, r1, disp)
#define CMPL_rbpdisp_r32(disp,r1)    x86_encode_r32_rbpdisp32(0x3B, r1, disp)
#define CMPQ_imms_r64(imm,r1)        x86_encode_imms_rm64(0x83, 0x81, 7, imm, r1)
#define CMPQ_r64_r64(r1,r2)          x86_encode_r64_rm64(0x39, r1, r2)

#define IDIVL_r32(r1)                x86_encode_r32_rm32(0xF7, 7, r1)
#define IDIVL_rbpdisp(disp)          x86_encode_r32_rbpdisp32(0xF7, 7, disp)
#define IDIVQ_r64(r1)                x86_encode_r64_rm64(0xF7, 7, r1)

#define IMULL_imms_r32(imm,r1)       x86_encode_imms_rm32(0x6B,0x69, r1, imm, r1)
#define IMULL_r32(r1)                x86_encode_r32_rm32(0xF7, 5, r1)
#define IMULL_r32_r32(r1,r2)         x86_encode_r32_rm32(0x0FAF, r2, r1)
#define IMULL_rbpdisp(disp)          x86_encode_r32_rbpdisp32(0xF7, 5, disp)
#define IMULL_rbpdisp_r32(disp,r1)   x86_encode_r32_rbpdisp32(0x0FAF, r1, disp)
#define IMULL_rspdisp(disp)          x86_encode_r32_rspdisp32(0xF7, 5, disp)
#define IMULL_rspdisp_r32(disp,r1)   x86_encode_r32_rspdisp32(0x0FAF, r1, disp)
#define IMULQ_imms_r64(imm,r1)       x86_encode_imms_rm64(0x6B,0x69, r1, imm, r1)
#define IMULQ_r64_r64(r1,r2)         x86_encode_r64_rm64(0x0FAF, r2, r1)

#define LEAL_r32disp_r32(r1,disp,r2) x86_encode_r32_mem32(0x8D, r2, r1, -1, 0, disp)
#define LEAL_rbpdisp_r32(disp,r1)    x86_encode_r32_rbpdisp32(0x8D, r1, disp)
#define LEAL_sib_r32(ss,ii,bb,d,r1)  x86_encode_r32_mem32(0x8D, r1, bb, ii, ss, d)
#define LEAQ_r64disp_r64(r1,disp,r2) x86_encode_r64_mem64(0x8D, r2, r1, -1, 0, disp)
#define LEAQ_rbpdisp_r64(disp,r1)    x86_encode_r64_rbpdisp64(0x8D, r1, disp)
#define LEAP_rptrdisp_rptr(r1,d,r2)  x86_encode_rptr_memptr(0x8D, r2, r1, -1, 0, disp)
#define LEAP_rbpdisp_rptr(disp,r1)   x86_encode_rptr_memptr(0x8D, r1, REG_RBP, -1, 0, disp)
#define LEAP_sib_rptr(ss,ii,bb,d,r1) x86_encode_rptr_memptr(0x8D, r1, bb, ii, ss, d)

#define MOVB_r8_r8(r1,r2)            x86_encode_r32_rm32(0x88, r1, r2)
#define MOVL_imm32_r32(i32,r1)       x86_encode_opcode32(0xB8, r1); OP32(i32)
#define MOVL_imm32_rbpdisp(i,disp)   x86_encode_r32_rbpdisp32(0xC7,0,disp); OP32(i)
#define MOVL_imm32_rspdisp(i,disp)   x86_encode_r32_rspdisp32(0xC7,0,disp); OP32(i)
#define MOVL_moffptr_eax(p)          OP(0xA1); OPPTR(p)
#define MOVL_r32_r32(r1,r2)          x86_encode_r32_rm32(0x89, r1, r2)
#define MOVL_r32_r32disp(r1,r2,dsp)  x86_encode_r32_mem32disp32(0x89, r1, r2, dsp)
#define MOVL_r32_rbpdisp(r1,disp)    x86_encode_r32_rbpdisp32(0x89, r1, disp)
#define MOVL_r32_rspdisp(r1,disp)    x86_encode_r32_rspdisp32(0x89, r1, disp)
#define MOVL_r32_sib(r1,ss,ii,bb,d)  x86_encode_r32_mem32(0x89, r1, bb, ii, ss, d)
#define MOVL_r32disp_r32(r1,dsp,r2)  x86_encode_r32_mem32disp32(0x8B, r2, r1, dsp)
#define MOVL_rbpdisp_r32(disp,r1)    x86_encode_r32_rbpdisp32(0x8B, r1, disp)
#define MOVL_rspdisp_r32(disp,r1)    x86_encode_r32_rspdisp32(0x8B, r1, disp)
#define MOVL_sib_r32(ss,ii,bb,d,r1)  x86_encode_r32_mem32(0x8B, r1, bb, ii, ss, d)
#define MOVQ_imm64_r64(i64,r1)       x86_encode_opcode64(0xB8, r1); OP64(i64)
#define MOVQ_moffptr_rax(p)          OP(PREF_REXW); OP(0xA1); OPPTR(p)
#define MOVQ_r64_r64(r1,r2)          x86_encode_r64_rm64(0x89, r1, r2)
#define MOVQ_r64_rbpdisp(r1,disp)    x86_encode_r64_rbpdisp64(0x89, r1, disp)
#define MOVQ_r64_rspdisp(r1,disp)    x86_encode_r64_rspdisp64(0x89, r1, disp)
#define MOVQ_rbpdisp_r64(disp,r1)    x86_encode_r64_rbpdisp64(0x8B, r1, disp)
#define MOVQ_rspdisp_r64(disp,r1)    x86_encode_r64_rspdisp64(0x8B, r1, disp)
#define MOVP_immptr_rptr(p,r1)       x86_encode_opcodereg( PREF_PTR, 0xB8, r1); OPPTR(p)
#define MOVP_moffptr_rax(p)          if( sizeof(void*)==8 ) { OP(PREF_REXW); } OP(0xA1); OPPTR(p)
#define MOVP_rptr_rptr(r1,r2)        x86_encode_reg_rm(PREF_PTR, 0x89, r1, r2)
#define MOVP_sib_rptr(ss,ii,bb,d,r1) x86_encode_rptr_memptr(0x8B, r1, bb, ii, ss, d)

#define MOVSXL_r8_r32(r1,r2)         x86_encode_r32_rm32(0x0FBE, r2, r1)
#define MOVSXL_r16_r32(r1,r2)        x86_encode_r32_rm32(0x0FBF, r2, r1)
#define MOVSXL_rbpdisp8_r32(disp,r1) x86_encode_r32_rbpdisp32(0x0FBE, r1, disp) 
#define MOVSXL_rbpdisp16_r32(dsp,r1) x86_encode_r32_rbpdisp32(0x0FBF, r1, dsp) 
#define MOVSXQ_imm32_r64(i32,r1)     x86_encode_r64_rm64(0xC7, 0, r1); OP32(i32) /* Technically a MOV */
#define MOVSXQ_r8_r64(r1,r2)         x86_encode_r64_rm64(0x0FBE, r2, r1)
#define MOVSXQ_r16_r64(r1,r2)        x86_encode_r64_rm64(0x0FBF, r2, r1)
#define MOVSXQ_r32_r64(r1,r2)        x86_encode_r64_rm64(0x63, r2, r1)
#define MOVSXQ_rbpdisp32_r64(dsp,r1) x86_encode_r64_rbpdisp64(0x63, r1, dsp)

#define MOVZXL_r8_r32(r1,r2)         x86_encode_r32_rm32(0x0FB6, r2, r1)
#define MOVZXL_r16_r32(r1,r2)        x86_encode_r32_rm32(0x0FB7, r2, r1)
#define MOVZXL_rbpdisp8_r32(disp,r1) x86_encode_r32_rbpdisp32(0x0FB6, r1, disp)
#define MOVZXL_rbpdisp16_r32(dsp,r1) x86_encode_r32_rbpdisp32(0x0FB7, r1, dsp)

#define MULL_r32(r1)                 x86_encode_r32_rm32(0xF7, 4, r1)
#define MULL_rbpdisp(disp)           x86_encode_r32_rbpdisp32(0xF7,4,disp)
#define MULL_rspdisp(disp)           x86_encode_r32_rspdisp32(0xF7,4,disp)

#define NEGB_r8(r1)                  x86_encode_r32_rm32(0xF6, 3, r1)
#define NEGL_r32(r1)                 x86_encode_r32_rm32(0xF7, 3, r1)
#define NEGL_rbpdisp(r1)             x86_encode_r32_rbspdisp32(0xF7, 3, disp)
#define NEGQ_r64(r1)                 x86_encode_r64_rm64(0xF7, 3, r1)

#define NOTB_r8(r1)                  x86_encode_r32_rm32(0xF6, 2, r1)
#define NOTL_r32(r1)                 x86_encode_r32_rm32(0xF7, 2, r1)
#define NOTL_rbpdisp(r1)             x86_encode_r32_rbspdisp32(0xF7, 2, disp)
#define NOTQ_r64(r1)                 x86_encode_r64_rm64(0xF7, 2, r1)

#define ORB_imms_r8(imm,r1)          x86_encode_r32_rm32(0x80, 1, r1); OP(imm)
#define ORB_r8_r8(r1,r2)             x86_encode_r32_rm32(0x08, r1, r2)
#define ORL_imms_r32(imm,r1)         x86_encode_imms_rm32(0x83, 0x81, 1, imm, r1)
#define ORL_imms_rbpdisp(imm,disp)   x86_encode_imms_rbpdisp32(0x83,0x81,1,imm,disp)
#define ORL_r32_r32(r1,r2)           x86_encode_r32_rm32(0x09, r1, r2)
#define ORL_r32_rbpdisp(r1,disp)     x86_encode_r32_rbpdisp32(0x09, r1, disp)
#define ORL_rbpdisp_r32(disp,r1)     x86_encode_r32_rbpdisp32(0x0B, r1, disp)
#define ORQ_imms_r64(imm,r1)         x86_encode_imms_rm64(0x83, 0x81, 1, imm, r1)
#define ORQ_r64_r64(r1,r2)           x86_encode_r64_rm64(0x09, r1, r2)

#define POP_r32(r1)                  x86_encode_opcode32(0x58, r1)

#define PUSH_imm32(imm)              OP(0x68); OP32(imm)
#define PUSH_r32(r1)                 x86_encode_opcode32(0x50, r1)

#define RCLL_cl_r32(r1)              x86_encode_r32_rm32(0xD3,2,r1)
#define RCLL_imm_r32(imm,r1)         if( imm == 1 ) { x86_encode_r32_rm32(0xD1,2,r1); } else { x86_encode_r32_rm32(0xC1,2,r1); OP(imm); }
#define RCLQ_cl_r64(r1)              x86_encode_r64_rm64(0xD3,2,r1)
#define RCLQ_imm_r64(imm,r1)         if( imm == 1 ) { x86_encode_r64_rm64(0xD1,2,r1); } else { x86_encode_r64_rm64(0xC1,2,r1); OP(imm); }
#define RCRL_cl_r32(r1)              x86_encode_r32_rm32(0xD3,3,r1)
#define RCRL_imm_r32(imm,r1)         if( imm == 1 ) { x86_encode_r32_rm32(0xD1,3,r1); } else { x86_encode_r32_rm32(0xC1,3,r1); OP(imm); }
#define RCRQ_cl_r64(r1)              x86_encode_r64_rm64(0xD3,3,r1)
#define RCRQ_imm_r64(imm,r1)         if( imm == 1 ) { x86_encode_r64_rm64(0xD1,3,r1); } else { x86_encode_r64_rm64(0xC1,3,r1); OP(imm); }
#define ROLL_cl_r32(r1)              x86_encode_r32_rm32(0xD3,0,r1)
#define ROLL_imm_r32(imm,r1)         if( imm == 1 ) { x86_encode_r32_rm32(0xD1,0,r1); } else { x86_encode_r32_rm32(0xC1,0,r1); OP(imm); }
#define ROLQ_cl_r64(r1)              x86_encode_r64_rm64(0xD3,0,r1)
#define ROLQ_imm_r64(imm,r1)         if( imm == 1 ) { x86_encode_r64_rm64(0xD1,0,r1); } else { x86_encode_r64_rm64(0xC1,0,r1); OP(imm); }
#define RORL_cl_r32(r1)              x86_encode_r32_rm32(0xD3,1,r1)
#define RORL_imm_r32(imm,r1)         if( imm == 1 ) { x86_encode_r32_rm32(0xD1,1,r1); } else { x86_encode_r32_rm32(0xC1,1,r1); OP(imm); }
#define RORQ_cl_r64(r1)              x86_encode_r64_rm64(0xD3,1,r1)
#define RORQ_imm_r64(imm,r1)         if( imm == 1 ) { x86_encode_r64_rm64(0xD1,1,r1); } else { x86_encode_r64_rm64(0xC1,1,r1); OP(imm); }

#define SARL_cl_r32(r1)              x86_encode_r32_rm32(0xD3,7,r1)
#define SARL_imm_r32(imm,r1)         if( imm == 1 ) { x86_encode_r32_rm32(0xD1,7,r1); } else { x86_encode_r32_rm32(0xC1,7,r1); OP(imm); }
#define SARQ_cl_r64(r1)              x86_encode_r64_rm64(0xD3,7,r1)
#define SARQ_imm_r64(imm,r1)         if( imm == 1 ) { x86_encode_r64_rm64(0xD1,7,r1); } else { x86_encode_r64_rm64(0xC1,7,r1); OP(imm); }
#define SHLL_cl_r32(r1)              x86_encode_r32_rm32(0xD3,4,r1)
#define SHLL_imm_r32(imm,r1)         if( imm == 1 ) { x86_encode_r32_rm32(0xD1,4,r1); } else { x86_encode_r32_rm32(0xC1,4,r1); OP(imm); }
#define SHLQ_cl_r64(r1)              x86_encode_r64_rm64(0xD3,4,r1)
#define SHLQ_imm_r64(imm,r1)         if( imm == 1 ) { x86_encode_r64_rm64(0xD1,4,r1); } else { x86_encode_r64_rm64(0xC1,4,r1); OP(imm); }
#define SHRL_cl_r32(r1)              x86_encode_r32_rm32(0xD3,5,r1)
#define SHRL_imm_r32(imm,r1)         if( imm == 1 ) { x86_encode_r32_rm32(0xD1,5,r1); } else { x86_encode_r32_rm32(0xC1,5,r1); OP(imm); }
#define SHRQ_cl_r64(r1)              x86_encode_r64_rm64(0xD3,5,r1)
#define SHRQ_imm_r64(imm,r1)         if( imm == 1 ) { x86_encode_r64_rm64(0xD1,5,r1); } else { x86_encode_r64_rm64(0xC1,5,r1); OP(imm); }

#define SBBB_imms_r8(imm,r1)         x86_encode_r32_rm32(0x80, 3, r1); OP(imm)
#define SBBB_r8_r8(r1,r2)            x86_encode_r32_rm32(0x18, r1, r2)
#define SBBL_imms_r32(imm,r1)        x86_encode_imms_rm32(0x83, 0x81, 3, imm, r1)
#define SBBL_imms_rbpdisp(imm,disp)  x86_encode_imms_rbpdisp32(0x83,0x81,3,imm,disp)
#define SBBL_r32_r32(r1,r2)          x86_encode_r32_rm32(0x19, r1, r2)
#define SBBL_r32_rbpdisp(r1,disp)    x86_encode_r32_rbpdisp32(0x19, r1, disp)
#define SBBL_rbpdisp_r32(disp,r1)    x86_encode_r32_rbpdisp32(0x1B, r1, disp)
#define SBBQ_imms_r64(imm,r1)        x86_encode_imms_rm64(0x83, 0x81, 3, imm, r1)
#define SBBQ_r64_r64(r1,r2)          x86_encode_r64_rm64(0x19, r1, r2)

#define SETCCB_cc_r8(cc,r1)          x86_encode_r32_rm32(0x0F90+(cc), 0, r1)
#define SETCCB_cc_rbpdisp(cc,disp)   x86_encode_r32_rbpdisp32(0x0F90+(cc), 0, disp)

#define STC()                        OP(0xF9)
#define STD()                        OP(0xFD)

#define SUBB_imms_r8(imm,r1)         x86_encode_r32_rm32(0x80, 5, r1); OP(imm)
#define SUBB_r8_r8(r1,r2)            x86_encode_r32_rm32(0x28, r1, r2)
#define SUBL_imms_r32(imm,r1)        x86_encode_imms_rm32(0x83, 0x81, 5, imm, r1)
#define SUBL_imms_rbpdisp(imm,disp)  x86_encode_imms_rbpdisp32(0x83,0x81,5,imm,disp)
#define SUBL_r32_r32(r1,r2)          x86_encode_r32_rm32(0x29, r1, r2)
#define SUBL_r32_rbpdisp(r1,disp)    x86_encode_r32_rbpdisp32(0x29, r1, disp)
#define SUBL_rbpdisp_r32(disp,r1)    x86_encode_r32_rbpdisp32(0x2B, r1, disp)
#define SUBQ_imms_r64(imm,r1)        x86_encode_imms_rm64(0x83, 0x81, 5, imm, r1)
#define SUBQ_r64_r64(r1,r2)          x86_encode_r64_rm64(0x29, r1, r2)

#define TESTB_imms_r8(imm,r1)        x86_encode_r32_rm32(0xF6, 0, r1); OP(imm)
#define TESTB_r8_r8(r1,r2)           x86_encode_r32_rm32(0x84, r1, r2)
#define TESTL_imms_r32(imm,r1)       x86_encode_r32_rm32(0xF7, 0, r1); OP32(imm)
#define TESTL_imms_rbpdisp(imm,dsp)  x86_encode_r32_rbpdisp32(0xF7, 0, dsp); OP32(imm)
#define TESTL_r32_r32(r1,r2)         x86_encode_r32_rm32(0x85, r1, r2)
#define TESTL_r32_rbpdisp(r1,disp)   x86_encode_r32_rbpdisp32(0x85, r1, disp)
#define TESTL_rbpdisp_r32(disp,r1)   x86_encode_r32_rbpdisp32(0x85, r1, disp) /* Same OP */
#define TESTQ_imms_r64(imm,r1)       x86_encode_r64_rm64(0xF7, 0, r1); OP32(imm)
#define TESTQ_r64_r64(r1,r2)         x86_encode_r64_rm64(0x85, r1, r2)

#define XCHGB_r8_r8(r1,r2)           x86_encode_r32_rm32(0x86, r1, r2)
#define XCHGL_r32_r32(r1,r2)         x86_encode_r32_rm32(0x87, r1, r2)
#define XCHGQ_r64_r64(r1,r2)         x86_encode_r64_rm64(0x87, r1, r2)

#define XORB_imms_r8(imm,r1)         x86_encode_r32_rm32(0x80, 6, r1); OP(imm)
#define XORB_r8_r8(r1,r2)            x86_encode_r32_rm32(0x30, r1, r2)
#define XORL_imms_r32(imm,r1)        x86_encode_imms_rm32(0x83, 0x81, 6, imm, r1)
#define XORL_imms_rbpdisp(imm,disp)  x86_encode_imms_rbpdisp32(0x83,0x81,6,imm,disp)
#define XORL_r32_r32(r1,r2)          x86_encode_r32_rm32(0x31, r1, r2)
#define XORL_r32_rbpdisp(r1,disp)    x86_encode_r32_rbpdisp32(0x31, r1, disp)
#define XORL_rbpdisp_r32(disp,r1)    x86_encode_r32_rbpdisp32(0x33, r1, disp)
#define XORQ_imms_r64(imm,r1)         x86_encode_imms_rm64(0x83, 0x81, 6, imm, r1)
#define XORQ_r64_r64(r1,r2)           x86_encode_r64_rm64(0x31, r1, r2)

/* Control flow */
#define CALL_rel(rel)                OP(0xE8); OP32(rel)
#define CALL_imm32(ptr)              x86_encode_r32_mem32disp32(0xFF, 2, -1, ptr)
#define CALL_r32(r1)                 x86_encode_r32_rm32(0xFF, 2, r1)
#define CALL_r32disp(r1,disp)        x86_encode_r32_mem32disp32(0xFF, 2, r1, disp)

#define JCC_cc_rel8(cc,rel)          OP(0x70+(cc)); OP(rel)
#define JCC_cc_rel32(cc,rel)         OP(0x0F); OP(0x80+(cc)); OP32(rel)
#define JCC_cc_rel(cc,rel)           if( IS_INT8(rel) ) { JCC_cc_rel8(cc,(int8_t)rel); } else { JCC_cc_rel32(cc,rel); }

#define JMP_rel8(rel)                OP(0xEB); OP(rel)
#define JMP_rel32(rel)               OP(0xE9); OP32(rel)
#define JMP_rel(rel)                 if( IS_INT8(rel) ) { JMP_rel8((int8_t)rel); } else { JMP_rel32(rel); }
#define JMP_prerel(rel)              if( IS_INT8(((int32_t)rel)-2) ) { JMP_rel8(((int8_t)rel)-2); } else { JMP_rel32(((int32_t)rel)-5); }
#define JMP_r32(r1,disp)             x86_encode_r32_rm32(0xFF, 4, r1)
#define JMP_r32disp(r1,disp)         x86_encode_r32_mem32disp32(0xFF, 4, r1, disp)
#define RET()                        OP(0xC3)
#define RET_imm(imm)                 OP(0xC2); OP16(imm)


/* x87 Floating point instructions */
#define FABS_st0()                   OP(0xD9); OP(0xE1)
#define FADDP_st(st)                 OP(0xDE); OP(0xC0+(st))
#define FCHS_st0()                   OP(0xD9); OP(0xE0)
#define FCOMIP_st(st)                OP(0xDF); OP(0xF0+(st))
#define FDIVP_st(st)                 OP(0xDE); OP(0xF8+(st))
#define FILD_r32disp(r32, disp)      x86_encode_r32_mem32disp32(0xDB, 0, r32, disp)
#define FLD0_st0()                   OP(0xD9); OP(0xEE);
#define FLD1_st0()                   OP(0xD9); OP(0xE8);
#define FLDCW_r32disp(r32, disp)     x86_encode_r32_mem32disp32(0xD9, 5, r32, disp)
#define FMULP_st(st)                 OP(0xDE); OP(0xC8+(st))
#define FNSTCW_r32disp(r32, disp)    x86_encode_r32_mem32disp32(0xD9, 7, r32, disp)
#define FPOP_st()                    OP(0xDD); OP(0xC0); OP(0xD9); OP(0xF7)
#define FSUBP_st(st)                 OP(0xDE); OP(0xE8+(st))
#define FSQRT_st0()                  OP(0xD9); OP(0xFA)

#define FILD_rbpdisp(disp)           x86_encode_r32_rbpdisp32(0xDB, 0, disp)
#define FLDF_rbpdisp(disp)           x86_encode_r32_rbpdisp32(0xD9, 0, disp)
#define FLDD_rbpdisp(disp)           x86_encode_r32_rbpdisp32(0xDD, 0, disp)
#define FISTP_rbpdisp(disp)          x86_encode_r32_rbpdisp32(0xDB, 3, disp)
#define FSTPF_rbpdisp(disp)          x86_encode_r32_rbpdisp32(0xD9, 3, disp)
#define FSTPD_rbpdisp(disp)          x86_encode_r32_rbpdisp32(0xDD, 3, disp)


/* SSE Packed floating point instructions */
#define ADDPS_rbpdisp_xmm(disp,r1)   x86_encode_r32_rbpdisp32(0x0F58, r1, disp)
#define ADDPS_xmm_xmm(r1,r2)         x86_encode_r32_rm32(0x0F58, r2, r1)
#define ANDPS_rbpdisp_xmm(disp,r1)   x86_encode_r32_rbpdisp32(0x0F54, r1, disp)
#define ANDPS_xmm_xmm(r1,r2)         x86_encode_r32_rm32(0x0F54, r2, r1)
#define ANDNPS_rbpdisp_xmm(disp,r1)  x86_encode_r32_rbpdisp32(0x0F55, r1, disp)
#define ANDNPS_xmm_xmm(r1,r2)        x86_encode_r32_rm32(0x0F55, r2, r1)
#define CMPPS_cc_rbpdisp_xmm(cc,d,r) x86_encode_r32_rbpdisp32(0x0FC2, r, d); OP(cc)
#define CMPPS_cc_xmm_xmm(cc,r1,r2)   x86_encode_r32_rm32(0x0FC2, r2, r1); OP(cc)
#define DIVPS_rbpdisp_xmm(disp,r1)   x86_encode_r32_rbpdisp32(0x0F5E, r1, disp)
#define DIVPS_xmm_xmm(r1,r2)         x86_encode_r32_rm32(0x0F5E, r2, r1)
#define MAXPS_rbpdisp_xmm(disp,r1)   x86_encode_r32_rbpdisp32(0x0F5F, r1, disp)
#define MAXPS_xmm_xmm(r1,r2)         x86_encode_r32_rm32(0x0F5F, r2, r1)
#define MINPS_rbpdisp_xmm(disp,r1)   x86_encode_r32_rbpdisp32(0x0F5D, r1, disp)
#define MINPS_xmm_xmm(r1,r2)         x86_encode_r32_rm32(0x0F5D, r2, r1)
#define MOV_xmm_xmm(r1,r2)           x86_encode_r32_rm32(0x0F28, r2, r1)
#define MOVAPS_rbpdisp_xmm(disp,r1)  x86_encode_r32_rbpdisp32(0x0F28, r1, disp)
#define MOVAPS_xmm_rbpdisp(r1,disp)  x86_encode_r32_rbpdisp32(0x0F29, r1, disp)
#define MOVHLPS_xmm_xmm(r1,r2)       x86_encode_r32_rm32(0x0F12, r2, r1)
#define MOVHPS_rbpdisp_xmm(disp,r1)  x86_encode_r32_rbpdisp32(0x0F16, r1, disp)
#define MOVHPS_xmm_rbpdisp(r1,disp)  x86_encode_r32_rbpdisp32(0x0F17, r1, disp)
#define MOVLHPS_xmm_xmm(r1,r2)       x86_encode_r32_rm32(0x0F16, r2, r1)
#define MOVLPS_rbpdisp_xmm(disp,r1)  x86_encode_r32_rbpdisp32(0x0F12, r1, disp)
#define MOVLPS_xmm_rbpdisp(r1,disp)  x86_encode_r32_rbpdisp32(0x0F13, r1, disp)
#define MOVUPS_rbpdisp_xmm(disp,r1)  x86_encode_r32_rbpdisp32(0x0F10, r1, disp)
#define MOVUPS_xmm_rbpdisp(disp,r1)  x86_encode_r32_rbpdisp32(0x0F11, r1, disp)
#define MULPS_xmm_xmm(r1,r2)         x86_encode_r32_rm32(0x0F59, r2, r1)
#define MULPS_rbpdisp_xmm(disp,r1)   x86_encode_r32_rbpdisp32(0xF59, r1, disp)
#define ORPS_rbpdisp_xmm(disp,r1)    x86_encode_r32_rbpdisp32(0x0F56, r1, disp)
#define ORPS_xmm_xmm(r1,r2)          x86_encode_r32_rm32(0x0F56, r2, r1)
#define RCPPS_rbpdisp_xmm(disp,r1)   x86_encode_r32_rbpdisp32(0xF53, r1, disp)
#define RCPPS_xmm_xmm(r1,r2)         x86_encode_r32_rm32(0x0F53, r2, r1)
#define RSQRTPS_rbpdisp_xmm(disp,r1) x86_encode_r32_rbpdisp32(0x0F52, r1, disp)
#define RSQRTPS_xmm_xmm(r1,r2)       x86_encode_r32_rm32(0x0F52, r2, r1)
#define SHUFPS_rbpdisp_xmm(disp,r1)  x86_encode_r32_rbpdisp32(0x0FC6, r1, disp)
#define SHUFPS_xmm_xmm(r1,r2)        x86_encode_r32_rm32(0x0FC6, r2, r1)
#define SQRTPS_rbpdisp_xmm(disp,r1)  x86_encode_r32_rbpdisp32(0x0F51, r1, disp)
#define SQRTPS_xmm_xmm(r1,r2)        x86_encode_r32_rm32(0x0F51, r2, r1)
#define SUBPS_rbpdisp_xmm(disp,r1)   x86_encode_r32_rbpdisp32(0x0F5C, r1, disp)
#define SUBPS_xmm_xmm(r1,r2)         x86_encode_r32_rm32(0x0F5C, r2, r1)
#define UNPCKHPS_rbpdisp_xmm(dsp,r1) x86_encode_r32_rbpdisp32(0x0F15, r1, disp)
#define UNPCKHPS_xmm_xmm(r1,r2)      x86_encode_r32_rm32(0x0F15, r2, r1)
#define UNPCKLPS_rbpdisp_xmm(dsp,r1) x86_encode_r32_rbpdisp32(0x0F14, r1, disp)
#define UNPCKLPS_xmm_xmm(r1,r2)      x86_encode_r32_rm32(0x0F14, r2, r1)
#define XORPS_rbpdisp_xmm(disp,r1)   x86_encode_r32_rbpdisp32(0x0F57, r1, disp)
#define XORPS_xmm_xmm(r1,r2)         x86_encode_r32_rm32(0x0F57, r2, r1)

/* SSE Scalar floating point instructions */
#define ADDSS_rbpdisp_xmm(disp,r1)   OP(0xF3); x86_encode_r32_rbpdisp32(0x0F58, r1, disp)
#define ADDSS_xmm_xmm(r1,r2)         OP(0xF3); x86_encode_r32_rm32(0x0F58, r2, r1)
#define CMPSS_cc_rbpdisp_xmm(cc,d,r) OP(0xF3); x86_encode_r32_rbpdisp32(0x0FC2, r, d); OP(cc)
#define CMPSS_cc_xmm_xmm(cc,r1,r2)   OP(0xF3); x86_encode_r32_rm32(0x0FC2, r2, r1); OP(cc)
#define COMISS_rbpdisp_xmm(disp,r1)  x86_encode_r32_rbpdisp32(0x0F2F, r1, disp)
#define COMISS_xmm_xmm(r1,r2)        x86_encode_r32_rm32(0x0F2F, r2, r1)
#define DIVSS_rbpdisp_xmm(disp,r1)   OP(0xF3); x86_encode_r32_rbpdisp32(0x0F5E, r1, disp)
#define DIVSS_xmm_xmm(r1,r2)         OP(0xF3); x86_encode_r32_rm32(0x0F5E, r2, r1)
#define MAXSS_rbpdisp_xmm(disp,r1)   OP(0xF3); x86_encode_r32_rbpdisp32(0x0F5F, r1, disp)
#define MAXSS_xmm_xmm(r1,r2)         OP(0xF3); x86_encode_r32_rm32(0x0F5F, r2, r1)
#define MINSS_rbpdisp_xmm(disp,r1)   OP(0xF3); x86_encode_r32_rbpdisp32(0x0F5D, r1, disp)
#define MINSS_xmm_xmm(r1,r2)         OP(0xF3); x86_encode_r32_rm32(0x0F5D, r2, r1)
#define MOVSS_rbpdisp_xmm(disp,r1)   OP(0xF3); x86_encode_r32_rbpdisp32(0x0F10, r1, disp)
#define MOVSS_xmm_rbpdisp(r1,disp)   OP(0xF3); x86_encode_r32_rbpdisp32(0x0F11, r1, disp)
#define MOVSS_xmm_xmm(r1,r2)         OP(0xF3); x86_encode_r32_rm32(0x0F10, r2, r1)
#define MULSS_rbpdisp_xmm(disp,r1)   OP(0xF3); x86_encode_r32_rbpdisp32(0xF59, r1, disp)
#define MULSS_xmm_xmm(r1,r2)         OP(0xF3); x86_encode_r32_rm32(0x0F59, r2, r1)
#define RCPSS_rbpdisp_xmm(disp,r1)   OP(0xF3); x86_encode_r32_rbpdisp32(0xF53, r1, disp)
#define RCPSS_xmm_xmm(r1,r2)         OP(0xF3); x86_encode_r32_rm32(0x0F53, r2, r1)
#define RSQRTSS_rbpdisp_xmm(disp,r1) OP(0xF3); x86_encode_r32_rbpdisp32(0x0F52, r1, disp)
#define RSQRTSS_xmm_xmm(r1,r2)       OP(0xF3); x86_encode_r32_rm32(0x0F52, r2, r1)
#define SQRTSS_rbpdisp_xmm(disp,r1)  OP(0xF3); x86_encode_r32_rbpdisp32(0x0F51, r1, disp)
#define SQRTSS_xmm_xmm(r1,r2)        OP(0xF3); x86_encode_r32_rm32(0x0F51, r2, r1)
#define SUBSS_rbpdisp_xmm(disp,r1)   OP(0xF3); x86_encode_r32_rbpdisp32(0x0F5C, r1, disp)
#define SUBSS_xmm_xmm(r1,r2)         OP(0xF3); x86_encode_r32_rm32(0x0F5C, r2, r1)
#define UCOMISS_rbpdisp_xmm(dsp,r1)  x86_encode_r32_rbpdisp32(0x0F2E, r1, dsp)
#define UCOMISS_xmm_xmm(r1,r2)       x86_encode_r32_rm32(0x0F2E, r2, r1)

/* SSE2 Packed floating point instructions */
#define ADDPD_rbpdisp_xmm(disp,r1)   OP(0x66); x86_encode_r32_rbpdisp32(0x0F58, r1, disp)
#define ADDPD_xmm_xmm(r1,r2)         OP(0x66); x86_encode_r32_rm32(0x0F58, r2, r1)
#define ANDPD_rbpdisp_xmm(disp,r1)   OP(0x66); x86_encode_r32_rbpdisp32(0x0F54, r1, disp)
#define ANDPD_xmm_xmm(r1,r2)         OP(0x66); x86_encode_r32_rm32(0x0F54, r2, r1)
#define ANDNPD_rbpdisp_xmm(disp,r1)  OP(0x66); x86_encode_r32_rbpdisp32(0x0F55, r1, disp)
#define ANDNPD_xmm_xmm(r1,r2)        OP(0x66); x86_encode_r32_rm32(0x0F55, r2, r1)
#define CMPPD_cc_rbpdisp_xmm(cc,d,r) OP(0x66); x86_encode_r32_rbpdisp32(0x0FC2, r, d); OP(cc)
#define CMPPD_cc_xmm_xmm(cc,r1,r2)   OP(0x66); x86_encode_r32_rm32(0x0FC2, r2, r1); OP(cc)
#define CVTPD2PS_rbpdisp_xmm(dsp,r1) OP(0x66); x86_encode_r32_rbpdisp32(0x0F5A, r1, disp)
#define CVTPD2PS_xmm_xmm(r1,r2)      OP(0x66); x86_encode_r32_rm32(0x0F5A, r2, r1)
#define CVTPS2PD_rbpdisp_xmm(dsp,r1) x86_encode_r32_rbpdisp32(0x0F5A, r1, disp)
#define CVTPS2PD_xmm_xmm(r1,r2)      x86_encode_r32_rm32(0x0F5A, r2, r1)
#define DIVPD_rbpdisp_xmm(disp,r1)   OP(0x66); x86_encode_r32_rbpdisp32(0x0F5E, r1, disp)
#define DIVPD_xmm_xmm(r1,r2)         OP(0x66); x86_encode_r32_rm32(0x0F5E, r2, r1)
#define MAXPD_rbpdisp_xmm(disp,r1)   OP(0x66); x86_encode_r32_rbpdisp32(0x0F5F, r1, disp)
#define MAXPD_xmm_xmm(r1,r2)         OP(0x66); x86_encode_r32_rm32(0x0F5F, r2, r1)
#define MINPD_rbpdisp_xmm(disp,r1)   OP(0x66); x86_encode_r32_rbpdisp32(0x0F5D, r1, disp)
#define MINPD_xmm_xmm(r1,r2)         OP(0x66); x86_encode_r32_rm32(0x0F5D, r2, r1)
#define MOVHPD_rbpdisp_xmm(disp,r1)  OP(0x66); x86_encode_r32_rbpdisp32(0x0F16, r1, disp)
#define MOVHPD_xmm_rbpdisp(r1,disp)  OP(0x66); x86_encode_r32_rbpdisp32(0x0F17, r1, disp)
#define MOVLPD_rbpdisp_xmm(disp,r1)  OP(0x66); x86_encode_r32_rbpdisp32(0x0F12, r1, disp)
#define MOVLPD_xmm_rbpdisp(r1,disp)  OP(0x66); x86_encode_r32_rbpdisp32(0x0F13, r1, disp)
#define MULPD_rbpdisp_xmm(disp,r1)   OP(0x66); x86_encode_r32_rbpdisp32(0xF59, r1, disp)
#define MULPD_xmm_xmm(r1,r2)         OP(0x66); x86_encode_r32_rm32(0x0F59, r2, r1)
#define ORPD_rbpdisp_xmm(disp,r1)    OP(0x66); x86_encode_r32_rbpdisp32(0x0F56, r1, disp)
#define ORPD_xmm_xmm(r1,r2)          OP(0x66); x86_encode_r32_rm32(0x0F56, r2, r1)
#define SHUFPD_rbpdisp_xmm(disp,r1)  OP(0x66); x86_encode_r32_rbpdisp32(0x0FC6, r1, disp)
#define SHUFPD_xmm_xmm(r1,r2)        OP(0x66); x86_encode_r32_rm32(0x0FC6, r2, r1)
#define SUBPD_rbpdisp_xmm(disp,r1)   OP(0x66); x86_encode_r32_rbpdisp32(0x0F5C, r1, disp)
#define SUBPD_xmm_xmm(r1,r2)         OP(0x66); x86_encode_r32_rm32(0x0F5C, r2, r1)
#define UNPCKHPD_rbpdisp_xmm(dsp,r1) OP(0x66); x86_encode_r32_rbpdisp32(0x0F15, r1, disp)
#define UNPCKHPD_xmm_xmm(r1,r2)      OP(0x66); x86_encode_r32_rm32(0x0F15, r2, r1)
#define UNPCKLPD_rbpdisp_xmm(dsp,r1) OP(0x66); x86_encode_r32_rbpdisp32(0x0F14, r1, disp)
#define UNPCKLPD_xmm_xmm(r1,r2)      OP(0x66); x86_encode_r32_rm32(0x0F14, r2, r1)
#define XORPD_rbpdisp_xmm(disp,r1)   OP(0x66); x86_encode_r32_rbpdisp32(0x0F57, r1, disp)
#define XORPD_xmm_xmm(r1,r2)         OP(0x66); x86_encode_r32_rm32(0x0F57, r2, r1)


/* SSE2 Scalar floating point instructions */
#define ADDSD_rbpdisp_xmm(disp,r1)   OP(0xF2); x86_encode_r32_rbpdisp32(0x0F58, r1, disp)
#define ADDSD_xmm_xmm(r1,r2)         OP(0xF2); x86_encode_r32_rm32(0x0F58, r2, r1)
#define CMPSD_cc_rbpdisp_xmm(cc,d,r) OP(0xF2); x86_encode_r32_rbpdisp32(0x0FC2, r, d); OP(cc)
#define CMPSD_cc_xmm_xmm(cc,r1,r2)   OP(0xF2); x86_encode_r32_rm32(0x0FC2, r2, r1); OP(cc)
#define COMISD_rbpdisp_xmm(disp,r1)  OP(0x66); x86_encode_r32_rbpdisp32(0x0F2F, r1, disp)
#define COMISD_xmm_xmm(r1,r2)        OP(0x66); x86_encode_r32_rm32(0x0F2F, r2, r1)
#define DIVSD_rbpdisp_xmm(disp,r1)   OP(0xF2); x86_encode_r32_rbpdisp32(0x0F5E, r1, disp)
#define DIVSD_xmm_xmm(r1,r2)         OP(0xF2); x86_encode_r32_rm32(0x0F5E, r2, r1)
#define MAXSD_rbpdisp_xmm(disp,r1)   OP(0xF2); x86_encode_r32_rbpdisp32(0x0F5F, r1, disp)
#define MAXSD_xmm_xmm(r1,r2)         OP(0xF2); x86_encode_r32_rm32(0x0F5F, r2, r1)
#define MINSD_rbpdisp_xmm(disp,r1)   OP(0xF2); x86_encode_r32_rbpdisp32(0x0F5D, r1, disp)
#define MINSD_xmm_xmm(r1,r2)         OP(0xF2); x86_encode_r32_rm32(0x0F5D, r2, r1)
#define MOVSD_rbpdisp_xmm(disp,r1)   OP(0xF2); x86_encode_r32_rbpdisp32(0x0F10, r1, disp)
#define MOVSD_xmm_rbpdisp(r1,disp)   OP(0xF2); x86_encode_r32_rbpdisp32(0x0F11, r1, disp)
#define MOVSD_xmm_xmm(r1,r2)         OP(0xF2); x86_encode_r32_rm32(0x0F10, r2, r1)
#define MULSD_rbpdisp_xmm(disp,r1)   OP(0xF2); x86_encode_r32_rbpdisp32(0xF59, r1, disp)
#define MULSD_xmm_xmm(r1,r2)         OP(0xF2); x86_encode_r32_rm32(0x0F59, r2, r1)
#define SQRTSD_rbpdisp_xmm(disp,r1)  OP(0xF2); x86_encode_r32_rbpdisp32(0x0F51, r1, disp)
#define SQRTSD_xmm_xmm(r1,r2)        OP(0xF2); x86_encode_r32_rm32(0x0F51, r2, r1)
#define SUBSD_rbpdisp_xmm(disp,r1)   OP(0xF2); x86_encode_r32_rbpdisp32(0x0F5C, r1, disp)
#define SUBSD_xmm_xmm(r1,r2)         OP(0xF2); x86_encode_r32_rm32(0x0F5C, r2, r1)
#define UCOMISD_rbpdisp_xmm(dsp,r1)  OP(0x66); x86_encode_r32_rbpdisp32(0x0F2E, r1, dsp)
#define UCOMISD_xmm_xmm(r1,r2)       OP(0x66); x86_encode_r32_rm32(0x0F2E, r2, r1)

/* SSE3 floating point instructions */
#define ADDSUBPD_rbpdisp_xmm(dsp,r1) OP(0x66); x86_encode_r32_rbpdisp32(0x0FD0, r1, dsp)
#define ADDSUBPD_xmm_xmm(r1,r2)      OP(0x66); x86_encode_r32_rm32(0x0FD0, r2, r1)
#define ADDSUBPS_rbpdisp_xmm(dsp,r1) OP(0xF2); x86_encode_r32_rbpdisp32(0x0FD0, r1, dsp)
#define ADDSUBPS_xmm_xmm(r1,r2)      OP(0xF2); x86_encode_r32_rm32(0x0FD0, r2, r1)
#define HADDPD_rbpdisp_xmm(dsp,r1)   OP(0x66); x86_encode_r32_rbpdisp32(0x0F7C, r1, dsp)
#define HADDPD_xmm_xmm(r1,r2)        OP(0x66); x86_encode_r32_rm32(0x0F7C, r2, r1)
#define HADDPS_rbpdisp_xmm(dsp,r1)   OP(0xF2); x86_encode_r32_rbpdisp32(0x0F7C, r1, dsp)
#define HADDPS_xmm_xmm(r1,r2)        OP(0xF2); x86_encode_r32_rm32(0x0F7C, r2, r1)
#define HSUBPD_rbpdisp_xmm(dsp,r1)   OP(0x66); x86_encode_r32_rbpdisp32(0x0F7D, r1, dsp)
#define HSUBPD_xmm_xmm(r1,r2)        OP(0x66); x86_encode_r32_rm32(0x0F7D, r2, r1)
#define HSUBPS_rbpdisp_xmm(dsp,r1)   OP(0xF2); x86_encode_r32_rbpdisp32(0x0F7D, r1, dsp)
#define HSUBPS_xmm_xmm(r1,r2)        OP(0xF2); x86_encode_r32_rm32(0x0F7D, r2, r1)
#define MOVSHDUP_rbpdisp_xmm(dsp,r1) OP(0xF3); x86_encode_r32_rbpdisp32(0x0F16, r1, dsp)
#define MOVSHDUP_xmm_xmm(r1,r2)      OP(0xF3); x86_encode_r32_rm32(0x0F16, r2, r1)
#define MOVSLDUP_rbpdisp_xmm(dsp,r1) OP(0xF3); x86_encode_r32_rbpdisp32(0x0F12, r1, dsp)
#define MOVSLDUP_xmm_xmm(r1,r2)      OP(0xF3); x86_encode_r32_rm32(0x0F12, r2, r1)

/************************ Import calling conventions *************************/
#if SIZEOF_VOID_P == 8
#include "xlat/x86/amd64abi.h"
#else /* 32-bit system */
#include "xlat/x86/ia32abi.h"
#endif

#endif /* !lxdream_x86op_H */
