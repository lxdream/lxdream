/**
 * $Id: sh4dasm.c,v 1.11 2007-08-23 12:33:27 nkeynes Exp $
 * 
 * SH4 CPU definition and disassembly functions
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

#include "sh4core.h"
#include "sh4dasm.h"
#include "mem.h"

#define UNIMP(ir) snprintf( buf, len, "???     " )


const struct reg_desc_struct sh4_reg_map[] = 
  { {"R0", REG_INT, &sh4r.r[0]}, {"R1", REG_INT, &sh4r.r[1]},
    {"R2", REG_INT, &sh4r.r[2]}, {"R3", REG_INT, &sh4r.r[3]},
    {"R4", REG_INT, &sh4r.r[4]}, {"R5", REG_INT, &sh4r.r[5]},
    {"R6", REG_INT, &sh4r.r[6]}, {"R7", REG_INT, &sh4r.r[7]},
    {"R8", REG_INT, &sh4r.r[8]}, {"R9", REG_INT, &sh4r.r[9]},
    {"R10",REG_INT, &sh4r.r[10]}, {"R11",REG_INT, &sh4r.r[11]},
    {"R12",REG_INT, &sh4r.r[12]}, {"R13",REG_INT, &sh4r.r[13]},
    {"R14",REG_INT, &sh4r.r[14]}, {"R15",REG_INT, &sh4r.r[15]},
    {"SR", REG_INT, &sh4r.sr}, {"GBR", REG_INT, &sh4r.gbr},
    {"SSR",REG_INT, &sh4r.ssr}, {"SPC", REG_INT, &sh4r.spc},
    {"SGR",REG_INT, &sh4r.sgr}, {"DBR", REG_INT, &sh4r.dbr},
    {"VBR",REG_INT, &sh4r.vbr},
    {"PC", REG_INT, &sh4r.pc}, {"PR", REG_INT, &sh4r.pr},
    {"MACL",REG_INT, &sh4r.mac},{"MACH",REG_INT, ((uint32_t *)&sh4r.mac)+1},
    {"FPUL", REG_INT, &sh4r.fpul}, {"FPSCR", REG_INT, &sh4r.fpscr},
    {NULL, 0, NULL} };


const struct cpu_desc_struct sh4_cpu_desc = 
    { "SH4", sh4_disasm_instruction, sh4_execute_instruction, mem_has_page, 
      sh4_set_breakpoint, sh4_clear_breakpoint, sh4_get_breakpoint, 2,
      (char *)&sh4r, sizeof(sh4r), sh4_reg_map,
      &sh4r.pc };

uint32_t sh4_disasm_instruction( uint32_t pc, char *buf, int len, char *opcode )
{
    uint16_t ir = sh4_read_word(pc);

#define UNDEF(ir) snprintf( buf, len, "????    " );
#define RN(ir) ((ir&0x0F00)>>8)
#define RN_BANK(ir) ((ir&0x0070)>>4)
#define RM(ir) ((ir&0x00F0)>>4)
#define DISP4(ir) (ir&0x000F) /* 4-bit displacements are *not* sign extended */
#define DISP8(ir) (ir&0x00FF)
#define PCDISP8(ir) SIGNEXT8(ir&0x00FF)
#define UIMM8(ir) (ir&0x00FF)
#define IMM8(ir) SIGNEXT8(ir&0x00FF)
#define DISP12(ir) SIGNEXT12(ir&0x0FFF)
#define FVN(ir) ((ir&0x0C00)>>10)
#define FVM(ir) ((ir&0x0300)>>8)

    sprintf( opcode, "%02X %02X", ir&0xFF, ir>>8 );

        switch( (ir&0xF000) >> 12 ) {
            case 0x0:
                switch( ir&0xF ) {
                    case 0x2:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* STC SR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "STC     SR, R%d", Rn );
                                        }
                                        break;
                                    case 0x1:
                                        { /* STC GBR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "STC     GBR, R%d", Rn );
                                        }
                                        break;
                                    case 0x2:
                                        { /* STC VBR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "STC     VBR, R%d", Rn );
                                        }
                                        break;
                                    case 0x3:
                                        { /* STC SSR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "STC     SSR, R%d", Rn );
                                        }
                                        break;
                                    case 0x4:
                                        { /* STC SPC, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "STC     SPC, R%d", Rn );
                                        }
                                        break;
                                    default:
                                        UNDEF();
                                        break;
                                }
                                break;
                            case 0x1:
                                { /* STC Rm_BANK, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm_BANK = ((ir>>4)&0x7); 
                                snprintf( buf, len, "STC     R%d_BANK, R%d", Rm_BANK, Rn );
                                }
                                break;
                        }
                        break;
                    case 0x3:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* BSRF Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "BSRF    R%d", Rn );
                                }
                                break;
                            case 0x2:
                                { /* BRAF Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "BRAF    R%d", Rn );
                                }
                                break;
                            case 0x8:
                                { /* PREF @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "PREF    R%d", Rn );
                                }
                                break;
                            case 0x9:
                                { /* OCBI @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "OCBI    @R%d", Rn );
                                }
                                break;
                            case 0xA:
                                { /* OCBP @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "OCBP    @R%d", Rn );
                                }
                                break;
                            case 0xB:
                                { /* OCBWB @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "OCBWB   @R%d", Rn );
                                }
                                break;
                            case 0xC:
                                { /* MOVCA.L R0, @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "MOVCA.L R0, @R%d", Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x4:
                        { /* MOV.B Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.B   R%d, @(R0, R%d)", Rm, Rn );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.W   R%d, @(R0, R%d)", Rm, Rn );
                        }
                        break;
                    case 0x6:
                        { /* MOV.L Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.L   R%d, @(R0, R%d)", Rm, Rn );
                        }
                        break;
                    case 0x7:
                        { /* MUL.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MUL.L   R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x8:
                        switch( (ir&0xFF0) >> 4 ) {
                            case 0x0:
                                { /* CLRT */
                                snprintf( buf, len, "CLRT    " );
                                }
                                break;
                            case 0x1:
                                { /* SETT */
                                snprintf( buf, len, "SETT    " );
                                }
                                break;
                            case 0x2:
                                { /* CLRMAC */
                                snprintf( buf, len, "CLRMAC  " );
                                }
                                break;
                            case 0x3:
                                { /* LDTLB */
                                snprintf( buf, len, "LDTLB   " );
                                }
                                break;
                            case 0x4:
                                { /* CLRS */
                                snprintf( buf, len, "CLRS    " );
                                }
                                break;
                            case 0x5:
                                { /* SETS */
                                snprintf( buf, len, "SETS    " );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x9:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* NOP */
                                snprintf( buf, len, "NOP     " );
                                }
                                break;
                            case 0x1:
                                { /* DIV0U */
                                snprintf( buf, len, "DIV0U   " );
                                }
                                break;
                            case 0x2:
                                { /* MOVT Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "MOVT    R%d", Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xA:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* STS MACH, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STS     MACH, R%d", Rn );
                                }
                                break;
                            case 0x1:
                                { /* STS MACL, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STS     MACL, R%d", Rn );
                                }
                                break;
                            case 0x2:
                                { /* STS PR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STS     PR, R%d", Rn );
                                }
                                break;
                            case 0x3:
                                { /* STC SGR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STC     SGR, R%d", Rn );
                                }
                                break;
                            case 0x5:
                                { /* STS FPUL, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STS     FPUL, R%d", Rn );
                                }
                                break;
                            case 0x6:
                                { /* STS FPSCR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STS     FPSCR, R%d", Rn );
                                }
                                break;
                            case 0xF:
                                { /* STC DBR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STC     DBR, R%d", Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xB:
                        switch( (ir&0xFF0) >> 4 ) {
                            case 0x0:
                                { /* RTS */
                                snprintf( buf, len, "RTS     " );
                                }
                                break;
                            case 0x1:
                                { /* SLEEP */
                                snprintf( buf, len, "SLEEP   " );
                                }
                                break;
                            case 0x2:
                                { /* RTE */
                                snprintf( buf, len, "RTE     " );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xC:
                        { /* MOV.B @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.B   @(R0, R%d), R%d", Rm, Rn );
                        }
                        break;
                    case 0xD:
                        { /* MOV.W @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.W   @(R0, R%d), R%d", Rm, Rn );
                        }
                        break;
                    case 0xE:
                        { /* MOV.L @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.L   @(R0, R%d), R%d", Rm, Rn );
                        }
                        break;
                    case 0xF:
                        { /* MAC.L @Rm+, @Rn+ */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MAC.L   @R%d+, @R%d+", Rm, Rn );
                        }
                        break;
                    default:
                        UNDEF();
                        break;
                }
                break;
            case 0x1:
                { /* MOV.L Rm, @(disp, Rn) */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<2; 
                snprintf( buf, len, "MOV.L   R%d, @(%d, R%d)", Rm, disp, Rn );
                }
                break;
            case 0x2:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* MOV.B Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.B   R%d, @R%d", Rm, Rn );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.W   R%d, @R%d", Rm, Rn );
                        }
                        break;
                    case 0x2:
                        { /* MOV.L Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.L   R%d, @R%d", Rm, Rn );
                        }
                        break;
                    case 0x4:
                        { /* MOV.B Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.B   R%d, @-R%d", Rm, Rn );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.W   R%d, @-R%d", Rm, Rn );
                        }
                        break;
                    case 0x6:
                        { /* MOV.L Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.L   R%d, @-R%d", Rm, Rn );
                        }
                        break;
                    case 0x7:
                        { /* DIV0S Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "DIV0S   R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x8:
                        { /* TST Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "TST     R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x9:
                        { /* AND Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "AND     R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xA:
                        { /* XOR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "XOR     R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xB:
                        { /* OR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "OR      R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xC:
                        { /* CMP/STR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "CMP/STR R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xD:
                        { /* XTRCT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "XTRCT   R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xE:
                        { /* MULU.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MULU.W  R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xF:
                        { /* MULS.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MULS.W  R%d, R%d", Rm, Rn );
                        }
                        break;
                    default:
                        UNDEF();
                        break;
                }
                break;
            case 0x3:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* CMP/EQ Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "CMP/EQ  R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x2:
                        { /* CMP/HS Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "CMP/HS  R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x3:
                        { /* CMP/GE Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "CMP/GE  R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x4:
                        { /* DIV1 Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "DIV1    R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x5:
                        { /* DMULU.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "DMULU.L R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x6:
                        { /* CMP/HI Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "CMP/HI  R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x7:
                        { /* CMP/GT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "CMP/GT  R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x8:
                        { /* SUB Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "SUB     R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xA:
                        { /* SUBC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "SUBC    R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xB:
                        { /* SUBV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "SUBV    R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xC:
                        { /* ADD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "ADD     R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xD:
                        { /* DMULS.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "DMULS.L R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xE:
                        { /* ADDC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "ADDC    R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xF:
                        { /* ADDV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "ADDV    R%d, R%d", Rm, Rn );
                        }
                        break;
                    default:
                        UNDEF();
                        break;
                }
                break;
            case 0x4:
                switch( ir&0xF ) {
                    case 0x0:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "SHLL    R%d", Rn );
                                }
                                break;
                            case 0x1:
                                { /* DT Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "DT      R%d", Rn );
                                }
                                break;
                            case 0x2:
                                { /* SHAL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "SHAL    R%d", Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x1:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "SHLR    R%d", Rn );
                                }
                                break;
                            case 0x1:
                                { /* CMP/PZ Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "CMP/PZ  R%d", Rn );
                                }
                                break;
                            case 0x2:
                                { /* SHAR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "SHAR    R%d", Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x2:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* STS.L MACH, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STS.L   MACH, @-R%d", Rn );
                                }
                                break;
                            case 0x1:
                                { /* STS.L MACL, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STS.L   MACL, @-R%d", Rn );
                                }
                                break;
                            case 0x2:
                                { /* STS.L PR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STS.L   PR, @-R%d", Rn );
                                }
                                break;
                            case 0x3:
                                { /* STC.L SGR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STC.L   SGR, @-R%d", Rn );
                                }
                                break;
                            case 0x5:
                                { /* STS.L FPUL, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STS.L   FPUL, @-R%d", Rn );
                                }
                                break;
                            case 0x6:
                                { /* STS.L FPSCR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STS.L   FPSCR, @-R%d", Rn );
                                }
                                break;
                            case 0xF:
                                { /* STC.L DBR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "STC.L    DBR, @-R%d", Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x3:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* STC.L SR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "STC.L   SR, @-R%d", Rn );
                                        }
                                        break;
                                    case 0x1:
                                        { /* STC.L GBR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "STC.L   GBR, @-R%d", Rn );
                                        }
                                        break;
                                    case 0x2:
                                        { /* STC.L VBR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "STC.L   VBR, @-R%d", Rn );
                                        }
                                        break;
                                    case 0x3:
                                        { /* STC.L SSR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "STC.L   SSR, @-R%d", Rn );
                                        }
                                        break;
                                    case 0x4:
                                        { /* STC.L SPC, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "STC.L   SPC, @-R%d", Rn );
                                        }
                                        break;
                                    default:
                                        UNDEF();
                                        break;
                                }
                                break;
                            case 0x1:
                                { /* STC.L Rm_BANK, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm_BANK = ((ir>>4)&0x7); 
                                snprintf( buf, len, "STC.L   @-R%d_BANK, @-R%d", Rm_BANK, Rn );
                                }
                                break;
                        }
                        break;
                    case 0x4:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* ROTL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "ROTL    R%d", Rn );
                                }
                                break;
                            case 0x2:
                                { /* ROTCL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "ROTCL   R%d", Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x5:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* ROTR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "ROTR    R%d", Rn );
                                }
                                break;
                            case 0x1:
                                { /* CMP/PL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "CMP/PL  R%d", Rn );
                                }
                                break;
                            case 0x2:
                                { /* ROTCR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "ROTCR   R%d", Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x6:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* LDS.L @Rm+, MACH */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDS.L   @R%d+, MACH", Rm );
                                }
                                break;
                            case 0x1:
                                { /* LDS.L @Rm+, MACL */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDS.L   @R%d+, MACL", Rm );
                                }
                                break;
                            case 0x2:
                                { /* LDS.L @Rm+, PR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDS.L   @R%d+, PR", Rm );
                                }
                                break;
                            case 0x3:
                                { /* LDC.L @Rm+, SGR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDC.L   @R%d+, SGR", Rm );
                                }
                                break;
                            case 0x5:
                                { /* LDS.L @Rm+, FPUL */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDS.L   @R%d+, FPUL", Rm );
                                }
                                break;
                            case 0x6:
                                { /* LDS.L @Rm+, FPSCR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDS.L   @R%d+, FPSCR", Rm );
                                }
                                break;
                            case 0xF:
                                { /* LDC.L @Rm+, DBR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDC.L   @R%d+, DBR", Rm );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x7:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* LDC.L @Rm+, SR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "LDC.L   @R%d+, SR", Rm );
                                        }
                                        break;
                                    case 0x1:
                                        { /* LDC.L @Rm+, GBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "LDC.L   @R%d+, GBR", Rm );
                                        }
                                        break;
                                    case 0x2:
                                        { /* LDC.L @Rm+, VBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "LDC.L   @R%d+, VBR", Rm );
                                        }
                                        break;
                                    case 0x3:
                                        { /* LDC.L @Rm+, SSR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "LDC.L   @R%d+, SSR", Rm );
                                        }
                                        break;
                                    case 0x4:
                                        { /* LDC.L @Rm+, SPC */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "LDC.L   @R%d+, SPC", Rm );
                                        }
                                        break;
                                    default:
                                        UNDEF();
                                        break;
                                }
                                break;
                            case 0x1:
                                { /* LDC.L @Rm+, Rn_BANK */
                                uint32_t Rm = ((ir>>8)&0xF); uint32_t Rn_BANK = ((ir>>4)&0x7); 
                                snprintf( buf, len, "LDC.L   @R%d+, @R%d+_BANK", Rm, Rn_BANK );
                                }
                                break;
                        }
                        break;
                    case 0x8:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLL2 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "SHLL2   R%d", Rn );
                                }
                                break;
                            case 0x1:
                                { /* SHLL8 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "SHLL8   R%d", Rn );
                                }
                                break;
                            case 0x2:
                                { /* SHLL16 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "SHLL16  R%d", Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0x9:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLR2 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "SHLR2   R%d", Rn );
                                }
                                break;
                            case 0x1:
                                { /* SHLR8 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "SHLR8   R%d", Rn );
                                }
                                break;
                            case 0x2:
                                { /* SHLR16 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "SHLR16  R%d", Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xA:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* LDS Rm, MACH */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDS     R%d, MACH", Rm );
                                }
                                break;
                            case 0x1:
                                { /* LDS Rm, MACL */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDS     R%d, MACL", Rm );
                                }
                                break;
                            case 0x2:
                                { /* LDS Rm, PR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDS     R%d, PR", Rm );
                                }
                                break;
                            case 0x3:
                                { /* LDC Rm, SGR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDC     R%d, SGR", Rm );
                                }
                                break;
                            case 0x5:
                                { /* LDS Rm, FPUL */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDS     R%d, FPUL", Rm );
                                }
                                break;
                            case 0x6:
                                { /* LDS Rm, FPSCR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDS     R%d, FPSCR", Rm );
                                }
                                break;
                            case 0xF:
                                { /* LDC Rm, DBR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "LDC     R%d, DBR", Rm );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xB:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* JSR @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "JSR     @R%d", Rn );
                                }
                                break;
                            case 0x1:
                                { /* TAS.B @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "TAS.B   R%d", Rn );
                                }
                                break;
                            case 0x2:
                                { /* JMP @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "JMP     @R%d", Rn );
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xC:
                        { /* SHAD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "SHAD    R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xD:
                        { /* SHLD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "SHLD    R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xE:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* LDC Rm, SR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "LDC     R%d, SR", Rm );
                                        }
                                        break;
                                    case 0x1:
                                        { /* LDC Rm, GBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "LDC     R%d, GBR", Rm );
                                        }
                                        break;
                                    case 0x2:
                                        { /* LDC Rm, VBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "LDC     R%d, VBR", Rm );
                                        }
                                        break;
                                    case 0x3:
                                        { /* LDC Rm, SSR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "LDC     R%d, SSR", Rm );
                                        }
                                        break;
                                    case 0x4:
                                        { /* LDC Rm, SPC */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        snprintf( buf, len, "LDC     R%d, SPC", Rm );
                                        }
                                        break;
                                    default:
                                        UNDEF();
                                        break;
                                }
                                break;
                            case 0x1:
                                { /* LDC Rm, Rn_BANK */
                                uint32_t Rm = ((ir>>8)&0xF); uint32_t Rn_BANK = ((ir>>4)&0x7); 
                                snprintf( buf, len, "LDC     R%d, R%d_BANK", Rm, Rn_BANK );
                                }
                                break;
                        }
                        break;
                    case 0xF:
                        { /* MAC.W @Rm+, @Rn+ */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MAC.W   @R%d+, @R%d+", Rm, Rn );
                        }
                        break;
                }
                break;
            case 0x5:
                { /* MOV.L @(disp, Rm), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<2; 
                snprintf( buf, len, "MOV.L   @(%d, R%d), @R%d", disp, Rm, Rn );
                }
                break;
            case 0x6:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* MOV.B @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.B   @R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.W   @R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x2:
                        { /* MOV.L @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.L   @R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x3:
                        { /* MOV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV     R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.B   @R%d+, R%d", Rm, Rn );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.W   @R%d+, R%d", Rm, Rn );
                        }
                        break;
                    case 0x6:
                        { /* MOV.L @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "MOV.L   @R%d+, R%d", Rm, Rn );
                        }
                        break;
                    case 0x7:
                        { /* NOT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "NOT     R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x8:
                        { /* SWAP.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "SWAP.B  R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0x9:
                        { /* SWAP.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "SWAP.W  R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xA:
                        { /* NEGC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "NEGC    R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xB:
                        { /* NEG Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "NEG     R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xC:
                        { /* EXTU.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "EXTU.B  R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xD:
                        { /* EXTU.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "EXTU.W  R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xE:
                        { /* EXTS.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "EXTS.B  R%d, R%d", Rm, Rn );
                        }
                        break;
                    case 0xF:
                        { /* EXTS.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "EXTS.W  R%d, R%d", Rm, Rn );
                        }
                        break;
                }
                break;
            case 0x7:
                { /* ADD #imm, Rn */
                uint32_t Rn = ((ir>>8)&0xF); int32_t imm = SIGNEXT8(ir&0xFF); 
                snprintf( buf, len, "ADD     #%d, R%d", imm, Rn );
                }
                break;
            case 0x8:
                switch( (ir&0xF00) >> 8 ) {
                    case 0x0:
                        { /* MOV.B R0, @(disp, Rn) */
                        uint32_t Rn = ((ir>>4)&0xF); uint32_t disp = (ir&0xF); 
                        snprintf( buf, len, "MOV.B   R0, @(%d, R%d)", disp, Rn );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W R0, @(disp, Rn) */
                        uint32_t Rn = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<1; 
                        snprintf( buf, len, "MOV.W   R0, @(%d, Rn)", disp, Rn );
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @(disp, Rm), R0 */
                        uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF); 
                        snprintf( buf, len, "MOV.B   @(%d, R%d), R0", disp, Rm );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @(disp, Rm), R0 */
                        uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<1; 
                        snprintf( buf, len, "MOV.W   @(%d, R%d), R0", disp, Rm );
                        }
                        break;
                    case 0x8:
                        { /* CMP/EQ #imm, R0 */
                        int32_t imm = SIGNEXT8(ir&0xFF); 
                        snprintf( buf, len, "CMP/EQ  #%d, R0", imm );
                        }
                        break;
                    case 0x9:
                        { /* BT disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
                        snprintf( buf, len, "BT      $%xh", disp+pc+4 );
                        }
                        break;
                    case 0xB:
                        { /* BF disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
                        snprintf( buf, len, "BF      $%xh", disp+pc+4 );
                        }
                        break;
                    case 0xD:
                        { /* BT/S disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
                        snprintf( buf, len, "BT/S    $%xh", disp+pc+4 );
                        }
                        break;
                    case 0xF:
                        { /* BF/S disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
                        snprintf( buf, len, "BF/S    $%xh", disp+pc+4 );
                        }
                        break;
                    default:
                        UNDEF();
                        break;
                }
                break;
            case 0x9:
                { /* MOV.W @(disp, PC), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t disp = (ir&0xFF)<<1; 
                snprintf( buf, len, "MOV.W   @($%xh), R%d ; <- #%08x", disp + pc + 4, Rn, sh4_read_word(disp+pc+4) );
                }
                break;
            case 0xA:
                { /* BRA disp */
                int32_t disp = SIGNEXT12(ir&0xFFF)<<1; 
                snprintf( buf, len, "BRA     $%xh", disp+pc+4 );
                }
                break;
            case 0xB:
                { /* BSR disp */
                int32_t disp = SIGNEXT12(ir&0xFFF)<<1; 
                snprintf( buf, len, "BSR     $%xh", disp+pc+4 );
                }
                break;
            case 0xC:
                switch( (ir&0xF00) >> 8 ) {
                    case 0x0:
                        { /* MOV.B R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF); 
                        snprintf( buf, len, "MOV.B   R0, @(%d, GBR)", disp );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF)<<1; 
                        snprintf( buf, len, "MOV.W   R0, @(%d, GBR)", disp);
                        }
                        break;
                    case 0x2:
                        { /* MOV.L R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF)<<2; 
                        snprintf( buf, len, "MOV.L   R0, @(%d, GBR)", disp );
                        }
                        break;
                    case 0x3:
                        { /* TRAPA #imm */
                        uint32_t imm = (ir&0xFF); 
                        snprintf( buf, len, "TRAPA   #%d", imm );
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF); 
                        snprintf( buf, len, "MOV.B   @(%d, GBR), R0", disp );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF)<<1; 
                        snprintf( buf, len, "MOV.W   @(%d, GBR), R0", disp );
                        }
                        break;
                    case 0x6:
                        { /* MOV.L @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF)<<2; 
                        snprintf( buf, len, "MOV.L   @(%d, GBR), R0",disp );
                        }
                        break;
                    case 0x7:
                        { /* MOVA @(disp, PC), R0 */
                        uint32_t disp = (ir&0xFF)<<2; 
                        snprintf( buf, len, "MOVA    @($%xh), R0", disp + (pc&0xFFFFFFFC) + 4 );
                        }
                        break;
                    case 0x8:
                        { /* TST #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
                        snprintf( buf, len, "TST     #%d, R0", imm );
                        }
                        break;
                    case 0x9:
                        { /* AND #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
                        snprintf( buf, len, "ADD     #%d, R0", imm );
                        }
                        break;
                    case 0xA:
                        { /* XOR #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
                        snprintf( buf, len, "XOR     #%d, R0", imm );
                        }
                        break;
                    case 0xB:
                        { /* OR #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
                        snprintf( buf, len, "OR      #%d, R0", imm );
                        }
                        break;
                    case 0xC:
                        { /* TST.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
                        snprintf( buf, len, "TST.B   #%d, @(R0, GBR)", imm );
                        }
                        break;
                    case 0xD:
                        { /* AND.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
                        snprintf( buf, len, "AND.B   #%d, @(R0, GBR)", imm );
                        }
                        break;
                    case 0xE:
                        { /* XOR.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
                        snprintf( buf, len, "XOR.B   #%d, @(R0, GBR)", imm );
                        }
                        break;
                    case 0xF:
                        { /* OR.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
                        snprintf( buf, len, "OR.B    #%d, @(R0, GBR)", imm );
                        }
                        break;
                }
                break;
            case 0xD:
                { /* MOV.L @(disp, PC), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t disp = (ir&0xFF)<<2; 
                snprintf( buf, len, "MOV.L   @($%xh), R%d ; <- #%08x", disp + (pc & 0xFFFFFFFC) + 4, Rn, sh4_read_long(disp+(pc&0xFFFFFFFC)+4) );
                }
                break;
            case 0xE:
                { /* MOV #imm, Rn */
                uint32_t Rn = ((ir>>8)&0xF); int32_t imm = SIGNEXT8(ir&0xFF); 
                snprintf( buf, len, "MOV     #%d, R%d", imm, Rn );
                }
                break;
            case 0xF:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* FADD FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FADD    FR%d, FR%d", FRm, FRn );
                        }
                        break;
                    case 0x1:
                        { /* FSUB FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FSUB    FRm, FR%d", FRm, FRn );
                        }
                        break;
                    case 0x2:
                        { /* FMUL FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FMUL    FRm, FR%d", FRm, FRn );
                        }
                        break;
                    case 0x3:
                        { /* FDIV FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FDIV    FR%d, FR%d", FRm, FRn );
                        }
                        break;
                    case 0x4:
                        { /* FCMP/EQ FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FCMP/EQ FR%d, FR%d", FRm, FRn );
                        }
                        break;
                    case 0x5:
                        { /* FCMP/GT FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FCMP/QT FR%d, FR%d", FRm, FRn );
                        }
                        break;
                    case 0x6:
                        { /* FMOV @(R0, Rm), FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FMOV    @(R0, R%d), FR%d", Rm, FRn );
                        }
                        break;
                    case 0x7:
                        { /* FMOV FRm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FMOV    FR%d, @(R0, R%d)", FRm, Rn );
                        }
                        break;
                    case 0x8:
                        { /* FMOV @Rm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FMOV    @R%d, FR%d", Rm, FRn );
                        }
                        break;
                    case 0x9:
                        { /* FMOV @Rm+, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FMOV    @R%d+, FR%d", Rm, FRn );
                        }
                        break;
                    case 0xA:
                        { /* FMOV FRm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FMOV    FR%d, @R%d", FRm, Rn );
                        }
                        break;
                    case 0xB:
                        { /* FMOV FRm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FMOV    FR%d, @-R%d", FRm, Rn );
                        }
                        break;
                    case 0xC:
                        { /* FMOV FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FMOV    FR%d, FR%d", FRm, FRn );
                        }
                        break;
                    case 0xD:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* FSTS FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "FSTS    FPUL, FR%d", FRn );
                                }
                                break;
                            case 0x1:
                                { /* FLDS FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "FLDS    FR%d, FPUL", FRm );
                                }
                                break;
                            case 0x2:
                                { /* FLOAT FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "FLOAT   FPUL, FR%d", FRn );
                                }
                                break;
                            case 0x3:
                                { /* FTRC FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "FTRC    FR%d, FPUL", FRm );
                                }
                                break;
                            case 0x4:
                                { /* FNEG FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "FNEG    FR%d", FRn );
                                }
                                break;
                            case 0x5:
                                { /* FABS FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "FABS    FR%d", FRn );
                                }
                                break;
                            case 0x6:
                                { /* FSQRT FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "FSQRT   FR%d", FRn );
                                }
                                break;
                            case 0x7:
                                { /* FSRRA FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "FSRRA   FR%d", FRn );
                                }
                                break;
                            case 0x8:
                                { /* FLDI0 FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "FLDI0   FR%d", FRn );
                                }
                                break;
                            case 0x9:
                                { /* FLDI1 FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "FLDI1   FR%d", FRn );
                                }
                                break;
                            case 0xA:
                                { /* FCNVSD FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                snprintf( buf, len, "FCNVSD  FPUL, FR%d", FRn );
                                }
                                break;
                            case 0xB:
                                { /* FCNVDS FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
                                snprintf( buf, len, "FCNVDS  FR%d, FPUL", FRm );
                                }
                                break;
                            case 0xE:
                                { /* FIPR FVm, FVn */
                                uint32_t FVn = ((ir>>10)&0x3); uint32_t FVm = ((ir>>8)&0x3); 
                                snprintf( buf, len, "FIPR    FV%d, FV%d", FVm, FVn );
                                }
                                break;
                            case 0xF:
                                switch( (ir&0x100) >> 8 ) {
                                    case 0x0:
                                        { /* FSCA FPUL, FRn */
                                        uint32_t FRn = ((ir>>9)&0x7)<<1; 
                                        snprintf( buf, len, "FSCA    FPUL, FR%d", FRn );
                                        }
                                        break;
                                    case 0x1:
                                        switch( (ir&0x200) >> 9 ) {
                                            case 0x0:
                                                { /* FTRV XMTRX, FVn */
                                                uint32_t FVn = ((ir>>10)&0x3); 
                                                snprintf( buf, len, "FTRV    XMTRX, FV%d", FVn );
                                                }
                                                break;
                                            case 0x1:
                                                switch( (ir&0xC00) >> 10 ) {
                                                    case 0x0:
                                                        { /* FSCHG */
                                                        snprintf( buf, len, "FSCHG   " );
                                                        }
                                                        break;
                                                    case 0x2:
                                                        { /* FRCHG */
                                                        snprintf( buf, len, "FRCHG   " );
                                                        }
                                                        break;
                                                    case 0x3:
                                                        { /* UNDEF */
                                                        snprintf( buf, len, "UNDEF   " );
                                                        }
                                                        break;
                                                    default:
                                                        UNDEF();
                                                        break;
                                                }
                                                break;
                                        }
                                        break;
                                }
                                break;
                            default:
                                UNDEF();
                                break;
                        }
                        break;
                    case 0xE:
                        { /* FMAC FR0, FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        snprintf( buf, len, "FMAC    FR0, FR%d, FR%d", FRm, FRn );
                        }
                        break;
                    default:
                        UNDEF();
                        break;
                }
                break;
        }

    return pc+2;
}


void sh4_disasm_region( const gchar *filename, int from, int to )
{
    int pc;
    char buf[80];
    char opcode[16];
    FILE *f;
    
    f = fopen( filename, "w" );
    for( pc = from; pc < to; pc+=2 ) {
        buf[0] = '\0';
        sh4_disasm_instruction( pc,
                                buf, sizeof(buf), opcode );
        fprintf( f, "  %08x:  %s  %s\n", pc, opcode, buf );
    }
    fclose(f);
}
