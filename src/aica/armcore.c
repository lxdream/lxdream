/**
 * $Id: armcore.c,v 1.17 2006-01-12 11:30:19 nkeynes Exp $
 * 
 * ARM7TDMI CPU emulation core.
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

#define MODULE aica_module
#include "dream.h"
#include "mem.h"
#include "aica/armcore.h"
#include "aica/aica.h"

#define STM_R15_OFFSET 12

struct arm_registers armr;

void arm_set_mode( int mode );

uint32_t arm_exceptions[][2] = {{ MODE_SVC, 0x00000000 },
				{ MODE_UND, 0x00000004 },
				{ MODE_SVC, 0x00000008 },
				{ MODE_ABT, 0x0000000C },
				{ MODE_ABT, 0x00000010 },
				{ MODE_IRQ, 0x00000018 },
				{ MODE_FIQ, 0x0000001C } };

#define EXC_RESET 0
#define EXC_UNDEFINED 1
#define EXC_SOFTWARE 2
#define EXC_PREFETCH_ABORT 3
#define EXC_DATA_ABORT 4
#define EXC_IRQ 5
#define EXC_FAST_IRQ 6

uint32_t arm_cpu_freq = ARM_BASE_RATE;
uint32_t arm_cpu_period = 1000 / ARM_BASE_RATE;

#define CYCLES_PER_SAMPLE ((ARM_BASE_RATE * 1000000) / AICA_SAMPLE_RATE)

static struct breakpoint_struct arm_breakpoints[MAX_BREAKPOINTS];
static int arm_breakpoint_count = 0;

void arm_set_breakpoint( uint32_t pc, int type )
{
    arm_breakpoints[arm_breakpoint_count].address = pc;
    arm_breakpoints[arm_breakpoint_count].type = type;
    arm_breakpoint_count++;
}

gboolean arm_clear_breakpoint( uint32_t pc, int type )
{
    int i;

    for( i=0; i<arm_breakpoint_count; i++ ) {
	if( arm_breakpoints[i].address == pc && 
	    arm_breakpoints[i].type == type ) {
	    while( ++i < arm_breakpoint_count ) {
		arm_breakpoints[i-1].address = arm_breakpoints[i].address;
		arm_breakpoints[i-1].type = arm_breakpoints[i].type;
	    }
	    arm_breakpoint_count--;
	    return TRUE;
	}
    }
    return FALSE;
}

int arm_get_breakpoint( uint32_t pc )
{
    int i;
    for( i=0; i<arm_breakpoint_count; i++ ) {
	if( arm_breakpoints[i].address == pc )
	    return arm_breakpoints[i].type;
    }
    return 0;
}

#define IS_TIMER_ENABLED() (MMIO_READ( AICA2, AICA_TCR ) & 0x40)

uint32_t arm_run_slice( uint32_t num_samples )
{
    int i,j,k;
    for( i=0; i<num_samples; i++ ) {
	for( j=0; j < CYCLES_PER_SAMPLE; j++ ) {
	    armr.icount++;
	    if( !arm_execute_instruction() )
		return i;
#ifdef ENABLE_DEBUG_MODE
	    for( k=0; k<arm_breakpoint_count; k++ ) {
		if( arm_breakpoints[k].address == armr.r[15] ) {
		    dreamcast_stop();
		    if( arm_breakpoints[k].type == BREAK_ONESHOT )
			arm_clear_breakpoint( armr.r[15], BREAK_ONESHOT );
		    return i;
		}
	    }
#endif	
	}

	if( IS_TIMER_ENABLED() ) {
	    uint8_t val = MMIO_READ( AICA2, AICA_TIMER );
	    val++;
	    if( val == 0 )
		aica_event( AICA_EVENT_TIMER );
	    MMIO_WRITE( AICA2, AICA_TIMER, val );
	}
	if( !dreamcast_is_running() )
	    break;
    }

    return i;
}

void arm_save_state( FILE *f )
{
    fwrite( &armr, sizeof(armr), 1, f );
}

int arm_load_state( FILE *f )
{
    fread( &armr, sizeof(armr), 1, f );
    return 0;
}

/* Exceptions */
void arm_reset( void )
{
    /* Wipe all processor state */
    memset( &armr, 0, sizeof(armr) );

    armr.cpsr = MODE_SVC | CPSR_I | CPSR_F;
    armr.r[15] = 0x00000000;
}

