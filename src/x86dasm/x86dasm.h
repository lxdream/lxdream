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
extern const struct cpu_desc_struct x86_cpu_desc;

typedef struct x86_symbol {
    const char *name;
    void *ptr;
} x86_symbol;

void x86_disasm_block( FILE *out, void *block, uint32_t len );
void x86_set_symtab( x86_symbol *symtab, int num_symbols );
void x86_disasm_init();
uintptr_t x86_disasm_instruction( uintptr_t pc, char *buf, int len, char *opcode );
void x86_print_symbolic_operand( char *buf, int hex, unsigned int disp );
