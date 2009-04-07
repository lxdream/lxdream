/**
 * $Id: testsh4x86.c 988 2009-01-15 11:23:20Z nkeynes $
 *
 * Test cases for the SH4 => XIR decoder. Takes as
 * input a binary SH4 object (and VMA), generates the
 * corresponding IR, and dumps it to stdout.
 *
 * Copyright (c) 2009 Nathan Keynes.
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
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "lxdream.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"
#include "sh4/sh4xir.h"


struct mmio_region mmio_region_MMU;
struct mmio_region mmio_region_PMM;
struct breakpoint_struct sh4_breakpoints[MAX_BREAKPOINTS];
struct sh4_registers sh4r;
int sh4_breakpoint_count = 0;
uint32_t sh4_cpu_period = 5;
struct sh4_icache_struct sh4_icache;

void MMU_ldtlb() { }
void FASTCALL sh4_sleep() { }
void FASTCALL sh4_write_fpscr( uint32_t val ) { }
void sh4_switch_fr_banks() { }
void FASTCALL sh4_write_sr( uint32_t val ) { }
uint32_t FASTCALL sh4_read_sr( void ) { return 0; }
void FASTCALL sh4_raise_trap( int exc ) { }
void FASTCALL sh4_raise_exception( int exc ) { }
void log_message( void *ptr, int level, const gchar *source, const char *msg, ... ) { }
gboolean sh4_execute_instruction( ) { return TRUE; }
void **sh4_address_space;
void **sh4_user_address_space;
unsigned char *xlat_output;

#define MAX_INS_SIZE 32
#define MAX_XIR_OPS 16384

char *option_list = "s:o:d:h";
struct option longopts[1] = { { NULL, 0, 0, 0 } };

struct xir_symbol_entry debug_symbols[] = {
    { "sh4_cpu_period", &sh4_cpu_period },
    { "sh4_write_fpscr", sh4_write_fpscr },
    { "sh4_write_sr", sh4_write_sr },
    { "sh4_read_sr", sh4_read_sr },
    { "sh4_sleep", sh4_sleep },
    { "sh4_switch_fr_banks", sh4_switch_fr_banks },
    { "sh4_raise_exception", sh4_raise_exception },
    { "sh4_raise_trap", sh4_raise_trap },
    { "sh4_execute_instruction", sh4_execute_instruction },
};

extern struct xlat_source_machine sh4_source_machine;
extern struct xlat_target_machine x86_target_machine;
void usage()
{
    fprintf( stderr, "Usage: testsh4xir [options] <input bin file>\n");
    fprintf( stderr, "Options:\n");
    fprintf( stderr, "  -d <filename>  Diff results against contents of file\n" );
    fprintf( stderr, "  -h             Display this help message\n" );
    fprintf( stderr, "  -o <filename>  Output disassembly to file [stdout]\n" );
    fprintf( stderr, "  -s <addr>      Specify start address of binary [8C010000]\n" );
}

int main( int argc, char *argv[] )
{
    char *input_file;
    char *output_file;
    char *diff_file;
    char *inbuf;
    uint32_t start_addr = 0x8c010000;
    struct stat st;
    int opt;
    struct xir_op xir[MAX_XIR_OPS];
    xir_op_t xir_ptr = &xir[0];
    
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

    mmio_region_MMU.mem = malloc(4096);
    memset( mmio_region_MMU.mem, 0, 4096 );

    ((uint32_t *)mmio_region_MMU.mem)[4] = 1;

    FILE *in = fopen( input_file, "ro" );
    if( in == NULL ) {
        perror( "Unable to open input file" );
        exit(2);
    }
    fstat( fileno(in), &st );
    inbuf = malloc( st.st_size );
    fread( inbuf, st.st_size, 1, in );
    sh4_icache.mask = 0xFFFFF000;
    sh4_icache.page_vma = start_addr & 0xFFFFF000;
    sh4_icache.page = (unsigned char *)(inbuf - (start_addr&0xFFF));
    sh4_icache.page_ppa = start_addr & 0xFFFFF000;

    struct xir_basic_block xbb;
    xbb.source = &sh4_source_machine;
    xbb.ir_alloc_begin = &xir[0];
    xbb.ir_alloc_end = &xir[MAX_XIR_OPS];
    xbb.ir_begin = xbb.ir_ptr = xbb.ir_end = xbb.ir_alloc_begin;
    xbb.pc_begin = start_addr;
    xbb.pc_end = start_addr+4096;
    xbb.source->decode_basic_block( &xbb );
    
    x86_target_machine.lower( &xbb, xbb.ir_begin, xbb.ir_end );
    xir_set_register_names( sh4_source_machine.reg_names, x86_target_machine.reg_names );
    xir_set_symbol_table( debug_symbols );
    xir_dump_block( &xir[0], NULL );
    return 0;
}