#define SET_CPSR_CONTROL   0x00010000
#define SET_CPSR_EXTENSION 0x00020000
#define SET_CPSR_STATUS    0x00040000
#define SET_CPSR_FLAGS     0x00080000

uint32_t arm_get_cpsr( void )
{
    /* write back all flags to the cpsr */
    armr.cpsr = armr.cpsr & CPSR_COMPACT_MASK;
    if( armr.n ) armr.cpsr |= CPSR_N;
    if( armr.z ) armr.cpsr |= CPSR_Z;
    if( armr.c ) armr.cpsr |= CPSR_C;
    if( armr.v ) armr.cpsr |= CPSR_V;
    if( armr.t ) armr.cpsr |= CPSR_T;  
    return armr.cpsr;
}

/**
 * Return a pointer to the specified register in the user bank,
 * regardless of the active bank
 */
static uint32_t *arm_user_reg( int reg )
{
    if( IS_EXCEPTION_MODE() ) {
	if( reg == 13 || reg == 14 )
	    return &armr.user_r[reg-8];
	if( IS_FIQ_MODE() ) {
	    if( reg >= 8 || reg <= 12 )
		return &armr.user_r[reg-8];
	}
    }
    return &armr.r[reg];
}

#define USER_R(n) *arm_user_reg(n)

/**
 * Set the CPSR to the specified value.
 *
 * @param value values to set in CPSR
 * @param fields set of mask values to define which sections of the 
 *   CPSR to set (one of the SET_CPSR_* values above)
 */
void arm_set_cpsr( uint32_t value, uint32_t fields )
{
    if( IS_PRIVILEGED_MODE() ) {
	if( fields & SET_CPSR_CONTROL ) {
	    int mode = value & CPSR_MODE;
	    arm_set_mode( mode );
	    armr.t = ( value & CPSR_T ); /* Technically illegal to change */
	    armr.cpsr = (armr.cpsr & 0xFFFFFF00) | (value & 0x000000FF);
	}

	/* Middle 16 bits not currently defined */
    }
    if( fields & SET_CPSR_FLAGS ) {
	/* Break flags directly out of given value - don't bother writing
	 * back to CPSR 
	 */
	armr.n = ( value & CPSR_N );
	armr.z = ( value & CPSR_Z );
	armr.c = ( value & CPSR_C );
	armr.v = ( value & CPSR_V );
    }
}

void arm_set_spsr( uint32_t value, uint32_t fields )
{
    /* Only defined if we actually have an SPSR register */
    if( IS_EXCEPTION_MODE() ) {
	if( fields & SET_CPSR_CONTROL ) {
	    armr.spsr = (armr.spsr & 0xFFFFFF00) | (value & 0x000000FF);
	}

	/* Middle 16 bits not currently defined */

	if( fields & SET_CPSR_FLAGS ) {
	    armr.spsr = (armr.spsr & 0x00FFFFFF) | (value & 0xFF000000);
	}
    }
}

/**
 * Raise an ARM exception (other than reset, which uses arm_reset().
 * @param exception one of the EXC_* exception codes defined above.
 */
void arm_raise_exception( int exception )
{
    int mode = arm_exceptions[exception][0];
    uint32_t spsr = arm_get_cpsr();
    arm_set_mode( mode );
    armr.spsr = spsr;
    armr.r[14] = armr.r[15] + 4;
    armr.cpsr = (spsr & 0xFFFFFF00) | mode | CPSR_I; 
    if( mode == MODE_FIQ )
	armr.cpsr |= CPSR_F;
    armr.r[15] = arm_exceptions[exception][1];
}

