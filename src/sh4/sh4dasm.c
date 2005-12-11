#include "sh4core.h"
#include "sh4dasm.h"
#include "mem.h"

#define UNIMP(ir) snprintf( buf, len, "???     " )


struct reg_desc_struct sh4_reg_map[] = 
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


struct cpu_desc_struct sh4_cpu_desc = { "SH4", sh4_disasm_instruction, 2,
					(char *)&sh4r, sizeof(sh4r), sh4_reg_map,
					&sh4r.pc, &sh4r.icount };

uint32_t sh4_disasm_instruction( uint32_t pc, char *buf, int len )
{
    uint16_t ir = sh4_read_word(pc);
    
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

    switch( (ir&0xF000)>>12 ) {
        case 0: /* 0000nnnnmmmmxxxx */
            switch( ir&0x000F ) {
                case 2:
                    switch( (ir&0x00F0)>>4 ) {
                        case 0: snprintf( buf, len, "STC     SR, R%d", RN(ir) ); break;
                        case 1: snprintf( buf, len, "STC     GBR, R%d", RN(ir) ); break;
                        case 2: snprintf( buf, len, "STC     VBR, R%d", RN(ir) ); break;
                        case 3: snprintf( buf, len, "STC     SSR, R%d", RN(ir) ); break;
                        case 4: snprintf( buf, len, "STC     SPC, R%d", RN(ir) ); break;
                        case 8: case 9: case 10: case 11: case 12: case 13: case 14:
                        case 15:snprintf( buf, len, "STC     R%d_bank, R%d", RN_BANK(ir), RN(ir) ); break;
                        default: UNIMP(ir);
                    }
                    break;
                case 3:
                    switch( (ir&0x00F0)>>4 ) {
                        case 0: snprintf( buf, len, "BSRF    R%d", RN(ir) ); break;
                        case 2: snprintf( buf, len, "BRAF    R%d", RN(ir) ); break;
                        case 8: snprintf( buf, len, "PREF    [R%d]", RN(ir) ); break;
                        case 9: snprintf( buf, len, "OCBI    [R%d]", RN(ir) ); break;
                        case 10:snprintf( buf, len, "OCBP    [R%d]", RN(ir) ); break;
                        case 11:snprintf( buf, len, "OCBWB   [R%d]", RN(ir) ); break;
                        case 12:snprintf( buf, len, "MOVCA.L R0, [R%d]", RN(ir) ); break;
                        default: UNIMP(ir);
                    }
                    break;
                case 4: snprintf( buf, len, "MOV.B   R%d, [R0+R%d]", RM(ir), RN(ir) ); break;
                case 5: snprintf( buf, len, "MOV.W   R%d, [R0+R%d]", RM(ir), RN(ir) ); break;
                case 6: snprintf( buf, len, "MOV.L   R%d, [R0+R%d]", RM(ir), RN(ir) ); break;
                case 7: snprintf( buf, len, "MUL.L   R%d, R%d", RM(ir), RN(ir) ); break;
                case 8:
                    switch( (ir&0x0FF0)>>4 ) {
                        case 0: snprintf( buf, len, "CLRT    " ); break;
                        case 1: snprintf( buf, len, "SETT    " ); break;
                        case 2: snprintf( buf, len, "CLRMAC  " ); break;
                        case 3: snprintf( buf, len, "LDTLB   " ); break;
                        case 4: snprintf( buf, len, "CLRS    " ); break;
                        case 5: snprintf( buf, len, "SETS    " ); break;
                        default: UNIMP(ir);
                    }
                    break;
                case 9:
                    if( (ir&0x00F0) == 0x20 )
                        snprintf( buf, len, "MOVT    R%d", RN(ir) );
                    else if( ir == 0x0019 )
                        snprintf( buf, len, "DIV0U   " );
                    else if( ir == 0x0009 )
                        snprintf( buf, len, "NOP     " );
                    else UNIMP(ir);
                    break;
                case 10:
                    switch( (ir&0x00F0) >> 4 ) {
                        case 0: snprintf( buf, len, "STS     MACH, R%d", RN(ir) ); break;
                        case 1: snprintf( buf, len, "STS     MACL, R%d", RN(ir) ); break;
                        case 2: snprintf( buf, len, "STS     PR, R%d", RN(ir) ); break;
                        case 3: snprintf( buf, len, "STC     SGR, R%d", RN(ir) ); break;
                        case 5: snprintf( buf, len, "STS     FPUL, R%d", RN(ir) ); break;
                        case 6: snprintf( buf, len, "STS     FPSCR, R%d", RN(ir) ); break;
                        case 15:snprintf( buf, len, "STC     DBR, R%d", RN(ir) ); break;
                        default: UNIMP(ir);
                    }
                    break;
                case 11:
                    switch( (ir&0x0FF0)>>4 ) {
                        case 0: snprintf( buf, len, "RTS     " ); break;
                        case 1: snprintf( buf, len, "SLEEP   " ); break;
                        case 2: snprintf( buf, len, "RTE     " ); break;
                        default:UNIMP(ir);
                    }
                    break;
                case 12:snprintf( buf, len, "MOV.B   [R0+R%d], R%d", RM(ir), RN(ir) ); break;
                case 13:snprintf( buf, len, "MOV.W   [R0+R%d], R%d", RM(ir), RN(ir) ); break;
                case 14:snprintf( buf, len, "MOV.L   [R0+R%d], R%d", RM(ir), RN(ir) ); break;
                case 15:snprintf( buf, len, "MAC.L   [R%d++], [R%d++]", RM(ir), RN(ir) ); break;
                default: UNIMP(ir);
            }
            break;
        case 1: /* 0001nnnnmmmmdddd */
            snprintf( buf, len, "MOV.L   R%d, [R%d%+d]", RM(ir), RN(ir), DISP4(ir)<<2 ); break;
        case 2: /* 0010nnnnmmmmxxxx */
            switch( ir&0x000F ) {
                case 0: snprintf( buf, len, "MOV.B   R%d, [R%d]", RM(ir), RN(ir) ); break;
                case 1: snprintf( buf, len, "MOV.W   R%d, [R%d]", RM(ir), RN(ir) ); break;
                case 2: snprintf( buf, len, "MOV.L   R%d, [R%d]", RM(ir), RN(ir) ); break;
                case 3: UNIMP(ir); break;
                case 4: snprintf( buf, len, "MOV.B   R%d, [--R%d]", RM(ir), RN(ir) ); break;
                case 5: snprintf( buf, len, "MOV.W   R%d, [--R%d]", RM(ir), RN(ir) ); break;
                case 6: snprintf( buf, len, "MOV.L   R%d, [--R%d]", RM(ir), RN(ir) ); break;
                case 7: snprintf( buf, len, "DIV0S   R%d, R%d", RM(ir), RN(ir) ); break;
                case 8: snprintf( buf, len, "TST     R%d, R%d", RM(ir), RN(ir) ); break;
                case 9: snprintf( buf, len, "AND     R%d, R%d", RM(ir), RN(ir) ); break;
                case 10:snprintf( buf, len, "XOR     R%d, R%d", RM(ir), RN(ir) ); break;
                case 11:snprintf( buf, len, "OR      R%d, R%d", RM(ir), RN(ir) ); break;
                case 12:snprintf( buf, len, "CMP/STR R%d, R%d", RM(ir), RN(ir) ); break;
                case 13:snprintf( buf, len, "XTRCT   R%d, R%d", RM(ir), RN(ir) ); break;
                case 14:snprintf( buf, len, "MULU.W  R%d, R%d", RM(ir), RN(ir) ); break;
                case 15:snprintf( buf, len, "MULS.W  R%d, R%d", RM(ir), RN(ir) ); break;
            }
            break;
        case 3: /* 0011nnnnmmmmxxxx */
            switch( ir&0x000F ) {
                case 0: snprintf( buf, len, "CMP/EQ  R%d, R%d", RM(ir), RN(ir) ); break;
                case 2: snprintf( buf, len, "CMP/HS  R%d, R%d", RM(ir), RN(ir) ); break;
                case 3: snprintf( buf, len, "CMP/GE  R%d, R%d", RM(ir), RN(ir) ); break;
                case 4: snprintf( buf, len, "DIV1    R%d, R%d", RM(ir), RN(ir) ); break;
                case 5: snprintf( buf, len, "DMULU.L R%d, R%d", RM(ir), RN(ir) ); break;
                case 6: snprintf( buf, len, "CMP/HI  R%d, R%d", RM(ir), RN(ir) ); break;
                case 7: snprintf( buf, len, "CMP/GT  R%d, R%d", RM(ir), RN(ir) ); break;
                case 8: snprintf( buf, len, "SUB     R%d, R%d", RM(ir), RN(ir) ); break;
                case 10:snprintf( buf, len, "SUBC    R%d, R%d", RM(ir), RN(ir) ); break;
                case 11:snprintf( buf, len, "SUBV    R%d, R%d", RM(ir), RN(ir) ); break;
                case 12:snprintf( buf, len, "ADD     R%d, R%d", RM(ir), RN(ir) ); break;
                case 13:snprintf( buf, len, "DMULS.L R%d, R%d", RM(ir), RN(ir) ); break;
                case 14:snprintf( buf, len, "ADDC    R%d, R%d", RM(ir), RN(ir) ); break;
                case 15:snprintf( buf, len, "ADDV    R%d, R%d", RM(ir), RN(ir) ); break;
                default: UNIMP(ir);
            }
            break;
        case 4: /* 0100nnnnxxxxxxxx */
            switch( ir&0x00FF ) {
                case 0x00: snprintf( buf, len, "SHLL    R%d", RN(ir) ); break;
                case 0x01: snprintf( buf, len, "SHLR    R%d", RN(ir) ); break;
                case 0x02: snprintf( buf, len, "STS.L   MACH, [--R%d]", RN(ir) ); break;
                case 0x03: snprintf( buf, len, "STC.L   SR, [--R%d]", RN(ir) ); break;
                case 0x04: snprintf( buf, len, "ROTL    R%d", RN(ir) ); break;
                case 0x05: snprintf( buf, len, "ROTR    R%d", RN(ir) ); break;
                case 0x06: snprintf( buf, len, "LDS.L   [R%d++], MACH", RN(ir) ); break;
                case 0x07: snprintf( buf, len, "LDC.L   [R%d++], SR", RN(ir) ); break;
                case 0x08: snprintf( buf, len, "SHLL2   R%d", RN(ir) ); break;
                case 0x09: snprintf( buf, len, "SHLR2   R%d", RN(ir) ); break;
                case 0x0A: snprintf( buf, len, "LDS     R%d, MACH", RN(ir) ); break;
                case 0x0B: snprintf( buf, len, "JSR     [R%d]", RN(ir) ); break;
                case 0x0E: snprintf( buf, len, "LDC     R%d, SR", RN(ir) ); break;
                case 0x10: snprintf( buf, len, "DT      R%d", RN(ir) ); break;
                case 0x11: snprintf( buf, len, "CMP/PZ  R%d", RN(ir) ); break;
                case 0x12: snprintf( buf, len, "STS.L   MACL, [--R%d]", RN(ir) ); break;
                case 0x13: snprintf( buf, len, "STC.L   GBR, [--R%d]", RN(ir) ); break;
                case 0x15: snprintf( buf, len, "CMP/PL  R%d", RN(ir) ); break;
                case 0x16: snprintf( buf, len, "LDS.L   [R%d++], MACL", RN(ir) ); break;
                case 0x17: snprintf( buf, len, "LDC.L   [R%d++], GBR", RN(ir) ); break;
                case 0x18: snprintf( buf, len, "SHLL8   R%d", RN(ir) ); break;
                case 0x19: snprintf( buf, len, "SHLR8   R%d", RN(ir) ); break;
                case 0x1A: snprintf( buf, len, "LDS     R%d, MACL", RN(ir) ); break;
                case 0x1B: snprintf( buf, len, "TAS.B   [R%d]", RN(ir) ); break;
                case 0x1E: snprintf( buf, len, "LDC     R%d, GBR", RN(ir) ); break;
                case 0x20: snprintf( buf, len, "SHAL    R%d", RN(ir) ); break;
                case 0x21: snprintf( buf, len, "SHAR    R%d", RN(ir) ); break;
                case 0x22: snprintf( buf, len, "STS.L   PR, [--R%d]", RN(ir) ); break;
                case 0x23: snprintf( buf, len, "STC.L   VBR, [--R%d]", RN(ir) ); break;
                case 0x24: snprintf( buf, len, "ROTCL   R%d", RN(ir) ); break;
                case 0x25: snprintf( buf, len, "ROTCR   R%d", RN(ir) ); break;
                case 0x26: snprintf( buf, len, "LDS.L   [R%d++], PR", RN(ir) ); break;
                case 0x27: snprintf( buf, len, "LDC.L   [R%d++], VBR", RN(ir) ); break;
                case 0x28: snprintf( buf, len, "SHLL16  R%d", RN(ir) ); break;
                case 0x29: snprintf( buf, len, "SHLR16  R%d", RN(ir) ); break;
                case 0x2A: snprintf( buf, len, "LDS     R%d, PR", RN(ir) ); break;
                case 0x2B: snprintf( buf, len, "JMP     [R%d]", RN(ir) ); break;
                case 0x2E: snprintf( buf, len, "LDC     R%d, VBR", RN(ir) ); break;
                case 0x32: snprintf( buf, len, "STC.L   SGR, [--R%d]", RN(ir) ); break;
                case 0x33: snprintf( buf, len, "STC.L   SSR, [--R%d]", RN(ir) ); break;
                case 0x37: snprintf( buf, len, "LDC.L   [R%d++], SSR", RN(ir) ); break;
                case 0x3E: snprintf( buf, len, "LDC     R%d, SSR", RN(ir) ); break;
                case 0x43: snprintf( buf, len, "STC.L   SPC, [--R%d]", RN(ir) ); break;
                case 0x47: snprintf( buf, len, "LDC.L   [R%d++], SPC", RN(ir) ); break;
                case 0x4E: snprintf( buf, len, "LDC     R%d, SPC", RN(ir) ); break;
                case 0x52: snprintf( buf, len, "STS.L   FPUL, [--R%d]", RN(ir) ); break;
                case 0x56: snprintf( buf, len, "LDS.L   [R%d++], FPUL", RN(ir) ); break;
                case 0x5A: snprintf( buf, len, "LDS     R%d, FPUL", RN(ir) ); break;
                case 0x62: snprintf( buf, len, "STS.L   FPSCR, [--R%d]", RN(ir) ); break;
                case 0x66: snprintf( buf, len, "LDS.L   [R%d++], FPSCR", RN(ir) ); break;
                case 0x6A: snprintf( buf, len, "LDS     R%d, FPSCR", RN(ir) ); break;
                case 0xF2: snprintf( buf, len, "STC.L   DBR, [--R%d]", RN(ir) ); break;
                case 0xF6: snprintf( buf, len, "LDC.L   [R%d++], DBR", RN(ir) ); break;
                case 0xFA: snprintf( buf, len, "LDC     R%d, DBR", RN(ir) ); break;
                case 0x83: case 0x93: case 0xA3: case 0xB3: case 0xC3: case 0xD3: case 0xE3:
                case 0xF3: snprintf( buf, len, "STC.L   R%d_BANK, [--R%d]", RN_BANK(ir), RN(ir) ); break;
                case 0x87: case 0x97: case 0xA7: case 0xB7: case 0xC7: case 0xD7: case 0xE7:
                case 0xF7: snprintf( buf, len, "LDC.L   [R%d++], R%d_BANK", RN(ir), RN_BANK(ir) ); break; 
                case 0x8E: case 0x9E: case 0xAE: case 0xBE: case 0xCE: case 0xDE: case 0xEE:
                case 0xFE: snprintf( buf, len, "LDC     R%d, R%d_BANK", RN(ir), RN_BANK(ir) ); break;
                default:
                    if( (ir&0x000F) == 0x0F ) {
                        snprintf( buf, len, "MAC.W   [R%d++], [R%d++]", RM(ir), RN(ir) );
                    } else if( (ir&0x000F) == 0x0C ) {
                        snprintf( buf, len, "SHAD    R%d, R%d", RM(ir), RN(ir) );
                    } else if( (ir&0x000F) == 0x0D ) {
                        snprintf( buf, len, "SHLD    R%d, R%d", RM(ir), RN(ir) );
                    } else UNIMP(ir);
            }
            break;
        case 5: /* 0101nnnnmmmmdddd */
            snprintf( buf, len, "MOV.L   [R%d%+d], R%d", RM(ir), DISP4(ir)<<2, RN(ir) ); break;
        case 6: /* 0110xxxxxxxxxxxx */
            switch( ir&0x000f ) {
                case 0: snprintf( buf, len, "MOV.B   [R%d], R%d", RM(ir), RN(ir) ); break;
                case 1: snprintf( buf, len, "MOV.W   [R%d], R%d", RM(ir), RN(ir) ); break;
                case 2: snprintf( buf, len, "MOV.L   [R%d], R%d", RM(ir), RN(ir) ); break;
                case 3: snprintf( buf, len, "MOV     R%d, R%d", RM(ir), RN(ir) );   break;
                case 4: snprintf( buf, len, "MOV.B   [R%d++], R%d", RM(ir), RN(ir) ); break;
                case 5: snprintf( buf, len, "MOV.W   [R%d++], R%d", RM(ir), RN(ir) ); break;
                case 6: snprintf( buf, len, "MOV.L   [R%d++], R%d", RM(ir), RN(ir) ); break;
                case 7: snprintf( buf, len, "NOT     R%d, R%d", RM(ir), RN(ir) ); break;
                case 8: snprintf( buf, len, "SWAP.B  R%d, R%d", RM(ir), RN(ir) ); break;
                case 9: snprintf( buf, len, "SWAP.W  R%d, R%d", RM(ir), RN(ir) ); break;
                case 10:snprintf( buf, len, "NEGC    R%d, R%d", RM(ir), RN(ir) ); break;
                case 11:snprintf( buf, len, "NEG     R%d, R%d", RM(ir), RN(ir) ); break;
                case 12:snprintf( buf, len, "EXTU.B  R%d, R%d", RM(ir), RN(ir) ); break;
                case 13:snprintf( buf, len, "EXTU.W  R%d, R%d", RM(ir), RN(ir) ); break;
                case 14:snprintf( buf, len, "EXTS.B  R%d, R%d", RM(ir), RN(ir) ); break;
                case 15:snprintf( buf, len, "EXTS.W  R%d, R%d", RM(ir), RN(ir) ); break;
            }
            break;
        case 7: /* 0111nnnniiiiiiii */
            snprintf( buf, len, "ADD    #%d, R%d", SIGNEXT8(ir&0x00FF), RN(ir) ); break;
        case 8: /* 1000xxxxxxxxxxxx */
            switch( (ir&0x0F00) >> 8 ) {
                case 0: snprintf( buf, len, "MOV.B   R0, [R%d%+d]", RM(ir), DISP4(ir) ); break;
                case 1: snprintf( buf, len, "MOV.W   R0, [R%d%+d]", RM(ir), DISP4(ir)<<1 ); break;
                case 4: snprintf( buf, len, "MOV.B   [R%d%+d], R0", RM(ir), DISP4(ir) ); break;
                case 5: snprintf( buf, len, "MOV.W   [R%d%+d], R0", RM(ir), DISP4(ir)<<1 ); break;
                case 8: snprintf( buf, len, "CMP/EQ  #%d, R0", IMM8(ir) ); break;
                case 9: snprintf( buf, len, "BT      $%xh", (PCDISP8(ir)<<1)+pc+4 ); break;
                case 11:snprintf( buf, len, "BF      $%xh", (PCDISP8(ir)<<1)+pc+4 ); break;
                case 13:snprintf( buf, len, "BT/S    $%xh", (PCDISP8(ir)<<1)+pc+4 ); break;
                case 15:snprintf( buf, len, "BF/S    $%xh", (PCDISP8(ir)<<1)+pc+4 ); break;
                default: UNIMP(ir);
            }
            break;
        case 9: /* 1001xxxxxxxxxxxx */
            snprintf( buf, len, "MOV.W   [$%xh], R%-2d ; <- #%08x", (DISP8(ir)<<1)+pc+4, RN(ir),
                      sh4_read_word( (DISP8(ir)<<1)+pc+4 ) ); break;
        case 10:/* 1010xxxxxxxxxxxx */
            snprintf( buf, len, "BRA     $%xh", (DISP12(ir)<<1)+pc+4 ); break;
        case 11:/* 1011xxxxxxxxxxxx */
            snprintf( buf, len, "BSR     $%xh", (DISP12(ir)<<1)+pc+4 ); break;            
        case 12:/* 1100xxxxdddddddd */
            switch( (ir&0x0F00)>>8 ) {
                case 0: snprintf( buf, len, "MOV.B   R0, [GBR%+d]", DISP8(ir) ); break;
                case 1: snprintf( buf, len, "MOV.W   R0, [GBR%+d]", DISP8(ir)<<1 ); break;
                case 2: snprintf( buf, len, "MOV.L   R0, [GBR%+d]", DISP8(ir)<<2 ); break;
                case 3: snprintf( buf, len, "TRAPA   #%d", UIMM8(ir) ); break;
                case 4: snprintf( buf, len, "MOV.B   [GBR%+d], R0", DISP8(ir) ); break;
                case 5: snprintf( buf, len, "MOV.W   [GBR%+d], R0", DISP8(ir)<<1 ); break;
                case 6: snprintf( buf, len, "MOV.L   [GBR%+d], R0", DISP8(ir)<<2 ); break;
                case 7: snprintf( buf, len, "MOVA    $%xh, R0", (DISP8(ir)<<2)+(pc&~3)+4 ); break;
                case 8: snprintf( buf, len, "TST     #%02Xh, R0", UIMM8(ir) ); break;
                case 9: snprintf( buf, len, "AND     #%02Xh, R0", UIMM8(ir) ); break;
                case 10:snprintf( buf, len, "XOR     #%02Xh, R0", UIMM8(ir) ); break;
                case 11:snprintf( buf, len, "OR      #%02Xh, R0", UIMM8(ir) ); break;
                case 12:snprintf( buf, len, "TST.B   #%02Xh, [R0+GBR]", UIMM8(ir) ); break;
                case 13:snprintf( buf, len, "AND.B   #%02Xh, [R0+GBR]", UIMM8(ir) ); break;
                case 14:snprintf( buf, len, "XOR.B   #%02Xh, [R0+GBR]", UIMM8(ir) ); break;
                case 15:snprintf( buf, len, "OR.B    #%02Xh, [R0+GBR]", UIMM8(ir) ); break;
            }
            break;
        case 13:/* 1101xxxxxxxxxxxx */
            snprintf( buf, len, "MOV.L   [$%xh], R%-2d ; <- #%08x", (DISP8(ir)<<2)+(pc&~3)+4, RN(ir),
                      sh4_read_long( (DISP8(ir)<<2)+(pc&~3)+4 ) ); break;
        case 14:/* 1110xxxxxxxxxxxx */
            snprintf( buf, len, "MOV     #%d, R%d", DISP8(ir), RN(ir)); break;
        case 15:/* 1111xxxxxxxxxxxx */
            switch( ir&0x000F ) {
                case 0: snprintf( buf, len, "FADD    FR%d, FR%d", RM(ir), RN(ir) ); break;
                case 1: snprintf( buf, len, "FSUB    FR%d, FR%d", RM(ir), RN(ir) ); break;
                case 2: snprintf( buf, len, "FMUL    FR%d, FR%d", RM(ir), RN(ir) ); break;
                case 3: snprintf( buf, len, "FDIV    FR%d, FR%d", RM(ir), RN(ir) ); break;
                case 4: snprintf( buf, len, "FCMP/EQ FR%d, FR%d", RM(ir), RN(ir) ); break;
                case 5: snprintf( buf, len, "FCMP/GT FR%d, FR%d", RM(ir), RN(ir) ); break;
                case 6: snprintf( buf, len, "FMOV.S  [R%d+R0], FR%d", RM(ir), RN(ir) ); break;
                case 7: snprintf( buf, len, "FMOV.S  FR%d, [R%d+R0]", RM(ir), RN(ir) ); break;
                case 8: snprintf( buf, len, "FMOV.S  [R%d], FR%d", RM(ir), RN(ir) ); break;
                case 9: snprintf( buf, len, "FMOV.S  [R%d++], FR%d", RM(ir), RN(ir) ); break;
                case 10:snprintf( buf, len, "FMOV.S  FR%d, [R%d]", RM(ir), RN(ir) ); break;
                case 11:snprintf( buf, len, "FMOV.S  FR%d, [--R%d]", RM(ir), RN(ir) ); break;
                case 12:snprintf( buf, len, "FMOV    FR%d, FR%d", RM(ir), RN(ir) ); break;
                case 13:
                    switch( (ir&0x00F0) >> 4 ) {
                        case 0: snprintf( buf, len, "FSTS    FPUL, FR%d", RN(ir) ); break;
                        case 1: snprintf( buf, len, "FLDS    FR%d, FPUL", RN(ir) ); break;
                        case 2: snprintf( buf, len, "FLOAT   FPUL, FR%d", RN(ir) ); break;
                        case 3: snprintf( buf, len, "FTRC    FR%d, FPUL", RN(ir) ); break;
                        case 4: snprintf( buf, len, "FNEG    FR%d", RN(ir) ); break;
                        case 5: snprintf( buf, len, "FABS    FR%d", RN(ir) ); break;
                        case 6: snprintf( buf, len, "FSQRT   FR%d", RN(ir) ); break;
                        case 7: snprintf( buf, len, "FSRRA   FR%d", RN(ir) ); break;
                        case 8: snprintf( buf, len, "FLDI0   FR%d", RN(ir) ); break;
                        case 9: snprintf( buf, len, "FLDI1   FR%d", RN(ir) ); break;
                        case 10:snprintf( buf, len, "FCNVSD  FPUL, DR%d", RN(ir)>>1 ); break;
                        case 11:snprintf( buf, len, "FCNVDS  DR%d, FPUL", RN(ir)>>1 ); break;
                        case 14:snprintf( buf, len, "FIPR    FV%d, FV%d", FVM(ir), FVN(ir) ); break;
                        case 15:
                            if( (ir & 0x0300) == 0x0100 )
                                snprintf( buf, len, "FTRV    XMTRX,FV%d", FVN(ir) );
                            else if( (ir & 0x0100) == 0 )
                                snprintf( buf, len, "FSCA    FPUL, DR%d", RN(ir) );
                            else if( ir == 0xFBFD )
                                snprintf( buf, len, "FRCHG   " );
                            else if( ir == 0xF3FD )
                                snprintf( buf, len, "FSCHG   " );
                            else UNIMP(ir);
                            break;
                        default: UNIMP(ir);
                    }
                    break;
                case 14:snprintf( buf, len, "FMAC    FR0, FR%d, FR%d", RM(ir), RN(ir) ); break;
                default: UNIMP(ir);
            }
            break;
    }
    return pc+2;
}


void sh4_disasm_region( FILE *f, int from, int to, int load_addr )
{
    int pc;
    char buf[80];
    
    for( pc = from; pc < to; pc+=2 ) {
        uint16_t op = sh4_read_word( pc );
        buf[0] = '\0';
        sh4_disasm_instruction( pc,
                                buf, sizeof(buf) );
        fprintf( f, "  %08x:  %04x  %s\n", pc + load_addr, op, buf );
    }
}
