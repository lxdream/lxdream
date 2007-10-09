/**
 * $Id: armcore.h,v 1.15 2007-10-09 08:11:51 nkeynes Exp $
 * 
 * Interface definitions for the ARM CPU emulation core proper.
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

#ifndef dream_armcore_H
#define dream_armcore_H 1

#include "dream.h"
#include <stdint.h>
#include <stdio.h>

#define ARM_BASE_RATE 2 /* MHZ */
extern uint32_t arm_cpu_freq;
extern uint32_t arm_cpu_period;

#define ROTATE_RIGHT_LONG(operand,shift) ((((uint32_t)operand) >> shift) | ((operand<<(32-shift))) )

struct arm_registers {
    uint32_t r[16]; /* Current register bank */
    
    uint32_t cpsr;
    uint32_t spsr;
    
    /* Various banked versions of the registers. Note that these are used
     * to save the registers for the named bank when leaving the mode, they're
     * not actually used actively.
     **/
    uint32_t user_r[7]; /* User/System bank 8..14 */
    uint32_t svc_r[3]; /* SVC bank 13..14, SPSR */
    uint32_t abt_r[3]; /* ABT bank 13..14, SPSR */
    uint32_t und_r[3]; /* UND bank 13..14, SPSR */
    uint32_t irq_r[3]; /* IRQ bank 13..14, SPSR */
    uint32_t fiq_r[8]; /* FIQ bank 8..14, SPSR */
    
    uint32_t c,n,z,v,t;
    
    /* "fake" registers */
    uint32_t int_pending; /* Mask of CPSR_I and CPSR_F */
    uint32_t shift_c;  /* used for temporary storage of shifter results */
    uint32_t icount; /* Instruction counter */
    gboolean running; /* Indicates that the ARM is operational, as opposed to
		       * halted */
};

#define CPSR_N 0x80000000 /* Negative flag */
#define CPSR_Z 0x40000000 /* Zero flag */
#define CPSR_C 0x20000000 /* Carry flag */
#define CPSR_V 0x10000000 /* Overflow flag */
#define CPSR_I 0x00000080 /* Interrupt disable bit */ 
#define CPSR_F 0x00000040 /* Fast interrupt disable bit */
#define CPSR_T 0x00000020 /* Thumb mode */
#define CPSR_MODE 0x0000001F /* Current execution mode */
#define CPSR_COMPACT_MASK 0x0FFFFFDF /* Mask excluding all separated flags */

#define MODE_USER 0x10 /* User mode */
#define MODE_FIQ   0x11 /* Fast IRQ mode */
#define MODE_IRQ  0x12 /* IRQ mode */
#define MODE_SVC  0x13 /* Supervisor mode */
#define MODE_ABT 0x17 /* Abort mode */
#define MODE_UND 0x1B /* Undefined mode */
#define MODE_SYS 0x1F /* System mode */

#define IS_PRIVILEGED_MODE() ((armr.cpsr & CPSR_MODE) != MODE_USER)
#define IS_EXCEPTION_MODE() (IS_PRIVILEGED_MODE() && (armr.cpsr & CPSR_MODE) != MODE_SYS)
#define IS_FIQ_MODE() ((armr.cpsr & CPSR_MODE) == MODE_FIQ)

extern struct arm_registers armr;

#define CARRY_FLAG (armr.cpsr&CPSR_C)

/* ARM core functions */
void arm_reset( void );
uint32_t arm_run_slice( uint32_t nanosecs );
void arm_save_state( FILE *f );
int arm_load_state( FILE *f );
gboolean arm_execute_instruction( void );
void arm_set_breakpoint( uint32_t pc, int type );
gboolean arm_clear_breakpoint( uint32_t pc, int type );
int arm_get_breakpoint( uint32_t pc );

/* ARM Memory */
uint32_t arm_read_long( uint32_t addr );
uint32_t arm_read_word( uint32_t addr );
uint32_t arm_read_byte( uint32_t addr );
uint32_t arm_read_long_user( uint32_t addr );
uint32_t arm_read_byte_user( uint32_t addr );
void arm_write_long( uint32_t addr, uint32_t val );
void arm_write_word( uint32_t addr, uint32_t val );
void arm_write_byte( uint32_t addr, uint32_t val );
void arm_write_long_user( uint32_t addr, uint32_t val );
void arm_write_byte_user( uint32_t addr, uint32_t val );
int32_t arm_read_phys_word( uint32_t addr );
int arm_has_page( uint32_t addr );
void arm_mem_init(void);
#endif /* !dream_armcore_H */