void arm_restore_cpsr( void )
{
    int spsr = armr.spsr;
    int mode = spsr & CPSR_MODE;
    arm_set_mode( mode );
    armr.cpsr = spsr;
    armr.n = ( spsr & CPSR_N );
    armr.z = ( spsr & CPSR_Z );
    armr.c = ( spsr & CPSR_C );
    armr.v = ( spsr & CPSR_V );
    armr.t = ( spsr & CPSR_T );
}



/**
 * Change the current executing ARM mode to the requested mode.
 * Saves any required registers to banks and restores those for the
 * correct mode. (Note does not actually update CPSR at the moment).
 */
void arm_set_mode( int targetMode )
{
    int currentMode = armr.cpsr & CPSR_MODE;
    if( currentMode == targetMode )
	return;

    switch( currentMode ) {
    case MODE_USER:
    case MODE_SYS:
	armr.user_r[5] = armr.r[13];
	armr.user_r[6] = armr.r[14];
	break;
    case MODE_SVC:
	armr.svc_r[0] = armr.r[13];
	armr.svc_r[1] = armr.r[14];
	armr.svc_r[2] = armr.spsr;
	break;
    case MODE_ABT:
	armr.abt_r[0] = armr.r[13];
	armr.abt_r[1] = armr.r[14];
	armr.abt_r[2] = armr.spsr;
	break;
    case MODE_UND:
	armr.und_r[0] = armr.r[13];
	armr.und_r[1] = armr.r[14];
	armr.und_r[2] = armr.spsr;
	break;
    case MODE_IRQ:
	armr.irq_r[0] = armr.r[13];
	armr.irq_r[1] = armr.r[14];
	armr.irq_r[2] = armr.spsr;
	break;
    case MODE_FIQ:
	armr.fiq_r[0] = armr.r[8];
	armr.fiq_r[1] = armr.r[9];
	armr.fiq_r[2] = armr.r[10];
	armr.fiq_r[3] = armr.r[11];
	armr.fiq_r[4] = armr.r[12];
	armr.fiq_r[5] = armr.r[13];
	armr.fiq_r[6] = armr.r[14];
	armr.fiq_r[7] = armr.spsr;
	armr.r[8] = armr.user_r[0];
	armr.r[9] = armr.user_r[1];
	armr.r[10] = armr.user_r[2];
	armr.r[11] = armr.user_r[3];
	armr.r[12] = armr.user_r[4];
	break;
    }
    
    switch( targetMode ) {
    case MODE_USER:
    case MODE_SYS:
	armr.r[13] = armr.user_r[5];
	armr.r[14] = armr.user_r[6];
	break;
    case MODE_SVC:
	armr.r[13] = armr.svc_r[0];
	armr.r[14] = armr.svc_r[1];
	armr.spsr = armr.svc_r[2];
	break;
    case MODE_ABT:
	armr.r[13] = armr.abt_r[0];
	armr.r[14] = armr.abt_r[1];
	armr.spsr = armr.abt_r[2];
	break;
    case MODE_UND:
	armr.r[13] = armr.und_r[0];
	armr.r[14] = armr.und_r[1];
	armr.spsr = armr.und_r[2];
	break;
    case MODE_IRQ:
	armr.r[13] = armr.irq_r[0];
	armr.r[14] = armr.irq_r[1];
	armr.spsr = armr.irq_r[2];
	break;
    case MODE_FIQ:
	armr.user_r[0] = armr.r[8];
	armr.user_r[1] = armr.r[9];
	armr.user_r[2] = armr.r[10];
	armr.user_r[3] = armr.r[11];
	armr.user_r[4] = armr.r[12];
	armr.r[8] = armr.fiq_r[0];
	armr.r[9] = armr.fiq_r[1];
	armr.r[10] = armr.fiq_r[2];
	armr.r[11] = armr.fiq_r[3];
	armr.r[12] = armr.fiq_r[4];
	armr.r[13] = armr.fiq_r[5];
	armr.r[14] = armr.fiq_r[6];
	armr.spsr = armr.fiq_r[7];
	break;
    }
}

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
#define WFLAG(ir) (ir&0x00200000)
#define LFLAG(ir) SFLAG(ir)
#define RN(ir) (armr.r[((ir>>16)&0x0F)] + (((ir>>16)&0x0F) == 0x0F ? 4 : 0))
#define RD(ir) (armr.r[((ir>>12)&0x0F)] + (((ir>>12)&0x0F) == 0x0F ? 4 : 0))
#define RDn(ir) ((ir>>12)&0x0F)
#define RS(ir) (armr.r[((ir>>8)&0x0F)] + (((ir>>8)&0x0F) == 0x0F ? 4 : 0))
#define RM(ir) (armr.r[(ir&0x0F)] + (((ir&0x0F) == 0x0F ? 4 : 0)) )
#define LRN(ir) armr.r[((ir>>16)&0x0F)]
#define LRD(ir) armr.r[((ir>>12)&0x0F)]
#define LRS(ir) armr.r[((ir>>8)&0x0F)]
#define LRM(ir) armr.r[(ir&0x0F)]

