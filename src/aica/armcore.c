
#include "aica/armcore.h"

struct arm_registers armr;

/* NB: The arm has a different memory map, but for the meantime... */
/* Page references are as per ARM DDI 0100E (June 2000) */

#define MEM_READ_BYTE( addr ) arm_read_byte(addr)
#define MEM_READ_WORD( addr ) arm_read_word(addr)
#define MEM_READ_LONG( addr ) arm_read_long(addr)
#define MEM_WRITE_BYTE( addr, val ) arm_write_byte(addr, val)
#define MEM_WRITE_WORD( addr, val ) arm_write_word(addr, val)
#define MEM_WRITE_LONG( addr, val ) arm_write_long(addr, val)


#define IS_NOTBORROW( result, op1, op2 ) (op2 > op1 ? 0 : 1)
#define IS_CARRY( result, op1, op2 ) (result < op1 ? 1 : 0)
#define IS_SUBOVERFLOW( result, op1, op2 ) (((op1^op2) & (result^op1)) >> 31)
#define IS_ADDOVERFLOW( result, op1, op2 ) (((op1&op2) & (result^op1)) >> 31)

#define PC armr.r[15]

/* Instruction fields */
#define COND(ir) (ir>>28)
#define GRP(ir) ((ir>>26)&0x03)
#define OPCODE(ir) ((ir>>20)&0x1F)
#define IFLAG(ir) (ir&0x02000000)
#define SFLAG(ir) (ir&0x00100000)
#define PFLAG(ir) (ir&0x01000000)
#define UFLAG(ir) (ir&0x00800000)
#define BFLAG(ir) (ir&0x00400000)
#define WFLAG(ir) (IR&0x00200000)
#define LFLAG(ir) SFLAG(ir)
#define RN(ir) (armr.r[((ir>>16)&0x0F)] + (((ir>>16)&0x0F) == 0x0F ? 4 : 0))
#define RD(ir) (armr.r[((ir>>12)&0x0F)] + (((ir>>16)&0x0F) == 0x0F ? 4 : 0))
#define RDn(ir) ((ir>>12)&0x0F)
#define RS(ir) (armr.r[((ir>>8)&0x0F)] + (((ir>>16)&0x0F) == 0x0F ? 4 : 0))
#define RM(ir) (armr.r[(ir&0x0F)] + (((ir>>16)&0x0F) == 0x0F ? 4 : 0))
#define LRN(ir) armr.r[((ir>>16)&0x0F)]
#define LRD(ir) armr.r[((ir>>12)&0x0F)]
#define LRS(ir) armr.r[((ir>>8)&0x0F)]
#define LRM(ir) armr.r[(ir&0x0F)]

#define IMM8(ir) (ir&0xFF)
#define IMM12(ir) (ir&0xFFF)
#define SHIFTIMM(ir) ((ir>>7)&0x1F)
#define IMMROT(ir) ((ir>>7)&0x1E)
#define SHIFT(ir) ((ir>>4)&0x07)
#define DISP24(ir) ((ir&0x00FFFFFF))
#define UNDEF(ir) do{ ERROR( "Raising exception on undefined instruction at %08x, opcode = %04x", PC, ir ); return; } while(0)
#define UNIMP(ir) do{ ERROR( "Halted on unimplemented instruction at %08x, opcode = %04x", PC, ir ); return; }while(0)

void arm_restore_cpsr()
{

}

