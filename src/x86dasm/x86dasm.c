/**
 * $Id: x86dasm.c,v 1.1 2007-08-28 08:46:54 nkeynes Exp $
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
#include "x86dasm.h"
#include "bfd.h"
#include "dis-asm.h"
#include "sh4/sh4core.h"

extern const struct reg_desc_struct sh4_reg_map[];
const struct cpu_desc_struct x86_cpu_desc = 
    { "x86", x86_disasm_instruction, NULL, mem_has_page, 
      NULL, NULL, NULL, 1, 
      (char *)&sh4r, sizeof(sh4r), sh4_reg_map,
      &sh4r.pc };

static int x86_disasm_output( void *data, const char *format, ... );
static int x86_read_memory( bfd_vma memaddr, bfd_byte *buffer, unsigned int length,
			    struct disassemble_info *info );
static int x86_print_address( bfd_vma memaddr, struct disassemble_info *info );

static struct disassemble_info x86_disasm_info;

void x86_disasm_init(char *buf, uint32_t vma, int buflen)
{
    init_disassemble_info( &x86_disasm_info, NULL, x86_disasm_output );
    x86_disasm_info.arch = bfd_arch_i386;
    x86_disasm_info.mach = bfd_mach_i386_i386_intel_syntax;
    x86_disasm_info.endian = BFD_ENDIAN_LITTLE;
    x86_disasm_info.buffer = buf;
    x86_disasm_info.buffer_vma = vma;
    x86_disasm_info.buffer_length = buflen;
}


uint32_t x86_disasm_instruction( uint32_t pc, char *buf, int len, char *opcode )
{
    int count, i;

    x86_disasm_info.stream = buf;
    buf[0] = 0;
    count = print_insn_i386_att( pc, &x86_disasm_info );
    if( count != 0 ) {
	char tmp[count];
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
