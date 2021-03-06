/**
 * $Id$
 *
 * SH4 shadow execution core - runs xlat + emu together and checks that the
 * results are the same.
 *
 * Copyright (c) 2010 Nathan Keynes.
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "clock.h"
#include "mem.h"
#include "mmio.h"
#include "sh4/sh4.h"
#include "sh4/sh4core.h"
#include "sh4/sh4trans.h"
#include "sh4/mmu.h"

typedef enum {
    READ_LONG,
    WRITE_LONG,
    READ_WORD,
    WRITE_WORD,
    READ_BYTE,
    WRITE_BYTE,
    PREFETCH,
    READ_BYTE_FOR_WRITE
} MemOp;

char *memOpNames[] = { "read_long", "write_long", "read_word", "write_word",
        "read_byte", "write_byte", "prefetch", "read_byte_for_write" };

struct mem_log_entry {
    MemOp op;
    sh4addr_t addr;
    uint32_t value;
};

static struct sh4_registers shadow_sh4r;
static struct mem_region_fn **shadow_address_space;
static struct mem_region_fn **p4_address_space;

typedef enum {
    SHADOW_LOG,
    SHADOW_CHECK
} shadow_mode_t;
static shadow_mode_t shadow_address_mode = SHADOW_LOG;

#define MEM_LOG_SIZE 4096
static struct mem_log_entry *mem_log;
static uint32_t mem_log_posn, mem_log_size;
static uint32_t mem_check_posn;

#define IS_STORE_QUEUE(X) (((X)&0xFC000000) == 0xE0000000)

static void log_mem_op( MemOp op, sh4addr_t addr, uint32_t value )
{
    if( mem_log_posn == mem_log_size ) {
        struct mem_log_entry *tmp = realloc(mem_log, mem_log_size * sizeof(struct mem_log_entry) * 2);
        assert( tmp != NULL );
        mem_log_size *= 2;
        mem_log = tmp;
    }
    mem_log[mem_log_posn].op = op;
    mem_log[mem_log_posn].addr = addr;
    mem_log[mem_log_posn].value = value;
    mem_log_posn++;
}

static void print_mem_op( FILE *f, MemOp op, sh4addr_t addr, uint32_t value )
{
    if( op == WRITE_LONG || op == WRITE_WORD || op == WRITE_BYTE ) {
        fprintf( f, "%s( %08X, %08X )\n", memOpNames[op], addr, value );
    } else {
        fprintf( f, "%s( %08X )\n", memOpNames[op], addr );
    }
}

void dump_mem_ops()
{
    for( unsigned i=0; i<mem_log_posn; i++ ) {
        print_mem_op( stderr, mem_log[i].op, mem_log[i].addr, mem_log[i].value );
    }
}

static int32_t check_mem_op( MemOp op, sh4addr_t addr, uint32_t value )
{
    if( mem_check_posn >= mem_log_posn ) {
        fprintf( stderr, "Unexpected interpreter memory operation: " );
        print_mem_op(stderr, op, addr, value );
        abort();
    }
    if( mem_log[mem_check_posn].op != op ||
        mem_log[mem_check_posn].addr != addr ||
        (( op == WRITE_LONG || op == WRITE_WORD || op == WRITE_BYTE ) &&
           mem_log[mem_check_posn].value != value ) ) {
        fprintf(stderr, "Memory operation mismatch. Translator: " );
        print_mem_op(stderr, mem_log[mem_check_posn].op,
                mem_log[mem_check_posn].addr, mem_log[mem_check_posn].value );
        fprintf(stderr, "Emulator: ");
        print_mem_op(stderr, op, addr, value );
        abort();
    }
    return mem_log[mem_check_posn++].value;
}

#define CHECK_REG(sym, name) if( xsh4r->sym != esh4r->sym ) { \
    isgood = FALSE; fprintf( stderr, name "  Xlt = %08X, Emu = %08X\n", xsh4r->sym, esh4r->sym ); }

static gboolean check_registers( struct sh4_registers *xsh4r, struct sh4_registers *esh4r )
{
    gboolean isgood = TRUE;
    for( unsigned i=0; i<16; i++ ) {
        if( xsh4r->r[i] != esh4r->r[i] ) {
            isgood = FALSE;
            fprintf( stderr, "R%d  Xlt = %08X, Emu = %08X\n", i, xsh4r->r[i], esh4r->r[i] );
        }
    }
    for( unsigned i=0; i<8; i++ ) {
        if( xsh4r->r_bank[i] != esh4r->r_bank[i] ) {
            isgood = FALSE;
            fprintf( stderr, "R_BANK%d  Xlt = %08X, Emu = %08X\n", i, xsh4r->r_bank[i], esh4r->r_bank[i] );
        }
    }
    for( unsigned i=0; i<16; i++ ) {
        if( *((uint32_t *)&xsh4r->fr[0][i]) != *((uint32_t *)&esh4r->fr[0][i]) ) {
            isgood = FALSE;
            fprintf( stderr, "FR%d  Xlt = %f (0x%08X), Emu = %f (0x%08X)\n", i, xsh4r->fr[0][i],
                    *((uint32_t *)&xsh4r->fr[0][i]),
                    esh4r->fr[0][i],
                    *((uint32_t *)&esh4r->fr[0][i])
                    );
        }
    }
    for( unsigned i=0; i<16; i++ ) {
        if( *((uint32_t *)&xsh4r->fr[1][i]) != *((uint32_t *)&esh4r->fr[1][i]) ) {
            isgood = FALSE;
            fprintf( stderr, "XF%d  Xlt = %f (0x%08X), Emu = %f (0x%08X)\n", i, xsh4r->fr[1][i],
                    *((uint32_t *)&xsh4r->fr[1][i]),
                    esh4r->fr[1][i],
                    *((uint32_t *)&esh4r->fr[1][i])
                    );
        }
    }

    CHECK_REG(t, "T");
    CHECK_REG(m, "M");
    CHECK_REG(s, "S");
    CHECK_REG(q, "Q");
    CHECK_REG(pc, "PC");
    CHECK_REG(pr, "PR");
    CHECK_REG(sr, "SR");
    CHECK_REG(fpscr, "FPSCR");
    CHECK_REG(fpul.i, "FPUL");
    if( xsh4r->mac != esh4r->mac ) {
        isgood = FALSE;
        fprintf( stderr, "MAC  Xlt = %016llX, Emu = %016llX\n", xsh4r->mac, esh4r->mac );
    }
    CHECK_REG(gbr, "GBR");
    CHECK_REG(ssr, "SSR");
    CHECK_REG(spc, "SPC");
    CHECK_REG(sgr, "SGR");
    CHECK_REG(dbr, "DBR");
    CHECK_REG(vbr, "VBR");
    CHECK_REG(sh4_state, "STATE");
    if( memcmp( xsh4r->store_queue, esh4r->store_queue, sizeof(xsh4r->store_queue) ) != 0 ) {
        isgood = FALSE;
        fprintf( stderr, "Store queue  Xlt =\n" );
        fwrite_dump( (unsigned char *)xsh4r->store_queue, sizeof(xsh4r->store_queue), stderr );
        fprintf( stderr, "             Emu =\n" );
        fwrite_dump( (unsigned char *)esh4r->store_queue, sizeof(esh4r->store_queue), stderr );
    }
    return isgood;
}

static mem_region_fn_t real_region( sh4addr_t addr )
{
    if( addr >= 0xE0000000 )
        return p4_address_space[VMA_TO_EXT_ADDR(addr)>>12];
    else
        return ext_address_space[VMA_TO_EXT_ADDR(addr)>>12];
}

static FASTCALL int32_t shadow_read_long( sh4addr_t addr )
{
    if( shadow_address_mode == SHADOW_LOG ) {
        int32_t rv = real_region(addr)->read_long(addr);
        log_mem_op( READ_LONG, addr, rv );
        return rv;
    } else {
        return check_mem_op( READ_LONG, addr, 0 );
    }
}

static FASTCALL int32_t shadow_read_word( sh4addr_t addr )
{
    if( shadow_address_mode == SHADOW_LOG ) {
        int32_t rv = real_region(addr)->read_word(addr);
        log_mem_op( READ_WORD, addr, rv );
        return rv;
    } else {
        return check_mem_op( READ_WORD, addr, 0 );
    }
}

static FASTCALL int32_t shadow_read_byte( sh4addr_t addr )
{
    if( shadow_address_mode == SHADOW_LOG ) {
        int32_t rv = real_region(addr)->read_byte(addr);
        log_mem_op( READ_BYTE, addr, rv );
        return rv;
    } else {
        return check_mem_op( READ_BYTE, addr, 0 );
    }
}

static FASTCALL int32_t shadow_read_byte_for_write( sh4addr_t addr )
{
    if( shadow_address_mode == SHADOW_LOG ) {
        int32_t rv = real_region(addr)->read_byte_for_write(addr);
        log_mem_op( READ_BYTE_FOR_WRITE, addr, rv );
        return rv;
    } else {
        return check_mem_op( READ_BYTE_FOR_WRITE, addr, 0 );
    }
}

static FASTCALL void shadow_write_long( sh4addr_t addr, uint32_t val )
{
    if( shadow_address_mode == SHADOW_LOG ) {
        real_region(addr)->write_long(addr, val);
        if( !IS_STORE_QUEUE(addr) )
            log_mem_op( WRITE_LONG, addr, val );
    } else {
        if( !IS_STORE_QUEUE(addr) ) {
            check_mem_op( WRITE_LONG, addr, val );
        } else {
            real_region(addr)->write_long(addr, val);
        }
    }
}

static FASTCALL void shadow_write_word( sh4addr_t addr, uint32_t val )
{
    if( shadow_address_mode == SHADOW_LOG ) {
        real_region(addr)->write_word(addr, val);
        if( !IS_STORE_QUEUE(addr) )
            log_mem_op( WRITE_WORD, addr, val );
    } else {
        if( !IS_STORE_QUEUE(addr) ) {
            check_mem_op( WRITE_WORD, addr, val );
        } else {
            real_region(addr)->write_word(addr, val);
        }
    }
}

static FASTCALL void shadow_write_byte( sh4addr_t addr, uint32_t val )
{
    if( shadow_address_mode == SHADOW_LOG ) {
        real_region(addr)->write_byte(addr, val);
        if( !IS_STORE_QUEUE(addr) )
            log_mem_op( WRITE_BYTE, addr, val );
    } else {
        if( !IS_STORE_QUEUE(addr) ) {
            check_mem_op( WRITE_BYTE, addr, val );
        } else {
            real_region(addr)->write_byte(addr, val);
        }
    }
}

static FASTCALL void shadow_prefetch( sh4addr_t addr )
{
    if( shadow_address_mode == SHADOW_LOG ) {
        real_region(addr)->prefetch(addr);
        log_mem_op( PREFETCH, addr, 0 );
    } else {
        check_mem_op( PREFETCH, addr, 0 );
    }
}
struct mem_region_fn shadow_fns = {
        shadow_read_long, shadow_write_long,
        shadow_read_word, shadow_write_word,
        shadow_read_byte, shadow_write_byte,
        NULL, NULL, shadow_prefetch, shadow_read_byte_for_write };

void sh4_shadow_block_begin()
{
    memcpy( &shadow_sh4r, &sh4r, sizeof(struct sh4_registers) );
    mem_log_posn = 0;
}

void sh4_shadow_block_end()
{
    struct sh4_registers temp_sh4r;

    /* Save the end registers, and restore the state back to the start */
    memcpy( &temp_sh4r, &sh4r, sizeof(struct sh4_registers) );
    memcpy( &sh4r, &shadow_sh4r, sizeof(struct sh4_registers) );

    shadow_address_mode = SHADOW_CHECK;
    mem_check_posn = 0;
    sh4r.new_pc = sh4r.pc + 2;
    while( sh4r.slice_cycle < temp_sh4r.slice_cycle ) {
        sh4_execute_instruction();
        sh4r.slice_cycle += sh4_cpu_period;
    }

    if( !check_registers( &temp_sh4r, &sh4r ) ) {
        fprintf( stderr, "After executing block at %08X\n", shadow_sh4r.pc );
        fprintf( stderr, "Translated block was:\n" );
        sh4_translate_dump_block(shadow_sh4r.pc);
        abort();
    }
    if( mem_check_posn < mem_log_posn ) {
        fprintf( stderr, "Additional translator memory operations:\n" );
        while( mem_check_posn < mem_log_posn ) {
            print_mem_op( stderr, mem_log[mem_check_posn].op, mem_log[mem_check_posn].addr, mem_log[mem_check_posn].value );
            mem_check_posn++;
        }
        abort();
    }
    shadow_address_mode = SHADOW_LOG;
}


void sh4_shadow_init()
{
    shadow_address_mode = SHADOW_LOG;
    p4_address_space = mem_alloc_pages( sizeof(mem_region_fn_t) * 32 );
    shadow_address_space = mem_alloc_pages( sizeof(mem_region_fn_t) * 32 );
    for( unsigned i=0; i < (32 * 4096); i++ ) {
        shadow_address_space[i] = &shadow_fns;
    }

    mem_log_size = MEM_LOG_SIZE;
    mem_log = malloc( mem_log_size * sizeof(struct mem_log_entry) );
    assert( mem_log != NULL );

    sh4_translate_set_callbacks( sh4_shadow_block_begin, sh4_shadow_block_end );
    sh4_translate_set_fastmem( FALSE );
    memcpy( p4_address_space, sh4_address_space + (0xE0000000>>LXDREAM_PAGE_BITS),
            sizeof(mem_region_fn_t) * (0x20000000>>LXDREAM_PAGE_BITS) );
    memcpy( sh4_address_space + (0xE0000000>>LXDREAM_PAGE_BITS), shadow_address_space,
            sizeof(mem_region_fn_t) * (0x20000000>>LXDREAM_PAGE_BITS) );
    mmu_set_ext_address_space(shadow_address_space);
}

