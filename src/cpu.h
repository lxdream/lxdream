/**
 * $Id$
 * 
 * Generic CPU definitions, primarily for providing information to the GUI.
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

#ifndef lxdream_cpu_H
#define lxdream_cpu_H 1

#include <stdio.h>

#include "lxdream.h"
#include "mem.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Disassembly function pointer typedef.
 *
 * @param pc Address to disassemble
 * @param buffer String buffer to write disassembly into
 * @param buflen Maximum length of buffer
 * @return next address to disassemble
 */
typedef uint32_t (*disasm_func_t)(uint32_t pc, char *buffer, int buflen, char *opcode );

#define REG_TYPE_INT 0
#define REG_TYPE_FLOAT 1
#define REG_TYPE_DOUBLE 2
#define REG_TYPE_NONE 3 /* Used for empty/separator field */

/**
 * Structure that defines a single register in a CPU for display purposes.
 */
typedef struct reg_desc_struct {
    char *name;
    int type;
    void *value;
} reg_desc_t;

/**
 * CPU definition structure - basic information and support functions. Most
 * of this is for debugger use.
 */
typedef struct cpu_desc_struct {
    char *name; /* CPU Name */
    /**
     * Single instruction disassembly 
     **/
    disasm_func_t disasm_func;
    
    /**
     * Return a pointer to a register (indexed per the reg_desc table)
     */
    void * (*get_register)( int reg );
    gboolean (*is_valid_page_func)(uint32_t); /* Test for valid memory page */
    /* Access to memory addressed by the CPU - physical and virtual versions.
     * Virtual access should account for the current CPU mode, privilege level,
     * etc.
     * All functions return the number of bytes copied, which may be 0 if the
     * address is unmapped.
     */
    size_t (*read_mem_phys)(unsigned char *buf, uint32_t addr, size_t length);
    size_t (*write_mem_phys)(uint32_t addr, unsigned char *buf, size_t length);
    size_t (*read_mem_vma)(unsigned char *buf, uint32_t addr, size_t length);
    size_t (*write_mem_vma)(uint32_t addr, unsigned char *buf, size_t length);
    gboolean (*step_func)(); /* Single step function */
    void (*set_breakpoint)(uint32_t, breakpoint_type_t);
    gboolean (*clear_breakpoint)(uint32_t, breakpoint_type_t);
    int (*get_breakpoint)(uint32_t);
    size_t instr_size; /* Size of instruction */
    char *regs; /* Pointer to start of registers */
    size_t regs_size; /* Size of register structure in bytes */
    const struct reg_desc_struct *regs_info; /* Description of all registers */
    unsigned int num_gpr_regs; /* Number of general purpose registers */
    unsigned int num_gdb_regs; /* Total number of registers visible to gdb */
    uint32_t *pc; /* Pointer to PC register */
} const *cpu_desc_t;

#ifdef __cplusplus
}
#endif

gboolean gdb_init_server( const char *interface, int port, cpu_desc_t cpu, gboolean mmu );
void cpu_print_registers( FILE *out, cpu_desc_t cpu );

#endif /* !lxdream_cpu_H */
