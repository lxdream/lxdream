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
#include "xlat/xltcache.h"
#include "xlat/xlatdasm.h"
#include "x86dasm/bfd.h"
#include "x86dasm/dis-asm.h"
#include "sh4/sh4.h"

#if defined(__i386__)
#define HOST_CPU_NAME "x86"
#define HOST_PRINT print_insn_i386_att
#define HOST_SYNTAX bfd_mach_i386_i386_intel_syntax
#elif defined(__x86_64__) || defined(__amd64__)
#define HOST_CPU_NAME "x86"
#define HOST_PRINT print_insn_i386_att
#define HOST_SYNTAX bfd_mach_x86_64_intel_syntax
#elif defined(__arm__)
#define HOST_CPU_NAME "arm"
#define HOST_PRINT print_insn_little_arm
#define HOST_SYNTAX bfd_mach_arm_unknown
#else
#error Unidentified host platform
#endif

const struct cpu_desc_struct xlat_cpu_desc =
    { HOST_CPU_NAME, (disasm_func_t)xlat_disasm_instruction, NULL, mem_has_page,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 1, 
      NULL, 0, NULL, 0, 0,  
      &sh4r.pc };

static int xlat_disasm_output( void *data, const char *format, ... );
static void xlat_print_address( bfd_vma memaddr, struct disassemble_info *info );

static struct disassemble_info xlat_disasm_info;

static xlat_symbol *xlat_symtab;
static int xlat_num_symbols = 0;

void xlat_dump_block( void *block )
{
    xlat_disasm_block( stderr, block );
}

void xlat_disasm_block( FILE *out, void *block )
{
    uint32_t buflen = xlat_get_code_size(block);
    xlat_disasm_region( out, block, buflen );
}

void xlat_disasm_region(FILE *out, void *start, uint32_t len)
{
    uintptr_t start_addr = (uintptr_t)start;
    uintptr_t pc;
    for( pc = start_addr; pc < start_addr + len;  ) {
	char buf[256];
	char op[256];
	uintptr_t pc2 = xlat_disasm_instruction( pc, buf, sizeof(buf), op );
	fprintf( out, "%08X: %-20s %s\n", (unsigned int)pc, op, buf );
	pc = pc2;
    }
}

void xlat_disasm_init( xlat_symbol *symtab, int num_symbols )
{
    init_disassemble_info( &xlat_disasm_info, NULL, xlat_disasm_output );
    xlat_disasm_info.arch = bfd_arch_i386;
    xlat_disasm_info.mach = HOST_SYNTAX;
    xlat_disasm_info.endian = BFD_ENDIAN_LITTLE;
    xlat_disasm_info.buffer = 0;
    xlat_disasm_info.print_address_func = xlat_print_address;
    xlat_symtab = symtab;
    xlat_num_symbols = num_symbols;
}

static const char *xlat_find_symbol( bfd_vma memaddr, struct disassemble_info *info )
{
    int i;
    for( i=0; i<xlat_num_symbols; i++ ) {
	if( xlat_symtab[i].ptr == (void *)(uintptr_t)memaddr ) {
	    return xlat_symtab[i].name;
	}
    }
    return NULL;
}

static void xlat_print_address( bfd_vma memaddr, struct disassemble_info *info )
{
    const char *sym = xlat_find_symbol(memaddr, info);
    info->fprintf_func( info->stream, "%08X", memaddr );
    if( sym != NULL ) {
	info->fprintf_func( info->stream, " <%s>", sym );
    }
}

void xlat_print_symbolic_operand( char *buf, int hex, uintptr_t disp )
{
    const char *sym = xlat_find_symbol(disp, NULL);
    if( sym != NULL ) {
        snprintf( buf, 50, "<%s>", sym );
    } else if( hex ) {
        sprintf( buf, "0x%lx", disp );
    } else {
        sprintf( buf, "%d", (int)disp );
    }
}

uintptr_t xlat_disasm_instruction( uintptr_t pc, char *buf, int len, char *opcode )
{
    int count, i;

    xlat_disasm_info.stream = buf;
    buf[0] = 0;
    count = HOST_PRINT( pc, &xlat_disasm_info );
    if( count != 0 ) {
	unsigned char tmp[count];
	xlat_disasm_info.read_memory_func( pc, tmp, count, &xlat_disasm_info );
	for( i=0; i<count; i++ ) {
	    sprintf( opcode, "%02X ", ((unsigned int)tmp[i])&0xFF );
	    opcode += 3;
	}
	*(opcode-1) = '\0';
    }
    return pc + count;
}

static int xlat_disasm_output( void *data, const char *format, ... )
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
