/*
 * armdasm.c    21 Aug 2004  - ARM7tdmi (ARMv4) disassembler
 *
 * Copyright (c) 2004 Nathan Keynes. Distribution and modification permitted
 * under the terms of the GNU General Public License version 2 or later.
 */

#include "armcore.h"

#define COND(ir) (ir>>28)
#define OPCODE(ir) ((ir>>20)&0x1F)
#define GRP(ir) ((ir>>26)&0x03)
#define IFLAG(ir) (ir&0x02000000)
#define SFLAG(ir) (ir&0x00100000)
#define PFLAG(ir) (ir&0x01000000)
#define UFLAG(ir) (ir&0x00800000)
#define BFLAG(ir) (ir&0x00400000)
#define WFLAG(ir) (IR&0x00200000)
#define LFLAG(ir) SFLAG(ir)
#define RN(ir) ((ir>>16)&0x0F)
#define RD(ir) ((ir>>12)&0x0F)
#define RS(ir) ((ir>>8)&0x0F)
#define RM(ir) (ir&0x0F)

#define IMM8(ir) (ir&0xFF)
#define IMM12(ir) (ir&0xFFF)
#define SHIFTIMM(ir) ((ir>>7)0x1F)
#define IMMROT(ir) ((ir>>7)&1E)
#define SHIFT(ir) ((ir>>4)&0x07)
#define DISP24(ir) ((ir&0x00FFFFFF))
#define FSXC(ir) msrFieldMask[RN(ir)]
#define ROTIMM12(ir) ROTATE_RIGHT_LONG(IMM8(ir),IMMROT(ir))

char *conditionNames[] = { "EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", 
                           "HI", "LS", "GE", "LT", "GT", "LE", "  " /*AL*/, "NV" };
                           
                         /* fsxc */
char *msrFieldMask[] = { "", "c", "x", "xc", "s", "sc", "sx", "sxc",
	                     "f", "fc", "fx", "fxc", "fs", "fsc", "fsx", "fsxc" };

#define UNIMP(ir) snprintf( buf, len, "???     " )

