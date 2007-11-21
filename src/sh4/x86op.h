/**
 * $Id: x86op.h,v 1.10 2007-09-19 09:15:18 nkeynes Exp $
 * 
 * Definitions of x86 opcodes for use by the translator.
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

#ifndef __lxdream_x86op_H
#define __lxdream_x86op_H

#define R_NONE -1
#define R_EAX 0
#define R_ECX 1
#define R_EDX 2
#define R_EBX 3
#define R_ESP 4
#define R_EBP 5
#define R_ESI 6 
#define R_EDI 7 

#define R_AL 0
#define R_CL 1
#define R_DL 2
#define R_BL 3
#define R_AH 4
#define R_CH 5
#define R_DH 6
#define R_BH 7

#ifdef DEBUG_JUMPS
#define MARK_JMP(n,x) uint8_t *_mark_jmp_##x = xlat_output + n
#define JMP_TARGET(x) assert( _mark_jmp_##x == xlat_output )
#else
#define MARK_JMP(n, x)
#define JMP_TARGET(x)
#endif





#define OP(x) *xlat_output++ = (x)
#define OP32(x) *((uint32_t *)xlat_output) = (x); xlat_output+=4
#define OP64(x) *((uint64_t *)xlat_output) = (x); xlat_output+=8
#if SH4_TRANSLATOR == TARGET_X86_64
#define OPPTR(x) OP64((uint64_t)(x))
#define STACK_ALIGN 16
#define POP_r32(r1)           OP(0x58 + r1); sh4_x86.stack_posn -= 8;
#define PUSH_r32(r1)          OP(0x50 + r1); sh4_x86.stack_posn += 8;
#define PUSH_imm32(imm)       OP(0x68); OP32(imm); sh4_x86.stack_posn += 4;
#define PUSH_imm64(imm)       REXW(); OP(0x68); OP64(imm); sh4_x86.stack_posn += 8;
#else
#define OPPTR(x) OP32((uint32_t)(x))
#ifdef APPLE_BUILD
#define STACK_ALIGN 16
#define POP_r32(r1)           OP(0x58 + r1); sh4_x86.stack_posn -= 4;
#define PUSH_r32(r1)          OP(0x50 + r1); sh4_x86.stack_posn += 4;
#define PUSH_imm32(imm)       OP(0x68); OP32(imm); sh4_x86.stack_posn += 4;
#else
#define POP_r32(r1)           OP(0x58 + r1)
#define PUSH_r32(r1)          OP(0x50 + r1)
#define PUSH_imm32(imm)       OP(0x68); OP32(imm)
#endif
#endif

#ifdef STACK_ALIGN
#else
#define POP_r32(r1)           OP(0x58 + r1)
#define PUSH_r32(r1)          OP(0x50 + r1)
#endif


/* Offset of a reg relative to the sh4r structure */
#define REG_OFFSET(reg)  (((char *)&sh4r.reg) - ((char *)&sh4r))

#define R_T   REG_OFFSET(t)
#define R_Q   REG_OFFSET(q)
#define R_S   REG_OFFSET(s)
#define R_M   REG_OFFSET(m)
#define R_SR  REG_OFFSET(sr)
#define R_GBR REG_OFFSET(gbr)
#define R_SSR REG_OFFSET(ssr)
#define R_SPC REG_OFFSET(spc)
#define R_VBR REG_OFFSET(vbr)
#define R_MACH REG_OFFSET(mac)+4
#define R_MACL REG_OFFSET(mac)
#define R_PR REG_OFFSET(pr)
#define R_SGR REG_OFFSET(sgr)
#define R_FPUL REG_OFFSET(fpul)
#define R_FPSCR REG_OFFSET(fpscr)
#define R_DBR REG_OFFSET(dbr)

/**************** Basic X86 operations *********************/
/* Note: operands follow SH4 convention (source, dest) rather than x86 
 * conventions (dest, source)
 */