static uint32_t arm_get_shift_operand( uint32_t ir )
{
	uint32_t operand, tmp;
	if( IFLAG(ir) == 0 ) {
		operand = RM(ir);
		switch(SHIFT(ir)) {
		case 0: /* (Rm << imm) */
			operand = operand << SHIFTIMM(ir);
			break;
		case 1: /* (Rm << Rs) */
			tmp = RS(ir)&0xFF;
			if( tmp > 31 ) operand = 0;
			else operand = operand << tmp;
			break;
		case 2: /* (Rm >> imm) */
			operand = operand >> SHIFTIMM(ir);
			break;
		case 3: /* (Rm >> Rs) */
			tmp = RS(ir) & 0xFF;
			if( tmp > 31 ) operand = 0;
			else operand = operand >> ir;
			break;
		case 4: /* (Rm >>> imm) */
			tmp = SHIFTIMM(ir);
			if( tmp == 0 ) operand = ((int32_t)operand) >> 31;
			else operand = ((int32_t)operand) >> tmp;
			break;
		case 5: /* (Rm >>> Rs) */
			tmp = RS(ir) & 0xFF;
			if( tmp > 31 ) operand = ((int32_t)operand) >> 31;
			else operand = ((int32_t)operand) >> tmp;
			break;
		case 6:
			tmp = SHIFTIMM(ir);
			if( tmp == 0 ) /* RRX aka rotate with carry */
				operand = (operand >> 1) | (armr.c<<31);
			else
				operand = ROTATE_RIGHT_LONG(operand,tmp);
			break;
		case 7:
			tmp = RS(ir)&0x1F;
			operand = ROTATE_RIGHT_LONG(operand,tmp);
			break;
		}
	} else {
		operand = IMM8(ir);
		tmp = IMMROT(ir);
		operand = ROTATE_RIGHT_LONG(operand, tmp);
	}
	return operand;
}

/**
 * Compute the "shift operand" of the instruction for the data processing
 * instructions. This variant also sets armr.shift_c (carry result for shifter)
 * Reason for the variants is that most cases don't actually need the shift_c.
 */
static uint32_t arm_get_shift_operand_s( uint32_t ir )
{
	uint32_t operand, tmp;
	if( IFLAG(ir) == 0 ) {
		operand = RM(ir);
		switch(SHIFT(ir)) {
		case 0: /* (Rm << imm) */
			tmp = SHIFTIMM(ir);
			if( tmp == 0 ) { /* Rm */
				armr.shift_c = armr.c;
			} else { /* Rm << imm */
				armr.shift_c = (operand >> (32-tmp)) & 0x01;
				operand = operand << tmp;
			}
			break;
		case 1: /* (Rm << Rs) */
			tmp = RS(ir)&0xFF;
			if( tmp == 0 ) {
				armr.shift_c = armr.c;
			} else {
				if( tmp <= 32 )
					armr.shift_c = (operand >> (32-tmp)) & 0x01;
				else armr.shift_c = 0;
				if( tmp < 32 )
					operand = operand << tmp;
				else operand = 0;
			}
			break;
		case 2: /* (Rm >> imm) */
			tmp = SHIFTIMM(ir);
			if( tmp == 0 ) {
				armr.shift_c = operand >> 31;
				operand = 0;
			} else {
				armr.shift_c = (operand >> (tmp-1)) & 0x01;
				operand = RM(ir) >> tmp;
			}
			break;
		case 3: /* (Rm >> Rs) */
			tmp = RS(ir) & 0xFF;
			if( tmp == 0 ) {
				armr.shift_c = armr.c;
			} else {
				if( tmp <= 32 )
					armr.shift_c = (operand >> (tmp-1))&0x01;
				else armr.shift_c = 0;
				if( tmp < 32 )
					operand = operand >> tmp;
				else operand = 0;
			}
			break;
		case 4: /* (Rm >>> imm) */
			tmp = SHIFTIMM(ir);
			if( tmp == 0 ) {
				armr.shift_c = operand >> 31;
				operand = -armr.shift_c;
			} else {
				armr.shift_c = (operand >> (tmp-1)) & 0x01;
				operand = ((int32_t)operand) >> tmp;
			}
			break;
		case 5: /* (Rm >>> Rs) */
			tmp = RS(ir) & 0xFF;
			if( tmp == 0 ) {
				armr.shift_c = armr.c;
			} else {
				if( tmp < 32 ) {
					armr.shift_c = (operand >> (tmp-1))&0x01;
					operand = ((int32_t)operand) >> tmp;
				} else {
					armr.shift_c = operand >> 31;
					operand = ((int32_t)operand) >> 31;
				}
			}
			break;
		case 6:
			tmp = SHIFTIMM(ir);
			if( tmp == 0 ) { /* RRX aka rotate with carry */
				armr.shift_c = operand&0x01;
				operand = (operand >> 1) | (armr.c<<31);
			} else {
				armr.shift_c = operand>>(tmp-1);
				operand = ROTATE_RIGHT_LONG(operand,tmp);
			}
			break;
		case 7:
			tmp = RS(ir)&0xFF;
			if( tmp == 0 ) {
				armr.shift_c = armr.c;
			} else {
				tmp &= 0x1F;
				if( tmp == 0 ) {
					armr.shift_c = operand>>31;
				} else {
					armr.shift_c = (operand>>(tmp-1))&0x1;
					operand = ROTATE_RIGHT_LONG(operand,tmp);
				}
			}
			break;
		}
	} else {
		operand = IMM8(ir);
		tmp = IMMROT(ir);
		if( tmp == 0 ) {
			armr.shift_c = armr.c;
		} else {
			operand = ROTATE_RIGHT_LONG(operand, tmp);
			armr.shift_c = operand>>31;
		}
	}
	return operand;
}

