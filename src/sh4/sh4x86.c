/**
 * $Id: sh4x86.c,v 1.1 2007-08-23 12:33:27 nkeynes Exp $
 * 
 * SH4 => x86 translation. This version does no real optimization, it just
 * outputs straight-line x86 code - it mainly exists to provide a baseline
 * to test the optimizing versions against.
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

#include "sh4core.h"
#include "sh4trans.h"
#include "x86op.h"

/**
 * Emit an instruction to load an SH4 reg into a real register
 */
static inline void load_reg( int x86reg, int sh4reg ) 
{
    /* mov [bp+n], reg */
    OP(0x89);
    OP(0x45 + x86reg<<3);
    OP(REG_OFFSET(r[sh4reg]));
}

static inline void load_spreg( int x86reg, int regoffset )
{
    /* mov [bp+n], reg */
    OP(0x89);
    OP(0x45 + x86reg<<3);
    OP(regoffset);
}

#define UNDEF()
#define MEM_READ_BYTE( addr_reg, value_reg )
#define MEM_READ_WORD( addr_reg, value_reg )
#define MEM_READ_LONG( addr_reg, value_reg )
#define MEM_WRITE_BYTE( addr_reg, value_reg )
#define MEM_WRITE_WORD( addr_reg, value_reg )
#define MEM_WRITE_LONG( addr_reg, value_reg )

/**
 * Emit an instruction to load an immediate value into a register
 */
static inline void load_imm32( int x86reg, uint32_t value ) {
    /* mov #value, reg */
    OP(0xB8 + x86reg);
    OP32(value);
}

/**
 * Emit an instruction to store an SH4 reg (RN)
 */
void static inline store_reg( int x86reg, int sh4reg ) {
    /* mov reg, [bp+n] */
    OP(0x8B);
    OP(0x45 + x86reg<<3);
    OP(REG_OFFSET(r[sh4reg]));
}
void static inline store_spreg( int x86reg, int regoffset ) {
    /* mov reg, [bp+n] */
    OP(0x8B);
    OP(0x45 + x86reg<<3);
    OP(regoffset);
}


/**
 * Emit the 'start of block' assembly. Sets up the stack frame and save
 * SI/DI as required
 */
void sh4_translate_begin_block() {
    /* push ebp */
    *xlat_output++ = 0x50 + R_EBP;

    /* mov &sh4r, ebp */
    load_imm32( R_EBP, (uint32_t)&sh4r );

    /* load carry from SR */
}

/**
 * Flush any open regs back to memory, restore SI/DI/, update PC, etc
 */
void sh4_translate_end_block( sh4addr_t pc ) {
    /* pop ebp */
    *xlat_output++ = 0x58 + R_EBP;

    /* ret */
    *xlat_output++ = 0xC3;
}

/**
 * Translate a single instruction. Delayed branches are handled specially
 * by translating both branch and delayed instruction as a single unit (as
 * 
 *
 * @return true if the instruction marks the end of a basic block
 * (eg a branch or 
 */