int arm_disasm_instruction( int pc, char *buf, int len )
{
    uint32_t ir = arm_mem_read_long(pc);
    
    if( COND(ir) == 0x0F ) {
    	UNIMP(ir);
    	return pc+4;
    }
    char *cond = conditionNames[COND(ir)];

	switch( GRP(ir) ) {
	case 0:
		if( (ir & 0x0D900000) == 0x01000000 ) {
			/* Instructions that aren't actual data processing */
			switch( ir & 0x0FF000F0 ) {
			case 0x01200010: /* BXcc */
				snprintf(buf, len, "BX%s     R%d", cond, RM(ir));
				break;
			case 0x01000000: /* MRS Rd, CPSR */
				snprintf(buf, len, "MRS%s    R%d, CPSR", cond, RD(ir));
				break;
			case 0x01400000: /* MRS Rd, SPSR */
				snprintf(buf, len, "MRS%s    R%d, SPSR", cond, RD(ir));
				break;
			case 0x01200000: /* MSR CPSR, Rm */
				snprintf(buf, len, "MSR%s    CPSR_%s, R%d", cond, FSXC(ir), RM(ir));
				break;
			case 0x01600000: /* MSR SPSR, Rm */
				snprintf(buf, len, "MSR%s    SPSR_%s, R%d", cond, FSXC(ir), RM(ir));
				break;
			case 0x03200000: /* MSR CPSR, imm */
				snprintf(buf, len, "MSR%s    CPSR_%s, #%08X", cond, FSXC(ir), ROTIMM12(ir));
				break;
			case 0x03600000: /* MSR SPSR, imm */
				snprintf(buf, len, "MSR%s    SPSR_%s, #%08X", cond, FSXC(ir), ROTIMM12(ir));
				break;
			default:
				UNIMP();
			}
		} else if( (ir & 0x0E000090) == 0x00000090 ) {
			/* Neither are these */
			switch( (ir>>5)&0x03 ) {
			case 0:
				/* Arithmetic extension area */
				switch(OPCODE(ir)) {
				case 0: /* MUL */
					snprintf(buf,len, "MUL%s    R%d, R%d, R%d", cond, RN(ir), RM(ir), RS(ir) );
					break;
				case 1: /* MULS */
					break;
				case 2: /* MLA */
					snprintf(buf,len, "MLA%s    R%d, R%d, R%d, R%d", cond, RN(ir), RM(ir), RS(ir), RD(ir) );
					break;
				case 3: /* MLAS */
					break;
				case 8: /* UMULL */
					snprintf(buf,len, "UMULL%s  R%d, R%d, R%d, R%d", cond, RD(ir), RN(ir), RM(ir), RS(ir) );
					break;
				case 9: /* UMULLS */
					break;
				case 10: /* UMLAL */
					snprintf(buf,len, "UMLAL%s  R%d, R%d, R%d, R%d", cond, RD(ir), RN(ir), RM(ir), RS(ir) );
					break;
				case 11: /* UMLALS */
					break;
				case 12: /* SMULL */
					snprintf(buf,len, "SMULL%s  R%d, R%d, R%d, R%d", cond, RD(ir), RN(ir), RM(ir), RS(ir) );
					break;
				case 13: /* SMULLS */
					break;
				case 14: /* SMLAL */
					snprintf(buf,len, "SMLAL%s  R%d, R%d, R%d, R%d", cond, RD(ir), RN(ir), RM(ir), RS(ir) );
					break;
				case 15: /* SMLALS */

					break;
				case 16: /* SWP */
					snprintf(buf,len, "SWP%s    R%d, R%d, [R%d]", cond, RD(ir), RN(ir), RM(ir) );
					break;
				case 20: /* SWPB */
					snprintf(buf,len, "SWPB%s   R%d, R%d, [R%d]", cond, RD(ir), RN(ir), RM(ir) );
					break;
				default:
					UNIMP(ir);
				}
				break;
			case 1:
				if( LFLAG(ir) ) {
					/* LDRH */
				} else {
					/* STRH */
				}
				break;
			case 2:
				if( LFLAG(ir) ) {
					/* LDRSB */
				} else {
					UNIMP(ir);
				}
				break;
			case 3:
				if( LFLAG(ir) ) {
					/* LDRSH */
				} else {
					UNIMP(ir);
				}
				break;
			}
		} else {
			/* Data processing */

			switch(OPCODE(ir)) {
			case 0: /* AND Rd, Rn, operand */
				RD(ir) = RN(ir) & arm_get_shift_operand(ir);
				break;
			case 1: /* ANDS Rd, Rn, operand */
				operand = arm_get_shift_operand_s(ir) & RN(ir);
				RD(ir) = operand;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = operand>>31;
					armr.z = (operand == 0);
					armr.c = armr.shift_c;
				}
				break;
			case 2: /* EOR Rd, Rn, operand */
				RD(ir) = RN(ir) ^ arm_get_shift_operand(ir);
				break;
			case 3: /* EORS Rd, Rn, operand */
				operand = arm_get_shift_operand_s(ir) ^ RN(ir);
				RD(ir) = operand;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = operand>>31;
					armr.z = (operand == 0);
					armr.c = armr.shift_c;
				}
				break;
			case 4: /* SUB Rd, Rn, operand */
				RD(ir) = RN(ir) - arm_get_shift_operand(ir);
				break;
			case 5: /* SUBS Rd, Rn, operand */
			    operand = RN(ir);
				operand2 = arm_get_shift_operand(ir)
				tmp = operand - operand2;
				RD(ir) = tmp;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = tmp>>31;
					armr.z = (tmp == 0);
					armr.c = IS_NOTBORROW(tmp,operand,operand2);
					armr.v = IS_SUBOVERFLOW(tmp,operand,operand2);
				}
				break;
			case 6: /* RSB Rd, operand, Rn */
				RD(ir) = arm_get_shift_operand(ir) - RN(ir);
				break;
			case 7: /* RSBS Rd, operand, Rn */
				operand = arm_get_shift_operand(ir);
			    operand2 = RN(ir);
				tmp = operand - operand2;
				RD(ir) = tmp;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = tmp>>31;
					armr.z = (tmp == 0);
					armr.c = IS_NOTBORROW(tmp,operand,operand2);
					armr.v = IS_SUBOVERFLOW(tmp,operand,operand2);
				}
				break;
			case 8: /* ADD Rd, Rn, operand */
				RD(ir) = RN(ir) + arm_get_shift_operand(ir);
				break;
			case 9: /* ADDS Rd, Rn, operand */
				operand = arm_get_shift_operand(ir);
			    operand2 = RN(ir);
				tmp = operand + operand2
				RD(ir) = tmp;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = tmp>>31;
					armr.z = (tmp == 0);
					armr.c = IS_CARRY(tmp,operand,operand2);
					armr.v = IS_ADDOVERFLOW(tmp,operand,operand2);
				}
				break;			
			case 10: /* ADC */
			case 11: /* ADCS */
			case 12: /* SBC */
			case 13: /* SBCS */
			case 14: /* RSC */
			case 15: /* RSCS */
				break;
			case 17: /* TST Rn, operand */
				operand = arm_get_shift_operand_s(ir) & RN(ir);
				armr.n = operand>>31;
				armr.z = (operand == 0);
				armr.c = armr.shift_c;
				break;
			case 19: /* TEQ Rn, operand */
				operand = arm_get_shift_operand_s(ir) ^ RN(ir);
				armr.n = operand>>31;
				armr.z = (operand == 0);
				armr.c = armr.shift_c;
				break;				
			case 21: /* CMP Rn, operand */
			    operand = RN(ir);
				operand2 = arm_get_shift_operand(ir)
				tmp = operand - operand2;
				armr.n = tmp>>31;
				armr.z = (tmp == 0);
				armr.c = IS_NOTBORROW(tmp,operand,operand2);
				armr.v = IS_SUBOVERFLOW(tmp,operand,operand2);
				break;
			case 23: /* CMN Rn, operand */
			    operand = RN(ir);
				operand2 = arm_get_shift_operand(ir)
				tmp = operand + operand2;
				armr.n = tmp>>31;
				armr.z = (tmp == 0);
				armr.c = IS_CARRY(tmp,operand,operand2);
				armr.v = IS_ADDOVERFLOW(tmp,operand,operand2);
				break;
			case 24: /* ORR Rd, Rn, operand */
				RD(ir) = RN(ir) | arm_get_shift_operand(ir);
				break;
			case 25: /* ORRS Rd, Rn, operand */
				operand = arm_get_shift_operand_s(ir) | RN(ir);
				RD(ir) = operand;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = operand>>31;
					armr.z = (operand == 0);
					armr.c = armr.shift_c;
				}
				break;
			case 26: /* MOV Rd, operand */
				RD(ir) = arm_get_shift_operand(ir);
				break;
			case 27: /* MOVS Rd, operand */
				operand = arm_get_shift_operand_s(ir);
				RD(ir) = operand;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = operand>>31;
					armr.z = (operand == 0);
					armr.c = armr.shift_c;
				}
				break;
			case 28: /* BIC Rd, Rn, operand */
				RD(ir) = RN(ir) & (~arm_get_shift_operand(ir));
				break;
			case 29: /* BICS Rd, Rn, operand */
				operand = RN(ir) & (~arm_get_shift_operand_s(ir));
				RD(ir) = operand;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = operand>>31;
					armr.z = (operand == 0);
					armr.c = armr.shift_c;
				}
				break;
			case 30: /* MVN Rd, operand */
				RD(ir) = ~arm_get_shift_operand(ir);
				break;
			case 31: /* MVNS Rd, operand */
				operand = ~arm_get_shift_operand_s(ir);
				RD(ir) = operand;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = operand>>31;
					armr.z = (operand == 0);
					armr.c = armr.shift_c;
				}
				break;
			default:
				UNIMP(ir);
			}
		}
		break;
	case 1: /* Load/store */
		break;
	case 2: /* Load/store multiple, branch*/
		break;
	case 3: /* Copro */
		break;
	}
	
	
	
	return pc+4;
}