/* Two-reg modrm form - first arg is the r32 reg, second arg is the r/m32 reg */
#define MODRM_r32_rm32(r1,r2) OP(0xC0 | (r1<<3) | r2)
#define MODRM_rm32_r32(r1,r2) OP(0xC0 | (r2<<3) | r1)

/* ebp+disp8 modrm form */
#define MODRM_r32_ebp8(r1,disp) OP(0x45 | (r1<<3)); OP(disp)

/* ebp+disp32 modrm form */
#define MODRM_r32_ebp32(r1,disp) OP(0x85 | (r1<<3)); OP32(disp)

#define MODRM_r32_sh4r(r1,disp) if(disp>127){ MODRM_r32_ebp32(r1,disp);}else{ MODRM_r32_ebp8(r1,(unsigned char)disp); }

#define REXW() OP(0x48)

/* Major opcodes */
#define ADD_sh4r_r32(disp,r1) OP(0x03); MODRM_r32_sh4r(r1,disp)
#define ADD_r32_sh4r(r1,disp) OP(0x01); MODRM_r32_sh4r(r1,disp)
#define ADD_r32_r32(r1,r2) OP(0x03); MODRM_rm32_r32(r1,r2)
#define ADD_imm8s_r32(imm,r1) OP(0x83); MODRM_rm32_r32(r1, 0); OP(imm)
#define ADD_imm8s_sh4r(imm,disp) OP(0x83); MODRM_r32_sh4r(0,disp); OP(imm)
#define ADD_imm32_r32(imm32,r1) OP(0x81); MODRM_rm32_r32(r1,0); OP32(imm32)
#define ADC_r32_r32(r1,r2)    OP(0x13); MODRM_rm32_r32(r1,r2)
#define ADC_sh4r_r32(disp,r1) OP(0x13); MODRM_r32_sh4r(r1,disp)
#define ADC_r32_sh4r(r1,disp) OP(0x11); MODRM_r32_sh4r(r1,disp)
#define AND_r32_r32(r1,r2)    OP(0x23); MODRM_rm32_r32(r1,r2)
#define AND_imm8_r8(imm8, r1) OP(0x80); MODRM_rm32_r32(r1,4); OP(imm8)
#define AND_imm8s_r32(imm8,r1) OP(0x83); MODRM_rm32_r32(r1,4); OP(imm8)
#define AND_imm32_r32(imm,r1) OP(0x81); MODRM_rm32_r32(r1,4); OP32(imm)
#define CALL_r32(r1)          OP(0xFF); MODRM_rm32_r32(r1,2)
#define CLC()                 OP(0xF8)
#define CMC()                 OP(0xF5)
#define CMP_sh4r_r32(disp,r1)  OP(0x3B); MODRM_r32_sh4r(r1,disp)
#define CMP_r32_r32(r1,r2)    OP(0x3B); MODRM_rm32_r32(r1,r2)
#define CMP_imm32_r32(imm32, r1) OP(0x81); MODRM_rm32_r32(r1,7); OP32(imm32)
#define CMP_imm8s_r32(imm,r1) OP(0x83); MODRM_rm32_r32(r1,7); OP(imm)
#define CMP_imm8s_sh4r(imm,disp) OP(0x83); MODRM_r32_sh4r(7,disp) OP(imm)
#define DEC_r32(r1)           OP(0x48+r1)
#define IMUL_r32(r1)          OP(0xF7); MODRM_rm32_r32(r1,5)
#define INC_r32(r1)           OP(0x40+r1)
#define JMP_rel8(rel, label)  OP(0xEB); OP(rel); MARK_JMP(rel,label)
#define MOV_r32_r32(r1,r2)    OP(0x89); MODRM_r32_rm32(r1,r2)
#define MOV_r32_sh4r(r1,disp) OP(0x89); MODRM_r32_sh4r(r1,disp)
#define MOV_moff32_EAX(off)   OP(0xA1); OPPTR(off)
#define MOV_sh4r_r32(disp, r1)  OP(0x8B); MODRM_r32_sh4r(r1,disp)
#define MOV_r32ind_r32(r1,r2) OP(0x8B); OP(0 + (r2<<3) + r1 )
#define MOVSX_r8_r32(r1,r2)   OP(0x0F); OP(0xBE); MODRM_rm32_r32(r1,r2)
#define MOVSX_r16_r32(r1,r2)  OP(0x0F); OP(0xBF); MODRM_rm32_r32(r1,r2)
#define MOVZX_r8_r32(r1,r2)   OP(0x0F); OP(0xB6); MODRM_rm32_r32(r1,r2)
#define MOVZX_r16_r32(r1,r2)  OP(0x0F); OP(0xB7); MODRM_rm32_r32(r1,r2)
#define MUL_r32(r1)           OP(0xF7); MODRM_rm32_r32(r1,4)
#define NEG_r32(r1)           OP(0xF7); MODRM_rm32_r32(r1,3)
#define NOT_r32(r1)           OP(0xF7); MODRM_rm32_r32(r1,2)
#define OR_r32_r32(r1,r2)     OP(0x0B); MODRM_rm32_r32(r1,r2)
#define OR_imm8_r8(imm,r1)    OP(0x80); MODRM_rm32_r32(r1,1); OP(imm)
#define OR_imm32_r32(imm,r1)  OP(0x81); MODRM_rm32_r32(r1,1); OP32(imm)
#define OR_sh4r_r32(disp,r1)  OP(0x0B); MODRM_r32_sh4r(r1,disp)
#define RCL1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,2)
#define RCR1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,3)
#define RET()                 OP(0xC3)
#define ROL1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,0)
#define ROR1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,1)
#define SAR1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,7)
#define SAR_imm8_r32(imm,r1)  OP(0xC1); MODRM_rm32_r32(r1,7); OP(imm)
#define SAR_r32_CL(r1)        OP(0xD3); MODRM_rm32_r32(r1,7)
#define SBB_r32_r32(r1,r2)    OP(0x1B); MODRM_rm32_r32(r1,r2)
#define SHL1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,4)
#define SHL_r32_CL(r1)        OP(0xD3); MODRM_rm32_r32(r1,4)
#define SHL_imm8_r32(imm,r1)  OP(0xC1); MODRM_rm32_r32(r1,4); OP(imm)
#define SHR1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,5)
#define SHR_r32_CL(r1)        OP(0xD3); MODRM_rm32_r32(r1,5)
#define SHR_imm8_r32(imm,r1)  OP(0xC1); MODRM_rm32_r32(r1,5); OP(imm)
#define STC()                 OP(0xF9)
#define SUB_r32_r32(r1,r2)    OP(0x2B); MODRM_rm32_r32(r1,r2)
#define SUB_sh4r_r32(disp,r1)  OP(0x2B); MODRM_r32_sh4r(r1, disp)
#define SUB_imm8s_r32(imm,r1) ADD_imm8s_r32(-(imm),r1)
#define TEST_r8_r8(r1,r2)     OP(0x84); MODRM_r32_rm32(r1,r2)
#define TEST_r32_r32(r1,r2)   OP(0x85); MODRM_rm32_r32(r1,r2)
#define TEST_imm8_r8(imm8,r1) OP(0xF6); MODRM_rm32_r32(r1,0); OP(imm8)
#define TEST_imm32_r32(imm,r1) OP(0xF7); MODRM_rm32_r32(r1,0); OP32(imm)
#define XCHG_r8_r8(r1,r2)     OP(0x86); MODRM_rm32_r32(r1,r2)
#define XOR_r8_r8(r1,r2)      OP(0x32); MODRM_rm32_r32(r1,r2)
#define XOR_imm8s_r32(imm,r1)   OP(0x83); MODRM_rm32_r32(r1,6); OP(imm)
#define XOR_r32_r32(r1,r2)    OP(0x33); MODRM_rm32_r32(r1,r2)
#define XOR_sh4r_r32(disp,r1)    OP(0x33); MODRM_r32_sh4r(r1,disp)
#define XOR_imm32_r32(imm,r1) OP(0x81); MODRM_rm32_r32(r1,6); OP32(imm)


