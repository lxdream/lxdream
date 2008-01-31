/**
 * $Id$
 *
 * Test cases for the SH4 => x86 translator core. Takes as
 * input a binary SH4 object (and VMA), generates the
 * corresponding x86 code, and outputs the disassembly.
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
#include <stdarg.h>
#include <getopt.h>
#include <sys/stat.h>
#include "x86dasm/x86dasm.h"
#include "sh4/sh4trans.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"

struct mmio_region mmio_region_MMU;
struct breakpoint_struct sh4_breakpoints[MAX_BREAKPOINTS];
int sh4_breakpoint_count = 0;

#define MAX_INS_SIZE 32

char *option_list = "s:o:d:h";
struct option longopts[1] = { { NULL, 0, 0, 0 } };

char *input_file = NULL;
char *diff_file = NULL;
char *output_file = NULL;
gboolean sh4_starting;
uint32_t start_addr = 0x8C010000;
uint32_t sh4_cpu_period = 5;
sh4ptr_t sh4_main_ram;
FILE *in;

char *inbuf;

struct x86_symbol local_symbols[] = {
    { "_sh4_read_byte", sh4_read_byte },
    { "_sh4_read_word", sh4_read_word },
    { "_sh4_read_long", sh4_read_long },
    { "_sh4_write_byte", sh4_write_byte },
    { "_sh4_write_word", sh4_write_word },
    { "_sh4_write_long", sh4_write_long }
};

int32_t sh4_read_byte( uint32_t addr ) 
{
    return *(uint8_t *)(inbuf+(addr-start_addr));
}
int32_t sh4_read_word( uint32_t addr ) 
{
    return *(uint16_t *)(inbuf+(addr-start_addr));
}
int32_t sh4_read_long( uint32_t addr ) 
{
    return *(uint32_t *)(inbuf+(addr-start_addr));
}
// Stubs
gboolean sh4_execute_instruction( ) { }
void sh4_accept_interrupt() {}
void sh4_set_breakpoint( uint32_t pc, breakpoint_type_t type ) { }
gboolean sh4_clear_breakpoint( uint32_t pc, breakpoint_type_t type ) { }
gboolean sh4_is_using_xlat() { return TRUE; }
int sh4_get_breakpoint( uint32_t pc ) { }
void event_execute() {}
void TMU_run_slice( uint32_t nanos ) {}
void SCIF_run_slice( uint32_t nanos ) {}
void sh4_write_byte( uint32_t addr, uint32_t val ) {}
void sh4_write_word( uint32_t addr, uint32_t val ) {}
void sh4_write_long( uint32_t addr, uint32_t val ) {}
void mem_copy_to_sh4( sh4addr_t addr, sh4ptr_t src, size_t size ) { }
void sh4_write_sr( uint32_t val ) { }
gboolean sh4_has_page( sh4vma_t vma ) { return TRUE; }
void syscall_invoke( uint32_t val ) { }
void dreamcast_stop() {} 
void dreamcast_reset() {}
uint32_t sh4_read_sr( void ) { }
gboolean sh4_raise_reset( int exc ) {}
gboolean sh4_raise_exception( int exc ) {}
gboolean sh4_raise_tlb_exception( int exc ) {}
gboolean sh4_raise_trap( int exc ) {}
void sh4_sleep() { }
uint32_t sh4_sleep_run_slice(uint32_t nanosecs) { }
void sh4_fsca( uint32_t angle, float *fr ) { }
void sh4_ftrv( float *fv, float *xmtrx ) { }
void signsat48(void) { }
gboolean gui_error_dialog( const char *fmt, ... ) { }
struct sh4_icache_struct sh4_icache;

void usage()
{
    fprintf( stderr, "Usage: testsh4x86 [options] <input bin file>\n");
    fprintf( stderr, "Options:\n");
    fprintf( stderr, "  -d <filename>  Diff results against contents of file\n" );
    fprintf( stderr, "  -h             Display this help message\n" );
    fprintf( stderr, "  -o <filename>  Output disassembly to file [stdout]\n" );
    fprintf( stderr, "  -s <addr>      Specify start address of binary [8C010000]\n" );
}

void emit( void *ptr, int level, const gchar *source, const char *msg, ... )
{
    va_list ap;
    va_start( ap, msg );
    vfprintf( stderr, msg, ap );
    fprintf( stderr, "\n" );
    va_end(ap);
}


struct sh4_registers sh4r;


int main( int argc, char *argv[] )
{
    struct stat st;
    int opt;
    while( (opt = getopt_long( argc, argv, option_list, longopts, NULL )) != -1 ) {
	switch( opt ) {
	case 'd':
	    diff_file = optarg;
	    break;
	case 'o':
	    output_file = optarg;
	    break;
	case 's':
	    start_addr = strtoul(optarg, NULL, 0);
	    break;
	case 'h':
	    usage();
	    exit(0);
	}
    }
    if( optind < argc ) {
	input_file = argv[optind++];
    } else {
	usage();
	exit(1);
    }

    in = fopen( input_file, "ro" );
    if( in == NULL ) {
	perror( "Unable to open input file" );
	exit(2);
    }
    fstat( fileno(in), &st );
    inbuf = malloc( st.st_size );
    fread( inbuf, st.st_size, 1, in );

    xlat_cache_init();
    uint32_t pc;
    uint8_t *buf = sh4_translate_basic_block( start_addr );
    uint32_t buflen = xlat_get_block_size(buf);
    x86_disasm_init( buf, 0x8c010000, buflen );
    x86_set_symtab( local_symbols, 6 );
    for( pc = 0x8c010000; pc < 0x8c010000 + buflen;  ) {
	char buf[256];
	char op[256];
	uint32_t pc2 = x86_disasm_instruction( pc, buf, sizeof(buf), op );
	fprintf( stdout, "%08X: %-20s %s\n", pc, op, buf );
	pc = pc2;
    }
}
