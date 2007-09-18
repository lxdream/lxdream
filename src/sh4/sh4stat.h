/**
 * $Id: sh4stat.h,v 1.1 2007-09-18 08:58:23 nkeynes Exp $
 * 
 * Support module for collecting instruction stats
 *
 * Copyright (c) 2005 Nathan Keynes.
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

enum sh4_inst_id {
    I_UNKNOWN,
    I_ADD, I_ADDI, I_ADDC, I_ADDV,
    I_AND, I_ANDI, I_ANDB, 
    I_BF, I_BFS, I_BRA, I_BRAF, I_BSR, I_BSRF, I_BT, I_BTS,
    I_CLRMAC, I_CLRS, I_CLRT, 
    I_CMPEQ, I_CMPEQI, I_CMPGE, I_CMPGT, I_CMPHI, I_CMPHS, I_CMPPL, I_CMPPZ, I_CMPSTR,
    I_DIV0S, I_DIV0U, I_DIV1,
    I_DMULS, I_DMULU, I_DT,   
    I_EXTSB, I_EXTSW, I_EXTUB, I_EXTUW, I_FABS, 
    I_FADD, I_FCMPEQ, I_FCMPGT, I_FCNVDS, I_FCNVSD, I_FDIV, I_FIPR, I_FLDS,
    I_FLDI0, I_FLDI1, I_FLOAT, I_FMAC, I_FMOV1, I_FMOV2, I_FMOV3, I_FMOV4, 
    I_FMOV5, I_FMOV6, I_FMOV7, I_FMUL, I_FNEG, I_FRCHG, I_FSCA, I_FSCHG, 
    I_FSQRT, I_FSRRA, I_FSTS, I_FSUB, I_FTRC, I_FTRV,  
    I_JMP, I_JSR,   
    I_LDCSR, I_LDC, I_LDCSRM, I_LDCM, I_LDS, I_LDSM, I_LDTLB, 
    I_MACL, I_MACW,  
    I_MOV, I_MOVI, I_MOVB, I_MOVL, I_MOVLPC, I_MOVW, I_MOVA, I_MOVCA, I_MOVT,  
    I_MULL, I_MULSW, I_MULUW, 
    I_NEG, I_NEGC, I_NOP, I_NOT,  
    I_OCBI, I_OCBP, I_OCBWB, 
    I_OR, I_ORI, I_ORB,   
    I_PREF, 
    I_ROTCL, I_ROTCR, I_ROTL, I_ROTR, 
    I_RTE, I_RTS, 
    I_SETS, I_SETT,  
    I_SHAD, I_SHAL, I_SHAR, I_SHLD, I_SHLL, I_SHLR,  
    I_SLEEP, 
    I_STCSR, I_STC, I_STCSRM, I_STCM, I_STS, I_STSM,  
    I_SUB, I_SUBC, I_SUBV,  
    I_SWAPB, I_SWAPW, I_TASB,  
    I_TRAPA,
    I_TST, I_TSTI, I_TSTB,  
    I_XOR, I_XORI, I_XORB,  
    I_XTRCT, 
    I_UNDEF };

#define SH4_INSTRUCTION_COUNT I_UNDEF