/**
 * Another variant of the shifter code for index-based memory addressing.
 * Distinguished by the fact that it doesn't support register shifts, and
 * ignores the I flag (WTF do the load/store instructions use the I flag to
 * mean the _exact opposite_ of what it means for the data processing 
 * instructions ???)
 */
static uint32_t arm_get_address_index( uint32_t ir )
{
	uint32_t operand = RM(ir);
	uint32_t tmp;
	
	switch(SHIFT(ir)) {
	case 0: /* (Rm << imm) */
		operand = operand << SHIFTIMM(ir);
		break;
	case 2: /* (Rm >> imm) */
		operand = operand >> SHIFTIMM(ir);
		break;
	case 4: /* (Rm >>> imm) */
		tmp = SHIFTIMM(ir);
		if( tmp == 0 ) operand = ((int32_t)operand) >> 31;
		else operand = ((int32_t)operand) >> tmp;
		break;
	case 6:
		tmp = SHIFTIMM(ir);
		if( tmp == 0 ) /* RRX aka rotate with carry */
			operand = (operand >> 1) | (armr.c<<31);
		else
			operand = ROTATE_RIGHT_LONG(operand,tmp);
		break;
	default: UNIMP(ir);
	}
	return operand;	
}

static uint32_t arm_get_address_operand( uint32_t ir )
{
	uint32_t addr;
	
	/* I P U . W */
	switch( (ir>>21)&0x1D ) {
	case 0: /* Rn -= imm offset (post-indexed) [5.2.8 A5-28] */
	case 1:
		addr = RN(ir);
		LRN(ir) = addr - IMM12(ir);
		break;
	case 4: /* Rn += imm offsett (post-indexed) [5.2.8 A5-28] */
	case 5:
		addr = RN(ir);
		LRN(ir) = addr + IMM12(ir);
		break;
	case 8: /* Rn - imm offset  [5.2.2 A5-20] */
		addr = RN(ir) - IMM12(ir);
		break;
	case 9: /* Rn -= imm offset (pre-indexed)  [5.2.5 A5-24] */
		addr = RN(ir) - IMM12(ir);
		LRN(ir) = addr;
		break;
	case 12: /* Rn + imm offset  [5.2.2 A5-20] */
		addr = RN(ir) + IMM12(ir);
		break;
	case 13: /* Rn += imm offset  [5.2.5 A5-24 ] */
		addr = RN(ir) + IMM12(ir);
		LRN(ir) = addr;
		break;
	case 16: /* Rn -= Rm (post-indexed)  [5.2.10 A5-32 ] */
	case 17:
		addr = RN(ir);
		LRN(ir) = addr - arm_get_address_index(ir);
		break;
	case 20: /* Rn += Rm (post-indexed)  [5.2.10 A5-32 ] */
	case 21:
		addr = RN(ir);
		LRN(ir) = addr - arm_get_address_index(ir);
		break;
	case 24: /* Rn - Rm  [5.2.4 A5-23] */
		addr = RN(ir) - arm_get_address_index(ir);
		break;
	case 25: /* RN -= Rm (pre-indexed)  [5.2.7 A5-26] */
		addr = RN(ir) - arm_get_address_index(ir);
		LRN(ir) = addr;
		break;
	case 28: /* Rn + Rm  [5.2.4 A5-23] */
		addr = RN(ir) + arm_get_address_index(ir);
		break;
	case 29: /* RN += Rm (pre-indexed) [5.2.7 A5-26] */
		addr = RN(ir) + arm_get_address_index(ir);
		LRN(ir) = addr;
		break;
	default:
		UNIMP(ir); /* Unreachable */
	}
	return addr;
}