/* Floating point ops */
#define FABS_st0() OP(0xD9); OP(0xE1)
#define FADDP_st(st) OP(0xDE); OP(0xC0+st)
#define FCHS_st0() OP(0xD9); OP(0xE0)
#define FCOMIP_st(st) OP(0xDF); OP(0xF0+st)
#define FDIVP_st(st) OP(0xDE); OP(0xF8+st)
#define FILD_sh4r(disp) OP(0xDB); MODRM_r32_sh4r(0, disp)
#define FILD_r32ind(r32) OP(0xDB); OP(0x00+r32)
#define FISTP_sh4r(disp) OP(0xDB); MODRM_r32_sh4r(3, disp)
#define FLD0_st0() OP(0xD9); OP(0xEE);
#define FLD1_st0() OP(0xD9); OP(0xE8);
#define FLDCW_r32ind(r32) OP(0xD9); OP(0x28+r32)
#define FMULP_st(st) OP(0xDE); OP(0xC8+st)
#define FNSTCW_r32ind(r32) OP(0xD9); OP(0x38+r32)
#define FPOP_st()  OP(0xDD); OP(0xC0); OP(0xD9); OP(0xF7)
#define FSUBP_st(st) OP(0xDE); OP(0xE8+st)
#define FSQRT_st0() OP(0xD9); OP(0xFA)