#define IMM8(ir) (ir&0xFF)
#define IMM12(ir) (ir&0xFFF)
#define SHIFTIMM(ir) ((ir>>7)&0x1F)
#define IMMROT(ir) ((ir>>7)&0x1E)
#define ROTIMM12(ir) ROTATE_RIGHT_LONG(IMM8(ir),IMMROT(ir))
#define SIGNEXT24(n) ((n&0x00800000) ? (n|0xFF000000) : (n&0x00FFFFFF))
#define SHIFT(ir) ((ir>>4)&0x07)
#define DISP24(ir) ((ir&0x00FFFFFF))
#define UNDEF(ir) do{ arm_raise_exception( EXC_UNDEFINED ); return TRUE; } while(0)
#define UNIMP(ir) do{ PC-=4; ERROR( "Halted on unimplemented instruction at %08x, opcode = %04x", PC, ir ); dreamcast_stop(); return FALSE; }while(0)

/**
 * Determine the value of the shift-operand for a data processing instruction,
 * without determing a value for shift_C (optimized form for instructions that
 * don't require shift_C ).
 * @see s5.1 Addressing Mode 1 - Data-processing operands (p A5-2, 218)
 */
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
 * Determine the value of the shift-operand for a data processing instruction,
 * and set armr.shift_c accordingly.
 * @see s5.1 Addressing Mode 1 - Data-processing operands (p A5-2, 218)
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

/**
 * Determine the address operand of a load/store instruction, including
 * applying any pre/post adjustments to the address registers.
 * @see s5.2 Addressing Mode 2 - Load and Store Word or Unsigned Byte
 * @param The instruction word.
 * @return The calculated address
 */
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
	}
	return addr;
}

