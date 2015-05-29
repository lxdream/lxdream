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
#include <string.h>

#include "xlat/xlatdasm.h"
#include "sh4/sh4trans.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"
#include "sh4/mmu.h"

struct dreamcast_module sh4_module;
struct mmio_region mmio_region_MMU;
struct mmio_region mmio_region_PMM;
struct breakpoint_struct sh4_breakpoints[MAX_BREAKPOINTS];
int sh4_breakpoint_count = 0;
gboolean sh4_profile_blocks = FALSE;

#define MAX_INS_SIZE 32


struct mem_region_fn **sh4_address_space = (void *)0x12345432;
struct mem_region_fn **sh4_user_address_space = (void *)0x12345678;
char *option_list = "s:o:d:h";
struct option longopts[1] = { { NULL, 0, 0, 0 } };

char *input_file = NULL;
char *diff_file = NULL;
char *output_file = NULL;
gboolean sh4_starting;
uint32_t start_addr = 0x8C010000;
uint32_t sh4_cpu_period = 5;
unsigned char dc_main_ram[4096];
unsigned char dc_boot_rom[4096];
FILE *in;

char *inbuf;

struct xlat_symbol local_symbols[] = {
    { "sh4r+128", ((char *)&sh4r)+128 },
    { "sh4_cpu_period", &sh4_cpu_period },
    { "sh4_address_space", (void *)0x12345432 },
    { "sh4_user_address_space", (void *)0x12345678 },
    { "sh4_write_fpscr", sh4_write_fpscr },
    { "sh4_write_sr", sh4_write_sr },
    { "sh4_read_sr", sh4_read_sr },
    { "sh4_sleep", sh4_sleep },
    { "sh4_fsca", sh4_fsca },
    { "sh4_ftrv", sh4_ftrv },
    { "sh4_switch_fr_banks", sh4_switch_fr_banks },
    { "sh4_execute_instruction", sh4_execute_instruction },
    { "signsat48", signsat48 },
    { "xlat_get_code_by_vma", xlat_get_code_by_vma },
    { "xlat_get_code", xlat_get_code }
};

// Stubs
gboolean sh4_execute_instruction( ) { return TRUE; }
void sh4_accept_interrupt() {}
void sh4_set_breakpoint( uint32_t pc, breakpoint_type_t type ) { }
gboolean sh4_clear_breakpoint( uint32_t pc, breakpoint_type_t type ) { return TRUE; }
gboolean dreamcast_is_running() { return FALSE; }
int sh4_get_breakpoint( uint32_t pc ) { return 0; }
void sh4_finalize_instruction() { }
void sh4_core_exit( int exit_code ){}
void sh4_crashdump() {}
void event_execute() {}
void TMU_run_slice( uint32_t nanos ) {}
void CCN_set_cache_control( int val ) { }
void PMM_write_control( int ctr, uint32_t val ) { }
void SCIF_run_slice( uint32_t nanos ) {}
void FASTCALL sh4_write_fpscr( uint32_t val ) { }
void FASTCALL sh4_write_sr( uint32_t val ) { }
uint32_t FASTCALL sh4_read_sr( void ) { return 0; }
void FASTCALL sh4_sleep() { }
void FASTCALL sh4_fsca( uint32_t angle, float *fr ) { }
void FASTCALL sh4_ftrv( float *fv ) { }
void FASTCALL signsat48(void) { }
void sh4_switch_fr_banks() { }
void mem_copy_to_sh4( sh4addr_t addr, sh4ptr_t src, size_t size ) { }
gboolean sh4_has_page( sh4vma_t vma ) { return TRUE; }
void syscall_invoke( uint32_t val ) { }
void dreamcast_stop() {} 
void dreamcast_reset() {}
void FASTCALL sh4_raise_reset( int exc ) { }
void FASTCALL sh4_raise_exception( int exc ) { }
void FASTCALL sh4_raise_tlb_exception( int exc, sh4vma_t vma ) { }
void FASTCALL sh4_raise_tlb_multihit( sh4vma_t vma) { }
void FASTCALL sh4_raise_trap( int exc ) { }
void FASTCALL sh4_flush_store_queue( sh4addr_t addr ) { }
void FASTCALL sh4_flush_store_queue_mmu( sh4addr_t addr, void *exc ) { }
void sh4_shadow_block_begin() {}
void sh4_shadow_block_end() {}
void sh4_handle_pending_events() { }
uint32_t sh4_sleep_run_slice(uint32_t nanosecs) { return nanosecs; }
gboolean gui_error_dialog( const char *fmt, ... ) { return TRUE; }
gboolean FASTCALL mmu_update_icache( sh4vma_t addr ) { return TRUE; }
void MMU_ldtlb() { }
void event_schedule(int event, uint32_t nanos) { }
struct sh4_icache_struct sh4_icache;
struct mem_region_fn mem_region_unmapped;
const struct cpu_desc_struct sh4_cpu_desc;
sh4addr_t FASTCALL mmu_vma_to_phys_disasm( sh4vma_t vma ) { return vma; }

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

    mmio_region_MMU.mem = malloc(4096);
    memset( mmio_region_MMU.mem, 0, 4096 );

    ((uint32_t *)mmio_region_MMU.mem)[4] = 1;

    in = fopen( input_file, "ro" );
    if( in == NULL ) {
	perror( "Unable to open input file" );
	exit(2);
    }
    fstat( fileno(in), &st );
    inbuf = malloc( st.st_size );
    fread( inbuf, st.st_size, 1, in );
    sh4_icache.mask = 0xFFFFF000;
    sh4_icache.page_vma = start_addr & 0xFFFFF000;
    sh4_icache.page = (unsigned char *)(inbuf - (sh4_icache.page_vma&0xFFF));
    sh4_icache.page_ppa = start_addr & 0xFFFFF000;

    xlat_cache_init();
    uintptr_t pc;
    uint8_t *buf = sh4_translate_basic_block( start_addr );
    uint32_t buflen = xlat_get_code_size(buf);
    xlat_disasm_init( local_symbols, sizeof(local_symbols)/sizeof(struct xlat_symbol) );
    for( pc = (uintptr_t)buf; pc < ((uintptr_t)buf) + buflen;  ) {
	char buf[256];
	char op[256];
	uintptr_t pc2 = xlat_disasm_instruction( pc, buf, sizeof(buf), op );
	fprintf( stdout, "%p: %s\n", (void *)pc, buf );
	pc = pc2;
    }
    return 0;
}