/* Conditional branches */
#define JE_rel8(rel,label)   OP(0x74); OP(rel); MARK_JMP(rel,label)
#define JA_rel8(rel,label)   OP(0x77); OP(rel); MARK_JMP(rel,label)
#define JAE_rel8(rel,label)  OP(0x73); OP(rel); MARK_JMP(rel,label)
#define JG_rel8(rel,label)   OP(0x7F); OP(rel); MARK_JMP(rel,label)
#define JGE_rel8(rel,label)  OP(0x7D); OP(rel); MARK_JMP(rel,label)
#define JC_rel8(rel,label)   OP(0x72); OP(rel); MARK_JMP(rel,label)
#define JO_rel8(rel,label)   OP(0x70); OP(rel); MARK_JMP(rel,label)
#define JNE_rel8(rel,label)  OP(0x75); OP(rel); MARK_JMP(rel,label)
#define JNA_rel8(rel,label)  OP(0x76); OP(rel); MARK_JMP(rel,label)
#define JNAE_rel8(rel,label) OP(0x72); OP(rel); MARK_JMP(rel,label)
#define JNG_rel8(rel,label)  OP(0x7E); OP(rel); MARK_JMP(rel,label)
#define JNGE_rel8(rel,label) OP(0x7C); OP(rel); MARK_JMP(rel,label)
#define JNC_rel8(rel,label)  OP(0x73); OP(rel); MARK_JMP(rel,label)
#define JNO_rel8(rel,label)  OP(0x71); OP(rel); MARK_JMP(rel,label)
#define JNS_rel8(rel,label)  OP(0x79); OP(rel); MARK_JMP(rel,label)
#define JS_rel8(rel,label)   OP(0x78); OP(rel); MARK_JMP(rel,label)


/* 32-bit long forms w/ backpatching to an exit routine */
#define JMP_exit(rel)  OP(0xE9); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JE_exit(rel)  OP(0x0F); OP(0x84); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JA_exit(rel)  OP(0x0F); OP(0x87); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JAE_exit(rel) OP(0x0F); OP(0x83); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JG_exit(rel)  OP(0x0F); OP(0x8F); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JGE_exit(rel) OP(0x0F); OP(0x8D); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JC_exit(rel)  OP(0x0F); OP(0x82); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JO_exit(rel)  OP(0x0F); OP(0x80); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JNE_exit(rel) OP(0x0F); OP(0x85); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JNA_exit(rel) OP(0x0F); OP(0x86); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JNAE_exit(rel) OP(0x0F);OP(0x82); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JNG_exit(rel) OP(0x0F); OP(0x8E); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JNGE_exit(rel) OP(0x0F);OP(0x8C); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JNC_exit(rel) OP(0x0F); OP(0x83); sh4_x86_add_backpatch(xlat_output); OP32(rel)
#define JNO_exit(rel) OP(0x0F); OP(0x81); sh4_x86_add_backpatch(xlat_output); OP32(rel)


