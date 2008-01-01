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

#ifndef dream_cpu_H
#define dream_cpu_H 1

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

#define REG_INT 0
#define REG_FLT 1
#define REG_SPECIAL 2

/**
 * Structure that defines a single register in a CPU for display purposes.
 */
typedef struct reg_desc_struct {
    char *name;
    int type;
    void *value;
} reg_desc_t;

/**
 * CPU definition structure - basic information and support functions.
 */
typedef struct cpu_desc_struct {
    char *name; /* CPU Name */
    disasm_func_t disasm_func; /* Disassembly function */
    gboolean (*step_func)(); /* Single step function */
    int (*is_valid_page_func)(uint32_t); /* Test for valid memory page */
    void (*set_breakpoint)(uint32_t, breakpoint_type_t);
    gboolean (*clear_breakpoint)(uint32_t, breakpoint_type_t);
    int (*get_breakpoint)(uint32_t);
    size_t instr_size; /* Size of instruction */
    char *regs; /* Pointer to start of registers */
    size_t regs_size; /* Size of register structure in bytes */
    const struct reg_desc_struct *regs_info; /* Description of all registers */
    uint32_t *pc; /* Pointer to PC register */
} const *cpu_desc_t;

#ifdef __cplusplus
}
#endif

#endif /* !dream_cpu_H */
