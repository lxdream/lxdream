/**
 * $Id: armcore.h,v 1.6 2005-12-25 05:57:00 nkeynes Exp $
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

#define ROTATE_RIGHT_LONG(operand,shift) ((((uint32_t)operand) >> shift) | ((operand<<(32-shift))) )

struct arm_registers {
    uint32_t r[16]; /* Current register bank */
    
    uint32_t cpsr;
    uint32_t spsr;
    
    /* Various banked versions of the registers. */
    uint32_t fiq_r[7]; /* FIQ bank 8..14 */
    uint32_t irq_r[2]; /* IRQ bank 13..14 */
    uint32_t und_r[2]; /* UND bank 13..14 */
    uint32_t abt_r[2]; /* ABT bank 13..14 */
    uint32_t svc_r[2]; /* SVC bank 13..14 */
    uint32_t user_r[7]; /* User/System bank 8..14 */
    
    uint32_t c,n,z,v,t;
    
    /* "fake" registers */
    uint32_t shift_c;  /* used for temporary storage of shifter results */
    uint32_t icount; /* Instruction counter */
};

#define CPSR_N 0x80000000 /* Negative flag */
#define CPSR_Z 0x40000000 /* Zero flag */
#define CPSR_C 0x20000000 /* Carry flag */
#define CPSR_V 0x10000000 /* Overflow flag */
#define CPSR_I 0x00000080 /* Interrupt disable bit */ 
#define CPSR_F 0x00000040 /* Fast interrupt disable bit */
#define CPSR_T 0x00000020 /* Thumb mode */
#define CPSR_MODE 0x0000001F /* Current execution mode */

#define MODE_USER 0x00 /* User mode */
#define MODE_FIQ   0x01 /* Fast IRQ mode */
#define MODE_IRQ  0x02 /* IRQ mode */
#define MODE_SV   0x03 /* Supervisor mode */
#define MODE_ABT 0x07 /* Abort mode */
#define MODE_UND 0x0B /* Undefined mode */
#define MODE_SYS 0x0F /* System mode */

extern struct arm_registers armr;

#define CARRY_FLAG (armr.cpsr&CPSR_C)

/* ARM Memory */
int32_t arm_read_long( uint32_t addr );
int32_t arm_read_word( uint32_t addr );
int32_t arm_read_byte( uint32_t addr );
void arm_write_long( uint32_t addr, uint32_t val );
void arm_write_word( uint32_t addr, uint32_t val );
void arm_write_byte( uint32_t addr, uint32_t val );
int32_t arm_read_phys_word( uint32_t addr );
int arm_has_page( uint32_t addr );
gboolean arm_execute_instruction( void );

#endif /* !dream_armcore_H */