gboolean arm_execute_instruction( void ) 
{
    uint32_t pc;
    uint32_t ir;
    uint32_t operand, operand2, tmp, tmp2, cond;
    int i;

    tmp = armr.int_pending & (~armr.cpsr);
    if( tmp ) {
	if( tmp & CPSR_F ) {
	    arm_raise_exception( EXC_FAST_IRQ );
	} else {
	    arm_raise_exception( EXC_IRQ );
	}
    }

    ir = MEM_READ_LONG(PC);
    pc = PC + 4;
    PC = pc;

    /** 
     * Check the condition bits first - if the condition fails return 
     * immediately without actually looking at the rest of the instruction.
     */
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
    if( !cond )
	return TRUE;

    /**
     * Condition passed, now for the actual instructions...
     */
    switch( GRP(ir) ) {
    case 0:
	if( (ir & 0x0D900000) == 0x01000000 ) {
	    /* Instructions that aren't actual data processing even though
	     * they sit in the DP instruction block.
	     */
	    switch( ir & 0x0FF000F0 ) {
	    case 0x01200010: /* BX Rd */
		armr.t = ir & 0x01;
		armr.r[15] = RM(ir) & 0xFFFFFFFE;
		break;
	    case 0x01000000: /* MRS Rd, CPSR */
		LRD(ir) = arm_get_cpsr();
		break;
	    case 0x01400000: /* MRS Rd, SPSR */
		LRD(ir) = armr.spsr;
		break;
	    case 0x01200000: /* MSR CPSR, Rd */
		arm_set_cpsr( RM(ir), ir );
		break;
	    case 0x01600000: /* MSR SPSR, Rd */
		arm_set_spsr( RM(ir), ir );
		break;
	    case 0x03200000: /* MSR CPSR, imm */
		arm_set_cpsr( ROTIMM12(ir), ir );
		break;
	    case 0x03600000: /* MSR SPSR, imm */
		arm_set_spsr( ROTIMM12(ir), ir );
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
		    LRN(ir) = RM(ir) * RS(ir);
		    break;
		case 1: /* MULS */
		    tmp = RM(ir) * RS(ir);
		    LRN(ir) = tmp;
		    armr.n = tmp>>31;
		    armr.z = (tmp == 0);
		    break;
		case 2: /* MLA */
		    LRN(ir) = RM(ir) * RS(ir) + RD(ir);
		    break;
		case 3: /* MLAS */
		    tmp = RM(ir) * RS(ir) + RD(ir);
		    LRN(ir) = tmp;
		    armr.n = tmp>>31;
		    armr.z = (tmp == 0);
		    break;
		case 8: /* UMULL */
		case 9: /* UMULLS */
		case 10: /* UMLAL */
		case 11: /* UMLALS */
		case 12: /* SMULL */
		case 13: /* SMULLS */
		case 14: /* SMLAL */
		case 15: /* SMLALS */
		    UNIMP(ir);
		    break;
		case 16: /* SWP */
		    tmp = arm_read_long( RN(ir) );
		    switch( RN(ir) & 0x03 ) {
		    case 1:
			tmp = ROTATE_RIGHT_LONG(tmp, 8);
			break;
		    case 2:
			tmp = ROTATE_RIGHT_LONG(tmp, 16);
			break;
		    case 3:
			tmp = ROTATE_RIGHT_LONG(tmp, 24);
			break;
		    }
		    arm_write_long( RN(ir), RM(ir) );
		    LRD(ir) = tmp;
		    break;
		case 20: /* SWPB */
		    tmp = arm_read_byte( RN(ir) );
		    arm_write_byte( RN(ir), RM(ir) );
		    LRD(ir) = tmp;
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
		UNIMP(ir);
		break;
	    case 2:
		if( LFLAG(ir) ) {
		    /* LDRSB */
		} else {
		}
		UNIMP(ir);
		break;
	    case 3:
		if( LFLAG(ir) ) {
		    /* LDRSH */
		} else {
		}
		UNIMP(ir);
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
		LRD(ir) = RN(ir) + arm_get_shift_operand(ir) + 
		    (armr.c ? 1 : 0);
		break;
	    case 11: /* ADCS */
		operand = arm_get_shift_operand(ir);
		operand2 = RN(ir);
		tmp = operand + operand2;
		tmp2 = tmp + armr.c ? 1 : 0;
		LRD(ir) = tmp2;
		if( RDn(ir) == 15 ) {
		    arm_restore_cpsr();
		} else {
		    armr.n = tmp >> 31;
		    armr.z = (tmp == 0 );
		    armr.c = IS_CARRY(tmp,operand,operand2) ||
			(tmp2 < tmp);
		    armr.v = IS_ADDOVERFLOW(tmp,operand, operand2) ||
			((tmp&0x80000000) != (tmp2&0x80000000));
		}
		break;
	    case 12: /* SBC */
		LRD(ir) = RN(ir) - arm_get_shift_operand(ir) - 
		    (armr.c ? 0 : 1);
		break;
	    case 13: /* SBCS */
		operand = RN(ir);
		operand2 = arm_get_shift_operand(ir);
		tmp = operand - operand2;
		tmp2 = tmp - (armr.c ? 0 : 1);
		if( RDn(ir) == 15 ) {
		    arm_restore_cpsr();
		} else {
		    armr.n = tmp >> 31;
		    armr.z = (tmp == 0 );
		    armr.c = IS_NOTBORROW(tmp,operand,operand2) &&
			(tmp2<tmp);
		    armr.v = IS_SUBOVERFLOW(tmp,operand,operand2) ||
			((tmp&0x80000000) != (tmp2&0x80000000));
		}
		break;
	    case 14: /* RSC */
		LRD(ir) = arm_get_shift_operand(ir) - RN(ir) -
		    (armr.c ? 0 : 1);
		break;
	    case 15: /* RSCS */
		operand = arm_get_shift_operand(ir);
		operand2 = RN(ir);
		tmp = operand - operand2;
		tmp2 = tmp - (armr.c ? 0 : 1);
		if( RDn(ir) == 15 ) {
		    arm_restore_cpsr();
		} else {
		    armr.n = tmp >> 31;
		    armr.z = (tmp == 0 );
		    armr.c = IS_NOTBORROW(tmp,operand,operand2) &&
			(tmp2<tmp);
		    armr.v = IS_SUBOVERFLOW(tmp,operand,operand2) ||
			((tmp&0x80000000) != (tmp2&0x80000000));
		}
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
	operand = arm_get_address_operand(ir);
	switch( (ir>>20)&0x17 ) {
	case 0: case 16: case 18: /* STR Rd, address */
	    arm_write_long( operand, RD(ir) );
	    break;
	case 1: case 17: case 19: /* LDR Rd, address */
	    LRD(ir) = arm_read_long(operand);
	    break;
	case 2: /* STRT Rd, address */
	    arm_write_long_user( operand, RD(ir) );
	    break;
	case 3: /* LDRT Rd, address */
	    LRD(ir) = arm_read_long_user( operand );
	    break;
	case 4: case 20: case 22: /* STRB Rd, address */
	    arm_write_byte( operand, RD(ir) );
	    break;
	case 5: case 21: case 23: /* LDRB Rd, address */
	    LRD(ir) = arm_read_byte( operand );
	    break;
	case 6: /* STRBT Rd, address */
	    arm_write_byte_user( operand, RD(ir) );
	    break;
	case 7: /* LDRBT Rd, address */
	    LRD(ir) = arm_read_byte_user( operand );
	    break;
	}
	break;
    case 2: /* Load/store multiple, branch*/
	if( (ir & 0x02000000) == 0x02000000 ) { /* B[L] imm24 */
	    operand = (SIGNEXT24(ir&0x00FFFFFF) << 2);
	    if( (ir & 0x01000000) == 0x01000000 ) { 
		armr.r[14] = pc; /* BL */
	    }
	    armr.r[15] = pc + 4 + operand;
	} else { /* Load/store multiple */
	    operand = RN(ir);
	    
	    switch( (ir & 0x01D00000) >> 20 ) {
	    case 0: /* STMDA */
		for( i=15; i>= 0; i-- ) {
		    if( (ir & (1<<i)) ) {
			arm_write_long( operand, armr.r[i] );
			operand -= 4;
		    }
		}
		break;
	    case 1: /* LDMDA */
		for( i=15; i>= 0; i-- ) {
		    if( (ir & (1<<i)) ) {
			armr.r[i] = arm_read_long( operand );
			operand -= 4;
		    }
		}
		break;
	    case 4: /* STMDA (S) */
		for( i=15; i>= 0; i-- ) {
		    if( (ir & (1<<i)) ) {
			arm_write_long( operand, USER_R(i) );
			operand -= 4;
		    }
		}
		break;
	    case 5: /* LDMDA (S) */
		if( (ir&0x00008000) ) { /* Load PC */
		    for( i=15; i>= 0; i-- ) {
			if( (ir & (1<<i)) ) {
			    armr.r[i] = arm_read_long( operand );
			    operand -= 4;
			}
		    }
		    arm_restore_cpsr();
		} else {
		    for( i=15; i>= 0; i-- ) {
			if( (ir & (1<<i)) ) {
			    USER_R(i) = arm_read_long( operand );
			    operand -= 4;
			}
		    }
		}
		break;
	    case 8: /* STMIA */
		for( i=0; i< 16; i++ ) {
		    if( (ir & (1<<i)) ) {
			arm_write_long( operand, armr.r[i] );
			operand += 4;
		    }
		}
		break;
	    case 9: /* LDMIA */
		for( i=0; i< 16; i++ ) {
		    if( (ir & (1<<i)) ) {
			armr.r[i] = arm_read_long( operand );
			operand += 4;
		    }
		}
		break;
	    case 12: /* STMIA (S) */
		for( i=0; i< 16; i++ ) {
		    if( (ir & (1<<i)) ) {
			arm_write_long( operand, USER_R(i) );
			operand += 4;
		    }
		}
		break;
	    case 13: /* LDMIA (S) */
		if( (ir&0x00008000) ) { /* Load PC */
		    for( i=0; i < 16; i++ ) {
			if( (ir & (1<<i)) ) {
			    armr.r[i] = arm_read_long( operand );
			    operand += 4;
			}
		    }
		    arm_restore_cpsr();
		} else {
		    for( i=0; i < 16; i++ ) {
			if( (ir & (1<<i)) ) {
			    USER_R(i) = arm_read_long( operand );
			    operand += 4;
			}
		    }
		}
		break;
	    case 16: /* STMDB */
		for( i=15; i>= 0; i-- ) {
		    if( (ir & (1<<i)) ) {
			operand -= 4;
			arm_write_long( operand, armr.r[i] );
		    }
		}
		break;
	    case 17: /* LDMDB */
		for( i=15; i>= 0; i-- ) {
		    if( (ir & (1<<i)) ) {
			operand -= 4;
			armr.r[i] = arm_read_long( operand );
		    }
		}
		break;
	    case 20: /* STMDB (S) */
		for( i=15; i>= 0; i-- ) {
		    if( (ir & (1<<i)) ) {
			operand -= 4;
			arm_write_long( operand, USER_R(i) );
		    }
		}
		break;
	    case 21: /* LDMDB (S) */
		if( (ir&0x00008000) ) { /* Load PC */
		    for( i=15; i>= 0; i-- ) {
			if( (ir & (1<<i)) ) {
			    operand -= 4;
			    armr.r[i] = arm_read_long( operand );
			}
		    }
		    arm_restore_cpsr();
		} else {
		    for( i=15; i>= 0; i-- ) {
			if( (ir & (1<<i)) ) {
			    operand -= 4;
			    USER_R(i) = arm_read_long( operand );
			}
		    }
		}
		break;
	    case 24: /* STMIB */
		for( i=0; i< 16; i++ ) {
		    if( (ir & (1<<i)) ) {
			operand += 4;
			arm_write_long( operand, armr.r[i] );
		    }
		}
		break;
	    case 25: /* LDMIB */
		for( i=0; i< 16; i++ ) {
		    if( (ir & (1<<i)) ) {
			operand += 4;
			armr.r[i] = arm_read_long( operand );
		    }
		}
		break;
	    case 28: /* STMIB (S) */
		for( i=0; i< 16; i++ ) {
		    if( (ir & (1<<i)) ) {
			operand += 4;
			arm_write_long( operand, USER_R(i) );
		    }
		}
		break;
	    case 29: /* LDMIB (S) */
		if( (ir&0x00008000) ) { /* Load PC */
		    for( i=0; i < 16; i++ ) {
			if( (ir & (1<<i)) ) {
			    operand += 4;
			    armr.r[i] = arm_read_long( operand );
			}
		    }
		    arm_restore_cpsr();
		} else {
		    for( i=0; i < 16; i++ ) {
			if( (ir & (1<<i)) ) {
			    operand += 4;
			    USER_R(i) = arm_read_long( operand );
			}
		    }
		}
		break;
	    }
	    
	    if( WFLAG(ir) ) 
		LRN(ir) = operand;
	}
	break;
    case 3: /* Copro */
	if( (ir & 0x0F000000) == 0x0F000000 ) { /* SWI */
	    arm_raise_exception( EXC_SOFTWARE );
	} else {
	    UNIMP(ir);
	}
	break;
    }
    return TRUE;
}
