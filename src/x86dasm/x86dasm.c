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

#include <stdarg.h>
#include <string.h>
#include "x86dasm.h"
#include "bfd.h"
#include "dis-asm.h"
#include "sh4/sh4.h"
#include "sh4/sh4trans.h"

extern const struct reg_desc_struct sh4_reg_map[];
const struct cpu_desc_struct x86_cpu_desc = 
    { "x86", x86_disasm_instruction, NULL, mem_has_page, 
      NULL, NULL, NULL, 1, 
      (char *)&sh4r, sizeof(sh4r), sh4_reg_map,
      &sh4r.pc };

static int x86_disasm_output( void *data, const char *format, ... );
static void x86_print_address( bfd_vma memaddr, struct disassemble_info *info );

static struct disassemble_info x86_disasm_info;

static x86_symbol *x86_symtab;
static int x86_num_symbols = 0;   


void xlat_disasm_block( FILE *out, void *block )
{
    uint32_t buflen = xlat_get_block_size(block);
    x86_set_symtab( NULL, 0 );
    x86_disasm_block( out, block, buflen );
}

void x86_disasm_block(FILE *out, void *start, uint32_t len)
{
    uintptr_t start_addr = (uintptr_t)start;
    uint32_t pc;
    x86_disasm_init( start, start_addr, len );
    for( pc = start_addr; pc < start_addr + len;  ) {
	char buf[256];
	char op[256];
	uint32_t pc2 = x86_disasm_instruction( pc, buf, sizeof(buf), op );
	fprintf( out, "%08X: %-20s %s\n", pc, op, buf );
	pc = pc2;
    }
}

void x86_disasm_init(unsigned char *buf, uintptr_t vma, int buflen)
{
    init_disassemble_info( &x86_disasm_info, NULL, x86_disasm_output );
    x86_disasm_info.arch = bfd_arch_i386;
#if SH4_TRANSLATOR == TARGET_X86_64
    x86_disasm_info.mach =  bfd_mach_x86_64_intel_syntax;
#else
    x86_disasm_info.mach = bfd_mach_i386_i386_intel_syntax;
#endif
    x86_disasm_info.endian = BFD_ENDIAN_LITTLE;
    x86_disasm_info.buffer = buf;
    x86_disasm_info.buffer_vma = vma;
    x86_disasm_info.buffer_length = buflen;
    x86_disasm_info.print_address_func = x86_print_address;
}

void x86_set_symtab( x86_symbol *symtab, int num_symbols )
{
    x86_symtab = symtab;
    x86_num_symbols = num_symbols;
}

static const char *x86_find_symbol( bfd_vma memaddr, struct disassemble_info *info )
{
    int i;
    for( i=0; i<x86_num_symbols; i++ ) {
	if( x86_symtab[i].ptr == (void *)(uintptr_t)memaddr ) {
	    return x86_symtab[i].name;
	}
    }
    return NULL;
}

static void x86_print_address( bfd_vma memaddr, struct disassemble_info *info )
{
    const char *sym = x86_find_symbol(memaddr, info);
    info->fprintf_func( info->stream, "%08X", memaddr );
    if( sym != NULL ) {
	info->fprintf_func( info->stream, " <%s>", sym );
    }
}

uint32_t x86_disasm_instruction( uintptr_t pc, char *buf, int len, char *opcode )
{
    int count, i;

    x86_disasm_info.stream = buf;
    buf[0] = 0;
    count = print_insn_i386_att( pc, &x86_disasm_info );
    if( count != 0 ) {
	unsigned char tmp[count];
	x86_disasm_info.read_memory_func( pc, tmp, count, &x86_disasm_info );
	for( i=0; i<count; i++ ) {
	    sprintf( opcode, "%02X ", ((unsigned int)tmp[i])&0xFF );
	    opcode += 3;
	}
	*(opcode-1) = '\0';
    }
    return pc + count;
}

int x86_disasm_output( void *data, const char *format, ... )
{
    char *p = (char *)data;
    va_list ap;
    int n;
    p += strlen(p);
    va_start( ap, format );
    n = vsprintf( p, format, ap );
    va_end( ap );
    return n;
}