void arm_execute_instruction( void ) 
{
	uint32_t pc = PC;
	uint32_t ir = MEM_READ_LONG(pc);
	uint32_t operand, operand2, tmp, cond;

	pc += 4;
	PC = pc;

	switch( COND(ir) ) {
		case 0: /* EQ */ 
			cond = armr.z;
			break;
		case 1: /* NE */
			cond = !armr.z;
			break;
		case 2: /* CS/HS */
			cond = armr.c;
			break;
		case 3: /* CC/LO */
			cond = !armr.c;
			break;
		case 4: /* MI */
			cond = armr.n;
			break;
		case 5: /* PL */
			cond = !armr.n;
			break;
		case 6: /* VS */
			cond = armr.v;
			break;
		case 7: /* VC */
			cond = !armr.v;
			break;
		case 8: /* HI */
			cond = armr.c && !armr.z;
			break;
		case 9: /* LS */
			cond = (!armr.c) || armr.z;
			break;
		case 10: /* GE */
			cond = (armr.n == armr.v);
			break;
		case 11: /* LT */
			cond = (armr.n != armr.v);
			break;
		case 12: /* GT */
			cond = (!armr.z) && (armr.n == armr.v);
			break;
		case 13: /* LE */
			cond = armr.z || (armr.n != armr.v);
			break;
		case 14: /* AL */
			cond = 1;
			break;
		case 15: /* (NV) */
			cond = 0;
			UNDEF(ir);
	}

	switch( GRP(ir) ) {
	case 0:
		if( (ir & 0x0D900000) == 0x01000000 ) {
			/* Instructions that aren't actual data processing */
			switch( ir & 0x0FF000F0 ) {
			case 0x01200010: /* BX */
				break;
			case 0x01000000: /* MRS Rd, CPSR */
				break;
			case 0x01400000: /* MRS Rd, SPSR */
				break;
			case 0x01200000: /* MSR CPSR, Rd */
				break;
			case 0x01600000: /* MSR SPSR, Rd */
				break;
			case 0x03200000: /* MSR CPSR, imm */
				break;
			case 0x03600000: /* MSR SPSR, imm */
				break;
			default:
				UNIMP(ir);
			}
		} else if( (ir & 0x0E000090) == 0x00000090 ) {
			/* Neither are these */
			switch( (ir>>5)&0x03 ) {
			case 0:
				/* Arithmetic extension area */
				switch(OPCODE(ir)) {
				case 0: /* MUL */
					break;
				case 1: /* MULS */
					break;
				case 2: /* MLA */
					break;
				case 3: /* MLAS */
					break;
				case 8: /* UMULL */
					break;
				case 9: /* UMULLS */
					break;
				case 10: /* UMLAL */
					break;
				case 11: /* UMLALS */
					break;
				case 12: /* SMULL */
					break;
				case 13: /* SMULLS */
					break;
				case 14: /* SMLAL */
					break;
				case 15: /* SMLALS */
					break;
				case 16: /* SWP */
					break;
				case 20: /* SWPB */
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
				LRD(ir) = RN(ir) & arm_get_shift_operand(ir);
				break;
			case 1: /* ANDS Rd, Rn, operand */
				operand = arm_get_shift_operand_s(ir) & RN(ir);
				LRD(ir) = operand;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = operand>>31;
					armr.z = (operand == 0);
					armr.c = armr.shift_c;
				}
				break;
			case 2: /* EOR Rd, Rn, operand */
				LRD(ir) = RN(ir) ^ arm_get_shift_operand(ir);
				break;
			case 3: /* EORS Rd, Rn, operand */
				operand = arm_get_shift_operand_s(ir) ^ RN(ir);
				LRD(ir) = operand;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = operand>>31;
					armr.z = (operand == 0);
					armr.c = armr.shift_c;
				}
				break;
			case 4: /* SUB Rd, Rn, operand */
				LRD(ir) = RN(ir) - arm_get_shift_operand(ir);
				break;
			case 5: /* SUBS Rd, Rn, operand */
			    operand = RN(ir);
				operand2 = arm_get_shift_operand(ir);
				tmp = operand - operand2;
				LRD(ir) = tmp;
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
				LRD(ir) = arm_get_shift_operand(ir) - RN(ir);
				break;
			case 7: /* RSBS Rd, operand, Rn */
				operand = arm_get_shift_operand(ir);
			    operand2 = RN(ir);
				tmp = operand - operand2;
				LRD(ir) = tmp;
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
				LRD(ir) = RN(ir) + arm_get_shift_operand(ir);
				break;
			case 9: /* ADDS Rd, Rn, operand */
				operand = arm_get_shift_operand(ir);
			    operand2 = RN(ir);
				tmp = operand + operand2;
				LRD(ir) = tmp;
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
				operand2 = arm_get_shift_operand(ir);
				tmp = operand - operand2;
				armr.n = tmp>>31;
				armr.z = (tmp == 0);
				armr.c = IS_NOTBORROW(tmp,operand,operand2);
				armr.v = IS_SUBOVERFLOW(tmp,operand,operand2);
				break;
			case 23: /* CMN Rn, operand */
			    operand = RN(ir);
				operand2 = arm_get_shift_operand(ir);
				tmp = operand + operand2;
				armr.n = tmp>>31;
				armr.z = (tmp == 0);
				armr.c = IS_CARRY(tmp,operand,operand2);
				armr.v = IS_ADDOVERFLOW(tmp,operand,operand2);
				break;
			case 24: /* ORR Rd, Rn, operand */
				LRD(ir) = RN(ir) | arm_get_shift_operand(ir);
				break;
			case 25: /* ORRS Rd, Rn, operand */
				operand = arm_get_shift_operand_s(ir) | RN(ir);
				LRD(ir) = operand;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = operand>>31;
					armr.z = (operand == 0);
					armr.c = armr.shift_c;
				}
				break;
			case 26: /* MOV Rd, operand */
				LRD(ir) = arm_get_shift_operand(ir);
				break;
			case 27: /* MOVS Rd, operand */
				operand = arm_get_shift_operand_s(ir);
				LRD(ir) = operand;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = operand>>31;
					armr.z = (operand == 0);
					armr.c = armr.shift_c;
				}
				break;
			case 28: /* BIC Rd, Rn, operand */
				LRD(ir) = RN(ir) & (~arm_get_shift_operand(ir));
				break;
			case 29: /* BICS Rd, Rn, operand */
				operand = RN(ir) & (~arm_get_shift_operand_s(ir));
				LRD(ir) = operand;
				if( RDn(ir) == 15 ) {
					arm_restore_cpsr();
				} else {
					armr.n = operand>>31;
					armr.z = (operand == 0);
					armr.c = armr.shift_c;
				}
				break;
			case 30: /* MVN Rd, operand */
				LRD(ir) = ~arm_get_shift_operand(ir);
				break;
			case 31: /* MVNS Rd, operand */
				operand = ~arm_get_shift_operand_s(ir);
				LRD(ir) = operand;
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
}
