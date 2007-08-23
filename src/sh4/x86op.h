/**
 * $Id: x86op.h,v 1.1 2007-08-23 12:33:27 nkeynes Exp $
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


#define OP(x) *xlat_output++ = x
#define OP32(x) *((uint32_t *)xlat_output) = x; xlat_output+=2

/* Offset of a reg relative to the sh4r structure */
#define REG_OFFSET(reg)  (((char *)&sh4r.reg) - ((char *)&sh4r))

#define R_T   REG_OFFSET(t)
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

/* Major opcodes */
#define ADD_r32_r32(r1,r2) OP(0x03); MODRM_rm32_r32(r1,r2)
#define ADD_imm8s_r32(imm,r1) OP(0x83); MODRM_rm32_r32(r1, 0); OP(imm)
#define ADC_r32_r32(r1,r2)    OP(0x13); MODRM_rm32_r32(r1,r2)
#define AND_r32_r32(r1,r2)    OP(0x23); MODRM_rm32_r32(r1,r2)
#define AND_imm32_r32(imm,r1) OP(0x81); MODRM_rm32_r32(r1,4); OP32(imm)
#define CMC()                 OP(0xF5)
#define CMP_r32_r32(r1,r2)    OP(0x3B); MODRM_rm32_r32(r1,r2)
#define CMP_imm8s_r32(imm,r1) OP(0x83); MODRM_rm32_r32(r1,7); OP(imm)
#define MOV_r32_ebp8(r1,disp) OP(0x89); MODRM_r32_ebp8(r1,disp)
#define MOV_r32_ebp32(r1,disp) OP(0x89); MODRM_r32_ebp32(r1,disp)
#define MOV_ebp8_r32(r1,disp) OP(0x8B); MODRM_r32_ebp8(r1,disp)
#define MOV_ebp32_r32(r1,disp) OP(0x8B); MODRM_r32_ebp32(r1,disp)
#define MOVSX_r8_r32(r1,r2)   OP(0x0F); OP(0xBE); MODRM_rm32_r32(r1,r2)
#define MOVSX_r16_r32(r1,r2)  OP(0x0F); OP(0xBF); MODRM_rm32_r32(r1,r2)
#define MOVZX_r8_r32(r1,r2)   OP(0x0F); OP(0xB6); MODRM_rm32_r32(r1,r2)
#define MOVZX_r16_r32(r1,r2)  OP(0x0F); OP(0xB7); MODRM_rm32_r32(r1,r2)
#define NEG_r32(r1)           OP(0xF7); MODRM_rm32_r32(r1,3)
#define NOT_r32(r1)           OP(0xF7); MODRM_rm32_r32(r1,2)
#define OR_r32_r32(r1,r2)     OP(0x0B); MODRM_rm32_r32(r1,r2)
#define OR_imm32_r32(imm,r1)  OP(0x81); MODRM_rm32_r32(r1,1); OP32(imm)
#define RCL1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,2)
#define RCR1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,3)
#define RET()                 OP(0xC3)
#define ROL1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,0)
#define ROR1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,1)
#define SAR1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,7)
#define SAR_imm8_r32(imm,r1)  OP(0xC1); MODRM_rm32_r32(r1,7); OP(imm)
#define SBB_r32_r32(r1,r2)    OP(0x1B); MODRM_rm32_r32(r1,r2)
#define SHL1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,4)
#define SHL_imm8_r32(imm,r1)  OP(0xC1); MODRM_rm32_r32(r1,4); OP(imm)
#define SHR1_r32(r1)          OP(0xD1); MODRM_rm32_r32(r1,5)
#define SHR_imm8_r32(imm,r1)  OP(0xC1); MODRM_rm32_r32(r1,5); OP(imm)
#define SUB_r32_r32(r1,r2)    OP(0x2B); MODRM_rm32_r32(r1,r2)
#define TEST_r32_r32(r1,r2)   OP(0x85); MODRM_rm32_r32(r1,r2)
#define TEST_imm32_r32(imm,r1) OP(0xF7); MODRM_rm32_r32(r1,0); OP32(imm)
#define XOR_r32_r32(r1,r2)    OP(0x33); MODRM_rm32_r32(r1,r2)
#define XOR_imm32_r32(imm,r1) OP(0x81); MODRM_rm32_r32(r1,6); OP32(imm)

#define ADD_imm32_r32(imm32,r1)
#define MOV_r32_r32(r1,r2)
#define XCHG_r8_r8(r1,r2)

/* Conditional branches */
#define JE_rel8(rel)   OP(0x74); OP(rel)
#define JA_rel8(rel)   OP(0x77); OP(rel)
#define JAE_rel8(rel)  OP(0x73); OP(rel)
#define JG_rel8(rel)   OP(0x7F); OP(rel)
#define JGE_rel8(rel)  OP(0x7D); OP(rel)
#define JC_rel8(rel)   OP(0x72); OP(rel)
#define JO_rel8(rel)   OP(0x70); OP(rel)

/* Negated forms */
#define JNE_rel8(rel)  OP(0x75); OP(rel)
#define JNA_rel8(rel)  OP(0x76); OP(rel)
#define JNAE_rel8(rel) OP(0x72); OP(rel)
#define JNG_rel8(rel)  OP(0x7E); OP(rel)
#define JNGE_rel8(rel) OP(0x7C); OP(rel)
#define JNC_rel8(rel)  OP(0x73); OP(rel)
#define JNO_rel8(rel)  OP(0x71); OP(rel)

/* Conditional setcc - writeback to sh4r.t */
#define SETE_t()    OP(0x0F); OP(0x94); MODRM_r32_ebp8(0, R_T);
#define SETA_t()    OP(0x0F); OP(0x97); MODRM_r32_ebp8(0, R_T);
#define SETAE_t()   OP(0x0F); OP(0x93); MODRM_r32_ebp8(0, R_T);
#define SETG_t()    OP(0x0F); OP(0x9F); MODRM_r32_ebp8(0, R_T);
#define SETGE_t()   OP(0x0F); OP(0x9D); MODRM_r32_ebp8(0, R_T);
#define SETC_t()    OP(0x0F); OP(0x92); MODRM_r32_ebp8(0, R_T);
#define SETO_t()    OP(0x0F); OP(0x90); MODRM_r32_ebp8(0, R_T);

#define SETNE_t()   OP(0x0F); OP(0x95); MODRM_r32_ebp8(0, R_T);
#define SETNA_t()   OP(0x0F); OP(0x96); MODRM_r32_ebp8(0, R_T);
#define SETNAE_t()  OP(0x0F); OP(0x92); MODRM_r32_ebp8(0, R_T);
#define SETNG_t()   OP(0x0F); OP(0x9E); MODRM_r32_ebp8(0, R_T);
#define SETNGE_t()  OP(0x0F); OP(0x9C); MODRM_r32_ebp8(0, R_T);
#define SETNC_t()   OP(0x0F); OP(0x93); MODRM_r32_ebp8(0, R_T);
#define SETNO_t()   OP(0x0F); OP(0x91); MODRM_r32_ebp8(0, R_T);

/* Pseudo-op Load carry from T: CMP [EBP+t], #01 ; CMC */
#define LDC_t()     OP(0x83); MODRM_r32_ebp8(7,R_T); OP(0x01); CMC()

#endif /* !__lxdream_x86op_H */
