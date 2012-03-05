/**
 * $Id$
 *
 * Wrapper around i386-dis to supply the same behaviour as the other
 * disassembly functions.
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

#include <stdio.h>
#include "cpu.h"
#include "mem.h"
extern const struct cpu_desc_struct xlat_cpu_desc;

typedef struct xlat_symbol {
    const char *name;
    void *ptr;
} xlat_symbol;



/**
 * Dump the disassembly of the specified code block to a stream
 * (primarily for debugging purposes)
 * @param out The stream to write the output to
 * @param code a translated block
 */
void xlat_disasm_block( FILE *out, void *code );

/**
 * Disassemble one host instruction
 * @param pc Instruction to disassemble
 * @param buf buffer to hold the disassembled instruction
 * @param len sizeof buf
 * @param opcode buffer to hold the raw opcodes for the instruction (must be at least
 *    3 * maximum number of instruction bytes)
 * @return next pc after the current instruction
 */
uintptr_t xlat_disasm_instruction( uintptr_t pc, char *buf, int len, char *opcode );

void xlat_disasm_region( FILE *out, void *block, uint32_t len );
void xlat_disasm_init( xlat_symbol *symtab, int num_symbols );
uintptr_t xlat_disasm_instruction( uintptr_t pc, char *buf, int len, char *opcode );

void xlat_print_symbolic_operand( char *buf, int hex, uintptr_t disp );