uint32_t sh4_x86_translate_instruction( uint32_t pc )
{
    uint16_t ir = 0;

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
                                        /* TODO */
                                        }
                                        break;
                                    case 0x1:
                                        { /* STC GBR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        load_spreg( R_EAX, R_GBR );
                                        store_reg( R_EAX, Rn );
                                        }
                                        break;
                                    case 0x2:
                                        { /* STC VBR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        load_spreg( R_EAX, R_VBR );
                                        store_reg( R_EAX, Rn );
                                        }
                                        break;
                                    case 0x3:
                                        { /* STC SSR, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        load_spreg( R_EAX, R_SSR );
                                        store_reg( R_EAX, Rn );
                                        }
                                        break;
                                    case 0x4:
                                        { /* STC SPC, Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        load_spreg( R_EAX, R_SPC );
                                        store_reg( R_EAX, Rn );
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
                                /* TODO */
                                }
                                break;
                        }
                        break;
                    case 0x3:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* BSRF Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x2:
                                { /* BRAF Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x8:
                                { /* PREF @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x9:
                                { /* OCBI @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0xA:
                                { /* OCBP @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0xB:
                                { /* OCBWB @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0xC:
                                { /* MOVCA.L R0, @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
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
                        load_reg( R_EAX, 0 );
                        load_reg( R_ECX, Rn );
                        ADD_r32_r32( R_EAX, R_ECX );
                        load_reg( R_EAX, Rm );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x6:
                        { /* MOV.L Rm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x7:
                        { /* MUL.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x8:
                        switch( (ir&0xFF0) >> 4 ) {
                            case 0x0:
                                { /* CLRT */
                                }
                                break;
                            case 0x1:
                                { /* SETT */
                                }
                                break;
                            case 0x2:
                                { /* CLRMAC */
                                }
                                break;
                            case 0x3:
                                { /* LDTLB */
                                }
                                break;
                            case 0x4:
                                { /* CLRS */
                                }
                                break;
                            case 0x5:
                                { /* SETS */
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
                                /* Do nothing. Well, we could emit an 0x90, but what would really be the point? */
                                }
                                break;
                            case 0x1:
                                { /* DIV0U */
                                }
                                break;
                            case 0x2:
                                { /* MOVT Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_T );
                                store_reg( R_EAX, Rn );
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
                                load_spreg( R_EAX, R_MACH );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x1:
                                { /* STS MACL, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_MACL );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x2:
                                { /* STS PR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_PR );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x3:
                                { /* STC SGR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_SGR );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x5:
                                { /* STS FPUL, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_FPUL );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x6:
                                { /* STS FPSCR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_FPSCR );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0xF:
                                { /* STC DBR, Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_spreg( R_EAX, R_DBR );
                                store_reg( R_EAX, Rn );
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
                                }
                                break;
                            case 0x1:
                                { /* SLEEP */
                                }
                                break;
                            case 0x2:
                                { /* RTE */
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
                        load_reg( R_EAX, 0 );
                        load_reg( R_ECX, Rm );
                        ADD_r32_r32( R_EAX, R_ECX );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0xD:
                        { /* MOV.W @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xE:
                        { /* MOV.L @(R0, Rm), Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xF:
                        { /* MAC.L @Rm+, @Rn+ */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
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
                }
                break;
            case 0x2:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* MOV.B Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x2:
                        { /* MOV.L Rm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x4:
                        { /* MOV.B Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        ADD_imm8s_r32( -1, Rn );
                        store_reg( R_ECX, Rn );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x6:
                        { /* MOV.L Rm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x7:
                        { /* DIV0S Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x8:
                        { /* TST Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x9:
                        { /* AND Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        AND_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xA:
                        { /* XOR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        XOR_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xB:
                        { /* OR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        OR_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xC:
                        { /* CMP/STR Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xD:
                        { /* XTRCT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xE:
                        { /* MULU.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xF:
                        { /* MULS.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
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
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        CMP_r32_r32( R_EAX, R_ECX );
                        SETE_t();
                        }
                        break;
                    case 0x2:
                        { /* CMP/HS Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        CMP_r32_r32( R_EAX, R_ECX );
                        SETAE_t();
                        }
                        break;
                    case 0x3:
                        { /* CMP/GE Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        CMP_r32_r32( R_EAX, R_ECX );
                        SETGE_t();
                        }
                        break;
                    case 0x4:
                        { /* DIV1 Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x5:
                        { /* DMULU.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x6:
                        { /* CMP/HI Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        CMP_r32_r32( R_EAX, R_ECX );
                        SETA_t();
                        }
                        break;
                    case 0x7:
                        { /* CMP/GT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        CMP_r32_r32( R_EAX, R_ECX );
                        SETG_t();
                        }
                        break;
                    case 0x8:
                        { /* SUB Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        SUB_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xA:
                        { /* SUBC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        LDC_t();
                        SBB_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xB:
                        { /* SUBV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        SUB_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        SETO_t();
                        }
                        break;
                    case 0xC:
                        { /* ADD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        ADD_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xD:
                        { /* DMULS.L Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xE:
                        { /* ADDC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        LDC_t();
                        ADC_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        SETC_t();
                        }
                        break;
                    case 0xF:
                        { /* ADDV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        load_reg( R_ECX, Rn );
                        ADD_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        SETO_t();
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
                                load_reg( R_EAX, Rn );
                                SHL1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x1:
                                { /* DT Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                ADD_imm8s_r32( -1, Rn );
                                store_reg( R_EAX, Rn );
                                SETE_t();
                                }
                                break;
                            case 0x2:
                                { /* SHAL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHL1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
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
                                load_reg( R_EAX, Rn );
                                SHR1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x1:
                                { /* CMP/PZ Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                CMP_imm8s_r32( 0, R_EAX );
                                SETGE_t();
                                }
                                break;
                            case 0x2:
                                { /* SHAR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SAR1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
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
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, Rn );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_MACH );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            case 0x1:
                                { /* STS.L MACL, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, Rn );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_MACL );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            case 0x2:
                                { /* STS.L PR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, Rn );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_PR );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            case 0x3:
                                { /* STC.L SGR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, Rn );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_SGR );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            case 0x5:
                                { /* STS.L FPUL, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, Rn );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_FPUL );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            case 0x6:
                                { /* STS.L FPSCR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, Rn );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_FPSCR );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
                                }
                                break;
                            case 0xF:
                                { /* STC.L DBR, @-Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_ECX, Rn );
                                ADD_imm8s_r32( -4, Rn );
                                store_reg( R_ECX, Rn );
                                load_spreg( R_EAX, R_DBR );
                                MEM_WRITE_LONG( R_ECX, R_EAX );
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
                                        /* TODO */
                                        }
                                        break;
                                    case 0x1:
                                        { /* STC.L GBR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        load_reg( R_ECX, Rn );
                                        ADD_imm8s_r32( -4, Rn );
                                        store_reg( R_ECX, Rn );
                                        load_spreg( R_EAX, R_GBR );
                                        MEM_WRITE_LONG( R_ECX, R_EAX );
                                        }
                                        break;
                                    case 0x2:
                                        { /* STC.L VBR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        load_reg( R_ECX, Rn );
                                        ADD_imm8s_r32( -4, Rn );
                                        store_reg( R_ECX, Rn );
                                        load_spreg( R_EAX, R_VBR );
                                        MEM_WRITE_LONG( R_ECX, R_EAX );
                                        }
                                        break;
                                    case 0x3:
                                        { /* STC.L SSR, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        load_reg( R_ECX, Rn );
                                        ADD_imm8s_r32( -4, Rn );
                                        store_reg( R_ECX, Rn );
                                        load_spreg( R_EAX, R_SSR );
                                        MEM_WRITE_LONG( R_ECX, R_EAX );
                                        }
                                        break;
                                    case 0x4:
                                        { /* STC.L SPC, @-Rn */
                                        uint32_t Rn = ((ir>>8)&0xF); 
                                        load_reg( R_ECX, Rn );
                                        ADD_imm8s_r32( -4, Rn );
                                        store_reg( R_ECX, Rn );
                                        load_spreg( R_EAX, R_SPC );
                                        MEM_WRITE_LONG( R_ECX, R_EAX );
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
                                }
                                break;
                        }
                        break;
                    case 0x4:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* ROTL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                ROL1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                SETC_t();
                                }
                                break;
                            case 0x2:
                                { /* ROTCL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                LDC_t();
                                RCL1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                SETC_t();
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
                                load_reg( R_EAX, Rn );
                                ROR1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                SETC_t();
                                }
                                break;
                            case 0x1:
                                { /* CMP/PL Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                CMP_imm8s_r32( 0, R_EAX );
                                SETG_t();
                                }
                                break;
                            case 0x2:
                                { /* ROTCR Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                LDC_t();
                                RCR1_r32( R_EAX );
                                store_reg( R_EAX, Rn );
                                SETC_t();
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
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_MACH );
                                }
                                break;
                            case 0x1:
                                { /* LDS.L @Rm+, MACL */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_MACL );
                                }
                                break;
                            case 0x2:
                                { /* LDS.L @Rm+, PR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_PR );
                                }
                                break;
                            case 0x3:
                                { /* LDC.L @Rm+, SGR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_SGR );
                                }
                                break;
                            case 0x5:
                                { /* LDS.L @Rm+, FPUL */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_FPUL );
                                }
                                break;
                            case 0x6:
                                { /* LDS.L @Rm+, FPSCR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_FPSCR );
                                }
                                break;
                            case 0xF:
                                { /* LDC.L @Rm+, DBR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                MOV_r32_r32( R_EAX, R_ECX );
                                ADD_imm8s_r32( 4, R_EAX );
                                store_reg( R_EAX, Rm );
                                MEM_READ_LONG( R_ECX, R_EAX );
                                store_spreg( R_EAX, R_DBR );
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
                                        }
                                        break;
                                    case 0x1:
                                        { /* LDC.L @Rm+, GBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        load_reg( R_EAX, Rm );
                                        MOV_r32_r32( R_EAX, R_ECX );
                                        ADD_imm8s_r32( 4, R_EAX );
                                        store_reg( R_EAX, Rm );
                                        MEM_READ_LONG( R_ECX, R_EAX );
                                        store_spreg( R_EAX, R_GBR );
                                        }
                                        break;
                                    case 0x2:
                                        { /* LDC.L @Rm+, VBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        load_reg( R_EAX, Rm );
                                        MOV_r32_r32( R_EAX, R_ECX );
                                        ADD_imm8s_r32( 4, R_EAX );
                                        store_reg( R_EAX, Rm );
                                        MEM_READ_LONG( R_ECX, R_EAX );
                                        store_spreg( R_EAX, R_VBR );
                                        }
                                        break;
                                    case 0x3:
                                        { /* LDC.L @Rm+, SSR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        load_reg( R_EAX, Rm );
                                        MOV_r32_r32( R_EAX, R_ECX );
                                        ADD_imm8s_r32( 4, R_EAX );
                                        store_reg( R_EAX, Rm );
                                        MEM_READ_LONG( R_ECX, R_EAX );
                                        store_spreg( R_EAX, R_SSR );
                                        }
                                        break;
                                    case 0x4:
                                        { /* LDC.L @Rm+, SPC */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        load_reg( R_EAX, Rm );
                                        MOV_r32_r32( R_EAX, R_ECX );
                                        ADD_imm8s_r32( 4, R_EAX );
                                        store_reg( R_EAX, Rm );
                                        MEM_READ_LONG( R_ECX, R_EAX );
                                        store_spreg( R_EAX, R_SPC );
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
                                }
                                break;
                        }
                        break;
                    case 0x8:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* SHLL2 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHL_imm8_r32( 2, R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x1:
                                { /* SHLL8 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHL_imm8_r32( 8, R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x2:
                                { /* SHLL16 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHL_imm8_r32( 16, R_EAX );
                                store_reg( R_EAX, Rn );
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
                                load_reg( R_EAX, Rn );
                                SHR_imm8_r32( 2, R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x1:
                                { /* SHLR8 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHR_imm8_r32( 8, R_EAX );
                                store_reg( R_EAX, Rn );
                                }
                                break;
                            case 0x2:
                                { /* SHLR16 Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rn );
                                SHR_imm8_r32( 16, R_EAX );
                                store_reg( R_EAX, Rn );
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
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_MACH );
                                }
                                break;
                            case 0x1:
                                { /* LDS Rm, MACL */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_MACL );
                                }
                                break;
                            case 0x2:
                                { /* LDS Rm, PR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_PR );
                                }
                                break;
                            case 0x3:
                                { /* LDC Rm, SGR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_SGR );
                                }
                                break;
                            case 0x5:
                                { /* LDS Rm, FPUL */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_FPUL );
                                }
                                break;
                            case 0x6:
                                { /* LDS Rm, FPSCR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_FPSCR );
                                }
                                break;
                            case 0xF:
                                { /* LDC Rm, DBR */
                                uint32_t Rm = ((ir>>8)&0xF); 
                                load_reg( R_EAX, Rm );
                                store_spreg( R_EAX, R_DBR );
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
                                }
                                break;
                            case 0x1:
                                { /* TAS.B @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x2:
                                { /* JMP @Rn */
                                uint32_t Rn = ((ir>>8)&0xF); 
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
                        /* Annoyingly enough, not directly convertible */
                        }
                        break;
                    case 0xD:
                        { /* SHLD Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xE:
                        switch( (ir&0x80) >> 7 ) {
                            case 0x0:
                                switch( (ir&0x70) >> 4 ) {
                                    case 0x0:
                                        { /* LDC Rm, SR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        /* We need to be a little careful about SR */
                                        }
                                        break;
                                    case 0x1:
                                        { /* LDC Rm, GBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        load_reg( R_EAX, Rm );
                                        store_spreg( R_EAX, R_GBR );
                                        }
                                        break;
                                    case 0x2:
                                        { /* LDC Rm, VBR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        load_reg( R_EAX, Rm );
                                        store_spreg( R_EAX, R_VBR );
                                        }
                                        break;
                                    case 0x3:
                                        { /* LDC Rm, SSR */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        load_reg( R_EAX, Rm );
                                        store_spreg( R_EAX, R_SSR );
                                        }
                                        break;
                                    case 0x4:
                                        { /* LDC Rm, SPC */
                                        uint32_t Rm = ((ir>>8)&0xF); 
                                        load_reg( R_EAX, Rm );
                                        store_spreg( R_EAX, R_SPC );
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
                                }
                                break;
                        }
                        break;
                    case 0xF:
                        { /* MAC.W @Rm+, @Rn+ */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                }
                break;
            case 0x5:
                { /* MOV.L @(disp, Rm), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<2; 
                }
                break;
            case 0x6:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* MOV.B @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_ECX, Rm );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x2:
                        { /* MOV.L @Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x3:
                        { /* MOV Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_ECX, Rm );
                        MOV_r32_r32( R_ECX, R_EAX );
                        ADD_imm8s_r32( 1, R_EAX );
                        store_reg( R_EAX, Rm );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x6:
                        { /* MOV.L @Rm+, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x7:
                        { /* NOT Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        NOT_r32( R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x8:
                        { /* SWAP.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        XCHG_r8_r8( R_AL, R_AH );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0x9:
                        { /* SWAP.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        MOV_r32_r32( R_EAX, R_ECX );
                        SHL_imm8_r32( 16, R_ECX );
                        SHR_imm8_r32( 16, R_EAX );
                        OR_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        }
                        break;
                    case 0xA:
                        { /* NEGC Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        XOR_r32_r32( R_ECX, R_ECX );
                        LDC_t();
                        SBB_r32_r32( R_EAX, R_ECX );
                        store_reg( R_ECX, Rn );
                        SETC_t();
                        }
                        break;
                    case 0xB:
                        { /* NEG Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        NEG_r32( R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0xC:
                        { /* EXTU.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xD:
                        { /* EXTU.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xE:
                        { /* EXTS.B Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        load_reg( R_EAX, Rm );
                        MOVSX_r8_r32( R_EAX, R_EAX );
                        store_reg( R_EAX, Rn );
                        }
                        break;
                    case 0xF:
                        { /* EXTS.W Rm, Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                }
                break;
            case 0x7:
                { /* ADD #imm, Rn */
                uint32_t Rn = ((ir>>8)&0xF); int32_t imm = SIGNEXT8(ir&0xFF); 
                load_reg( R_EAX, Rn );
                ADD_imm8s_r32( imm, R_EAX );
                store_reg( R_EAX, Rn );
                }
                break;
            case 0x8:
                switch( (ir&0xF00) >> 8 ) {
                    case 0x0:
                        { /* MOV.B R0, @(disp, Rn) */
                        uint32_t Rn = ((ir>>4)&0xF); uint32_t disp = (ir&0xF); 
                        load_reg( R_EAX, 0 );
                        load_reg( R_ECX, Rn );
                        ADD_imm32_r32( disp, R_ECX );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W R0, @(disp, Rn) */
                        uint32_t Rn = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<1; 
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @(disp, Rm), R0 */
                        uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF); 
                        load_reg( R_ECX, Rm );
                        ADD_imm32_r32( disp, R_ECX );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @(disp, Rm), R0 */
                        uint32_t Rm = ((ir>>4)&0xF); uint32_t disp = (ir&0xF)<<1; 
                        }
                        break;
                    case 0x8:
                        { /* CMP/EQ #imm, R0 */
                        int32_t imm = SIGNEXT8(ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        CMP_imm8s_r32(imm, R_EAX);
                        SETE_t();
                        }
                        break;
                    case 0x9:
                        { /* BT disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
                        /* If true, result PC += 4 + disp. else result PC = pc+2 */
                          return pc + 2;
                        }
                        break;
                    case 0xB:
                        { /* BF disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
                        }
                        break;
                    case 0xD:
                        { /* BT/S disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
                        return pc + 4;
                        }
                        break;
                    case 0xF:
                        { /* BF/S disp */
                        int32_t disp = SIGNEXT8(ir&0xFF)<<1; 
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
                }
                break;
            case 0xA:
                { /* BRA disp */
                int32_t disp = SIGNEXT12(ir&0xFFF)<<1; 
                }
                break;
            case 0xB:
                { /* BSR disp */
                int32_t disp = SIGNEXT12(ir&0xFFF)<<1; 
                }
                break;
            case 0xC:
                switch( (ir&0xF00) >> 8 ) {
                    case 0x0:
                        { /* MOV.B R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        load_spreg( R_ECX, R_GBR );
                        ADD_imm32_r32( disp, R_ECX );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0x1:
                        { /* MOV.W R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF)<<1; 
                        }
                        break;
                    case 0x2:
                        { /* MOV.L R0, @(disp, GBR) */
                        uint32_t disp = (ir&0xFF)<<2; 
                        }
                        break;
                    case 0x3:
                        { /* TRAPA #imm */
                        uint32_t imm = (ir&0xFF); 
                        }
                        break;
                    case 0x4:
                        { /* MOV.B @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF); 
                        load_spreg( R_ECX, R_GBR );
                        ADD_imm32_r32( disp, R_ECX );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0x5:
                        { /* MOV.W @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF)<<1; 
                        }
                        break;
                    case 0x6:
                        { /* MOV.L @(disp, GBR), R0 */
                        uint32_t disp = (ir&0xFF)<<2; 
                        }
                        break;
                    case 0x7:
                        { /* MOVA @(disp, PC), R0 */
                        uint32_t disp = (ir&0xFF)<<2; 
                        }
                        break;
                    case 0x8:
                        { /* TST #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
                        }
                        break;
                    case 0x9:
                        { /* AND #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
                        // Note: x86 AND imm8 sign-extends, SH4 version zero-extends. So 
                        // need to use the imm32 version
                        load_reg( R_EAX, 0 );
                        AND_imm32_r32(imm, R_EAX); 
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0xA:
                        { /* XOR #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        XOR_imm32_r32( imm, R_EAX );
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0xB:
                        { /* OR #imm, R0 */
                        uint32_t imm = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        OR_imm32_r32(imm, R_EAX);
                        store_reg( R_EAX, 0 );
                        }
                        break;
                    case 0xC:
                        { /* TST.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
                        }
                        break;
                    case 0xD:
                        { /* AND.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        load_spreg( R_ECX, R_GBR );
                        ADD_r32_r32( R_EAX, R_EBX );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        AND_imm32_r32(imm, R_ECX );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0xE:
                        { /* XOR.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
                        load_reg( R_EAX, 0 );
                        load_spreg( R_ECX, R_GBR );
                        ADD_r32_r32( R_EAX, R_ECX );
                        MEM_READ_BYTE( R_ECX, R_EAX );
                        XOR_imm32_r32( imm, R_EAX );
                        MEM_WRITE_BYTE( R_ECX, R_EAX );
                        }
                        break;
                    case 0xF:
                        { /* OR.B #imm, @(R0, GBR) */
                        uint32_t imm = (ir&0xFF); 
                        }
                        break;
                }
                break;
            case 0xD:
                { /* MOV.L @(disp, PC), Rn */
                uint32_t Rn = ((ir>>8)&0xF); uint32_t disp = (ir&0xFF)<<2; 
                }
                break;
            case 0xE:
                { /* MOV #imm, Rn */
                uint32_t Rn = ((ir>>8)&0xF); int32_t imm = SIGNEXT8(ir&0xFF); 
                load_imm32( R_EAX, imm );
                store_reg( R_EAX, Rn );
                }
                break;
            case 0xF:
                switch( ir&0xF ) {
                    case 0x0:
                        { /* FADD FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x1:
                        { /* FSUB FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x2:
                        { /* FMUL FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x3:
                        { /* FDIV FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x4:
                        { /* FCMP/EQ FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x5:
                        { /* FCMP/GT FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x6:
                        { /* FMOV @(R0, Rm), FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x7:
                        { /* FMOV FRm, @(R0, Rn) */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x8:
                        { /* FMOV @Rm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0x9:
                        { /* FMOV @Rm+, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t Rm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xA:
                        { /* FMOV FRm, @Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xB:
                        { /* FMOV FRm, @-Rn */
                        uint32_t Rn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xC:
                        { /* FMOV FRm, FRn */
                        uint32_t FRn = ((ir>>8)&0xF); uint32_t FRm = ((ir>>4)&0xF); 
                        }
                        break;
                    case 0xD:
                        switch( (ir&0xF0) >> 4 ) {
                            case 0x0:
                                { /* FSTS FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x1:
                                { /* FLDS FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x2:
                                { /* FLOAT FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x3:
                                { /* FTRC FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x4:
                                { /* FNEG FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x5:
                                { /* FABS FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x6:
                                { /* FSQRT FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x7:
                                { /* FSRRA FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x8:
                                { /* FLDI0 FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0x9:
                                { /* FLDI1 FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0xA:
                                { /* FCNVSD FPUL, FRn */
                                uint32_t FRn = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0xB:
                                { /* FCNVDS FRm, FPUL */
                                uint32_t FRm = ((ir>>8)&0xF); 
                                }
                                break;
                            case 0xE:
                                { /* FIPR FVm, FVn */
                                uint32_t FVn = ((ir>>10)&0x3); uint32_t FVm = ((ir>>8)&0x3); 
                                }
                                break;
                            case 0xF:
                                switch( (ir&0x100) >> 8 ) {
                                    case 0x0:
                                        { /* FSCA FPUL, FRn */
                                        uint32_t FRn = ((ir>>9)&0x7)<<1; 
                                        }
                                        break;
                                    case 0x1:
                                        switch( (ir&0x200) >> 9 ) {
                                            case 0x0:
                                                { /* FTRV XMTRX, FVn */
                                                uint32_t FVn = ((ir>>10)&0x3); 
                                                }
                                                break;
                                            case 0x1:
                                                switch( (ir&0xC00) >> 10 ) {
                                                    case 0x0:
                                                        { /* FSCHG */
                                                        }
                                                        break;
                                                    case 0x2:
                                                        { /* FRCHG */
                                                        }
                                                        break;
                                                    case 0x3:
                                                        { /* UNDEF */
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
                        }
                        break;
                    default:
                        UNDEF();
                        break;
                }
                break;
        }


    return 0;
}