/* Conditional moves ebp-rel */
#define CMOVE_r32_r32(r1,r2)  OP(0x0F); OP(0x44); MODRM_rm32_r32(r1,r2)
#define CMOVA_r32_r32(r1,r2)  OP(0x0F); OP(0x47); MODRM_rm32_r32(r1,r2)
#define CMOVAE_r32_r32(r1,r2) OP(0x0F); OP(0x43); MODRM_rm32_r32(r1,r2)
#define CMOVG_r32_r32(r1,r2)  OP(0x0F); OP(0x4F); MODRM_rm32_r32(r1,r2)
#define CMOVGE_r32_r32(r1,r2)  OP(0x0F); OP(0x4D); MODRM_rm32_r32(r1,r2)
#define CMOVC_r32_r32(r1,r2)  OP(0x0F); OP(0x42); MODRM_rm32_r32(r1,r2)
#define CMOVO_r32_r32(r1,r2)  OP(0x0F); OP(0x40); MODRM_rm32_r32(r1,r2)


/* Conditional setcc - writeback to sh4r.t */
#define SETE_sh4r(disp)    OP(0x0F); OP(0x94); MODRM_r32_sh4r(0, disp);
#define SETA_sh4r(disp)    OP(0x0F); OP(0x97); MODRM_r32_sh4r(0, disp);
#define SETAE_sh4r(disp)   OP(0x0F); OP(0x93); MODRM_r32_sh4r(0, disp);
#define SETG_sh4r(disp)    OP(0x0F); OP(0x9F); MODRM_r32_sh4r(0, disp);
#define SETGE_sh4r(disp)   OP(0x0F); OP(0x9D); MODRM_r32_sh4r(0, disp);
#define SETC_sh4r(disp)    OP(0x0F); OP(0x92); MODRM_r32_sh4r(0, disp);
#define SETO_sh4r(disp)    OP(0x0F); OP(0x90); MODRM_r32_sh4r(0, disp);

#define SETNE_sh4r(disp)   OP(0x0F); OP(0x95); MODRM_r32_sh4r(0, disp);
#define SETNA_sh4r(disp)   OP(0x0F); OP(0x96); MODRM_r32_sh4r(0, disp);
#define SETNAE_sh4r(disp)  OP(0x0F); OP(0x92); MODRM_r32_sh4r(0, disp);
#define SETNG_sh4r(disp)   OP(0x0F); OP(0x9E); MODRM_r32_sh4r(0, disp);
#define SETNGE_sh4r(disp)  OP(0x0F); OP(0x9C); MODRM_r32_sh4r(0, disp);
#define SETNC_sh4r(disp)   OP(0x0F); OP(0x93); MODRM_r32_sh4r(0, disp);
#define SETNO_sh4r(disp)   OP(0x0F); OP(0x91); MODRM_r32_sh4r(0, disp);

#define SETE_t() SETE_sh4r(R_T)
#define SETA_t() SETA_sh4r(R_T)
#define SETAE_t() SETAE_sh4r(R_T)
#define SETG_t() SETG_sh4r(R_T)
#define SETGE_t() SETGE_sh4r(R_T)
#define SETC_t() SETC_sh4r(R_T)
#define SETO_t() SETO_sh4r(R_T)
#define SETNE_t() SETNE_sh4r(R_T)

#define SETC_r8(r1)      OP(0x0F); OP(0x92); MODRM_rm32_r32(r1, 0)

/* Pseudo-op Load carry from T: CMP [EBP+t], #01 ; CMC */
#define LDC_t()     OP(0x83); MODRM_r32_sh4r(7,R_T); OP(0x01); CMC()

#endif /* !__lxdream_x86op_H */
