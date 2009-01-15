/**
 * $Id$
 * Implements the on-chip operand cache, instruction cache, and store queue.
 *
 * Copyright (c) 2008 Nathan Keynes.
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

#define MODULE sh4_module

#include <string.h>
#include "dream.h"
#include "mem.h"
#include "mmio.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"
#include "sh4/xltcache.h"
#include "sh4/mmu.h"

#define OCRAM_START (0x7C000000>>LXDREAM_PAGE_BITS)
#define OCRAM_MID   (0x7E000000>>LXDREAM_PAGE_BITS)
#define OCRAM_END   (0x80000000>>LXDREAM_PAGE_BITS)

#define CACHE_VALID 1
#define CACHE_DIRTY 2

#define ICACHE_ENTRY_COUNT 256
#define OCACHE_ENTRY_COUNT 512

struct cache_line {
    uint32_t key;  // Fast address match - bits 5..28 for valid entry, -1 for invalid entry
    uint32_t tag;  // tag + flags value from the address field
};    


static struct cache_line ccn_icache[ICACHE_ENTRY_COUNT];
static struct cache_line ccn_ocache[OCACHE_ENTRY_COUNT];
static unsigned char ccn_icache_data[ICACHE_ENTRY_COUNT*32];
static unsigned char ccn_ocache_data[OCACHE_ENTRY_COUNT*32];


/*********************** General module requirements ********************/

void CCN_save_state( FILE *f )
{
    fwrite( &ccn_icache, sizeof(ccn_icache), 1, f );
    fwrite( &ccn_icache_data, sizeof(ccn_icache_data), 1, f );
    fwrite( &ccn_ocache, sizeof(ccn_ocache), 1, f);
    fwrite( &ccn_ocache_data, sizeof(ccn_ocache_data), 1, f);
}

int CCN_load_state( FILE *f )
{
    /* Setup the cache mode according to the saved register value
     * (mem_load runs before this point to load all MMIO data)
     */
    mmio_region_MMU_write( CCR, MMIO_READ(MMU, CCR) );

    if( fread( &ccn_icache, sizeof(ccn_icache), 1, f ) != 1 ) {
        return 1;
    }
    if( fread( &ccn_icache_data, sizeof(ccn_icache_data), 1, f ) != 1 ) {
        return 1;
    }
    if( fread( &ccn_ocache, sizeof(ccn_ocache), 1, f ) != 1 ) {
        return 1;
    }
    if( fread( &ccn_ocache_data, sizeof(ccn_ocache_data), 1, f ) != 1 ) {
        return 1;
    }
    return 0;
}

/************************* OCRAM memory address space ************************/

#define OCRAMPAGE0 (&ccn_ocache_data[4096])  /* Lines 128-255 */
#define OCRAMPAGE1 (&ccn_ocache_data[12288]) /* Lines 384-511 */

static int32_t FASTCALL ocram_page0_read_long( sh4addr_t addr )
{
    return *((int32_t *)(OCRAMPAGE0 + (addr&0x00000FFF)));
}
static int32_t FASTCALL ocram_page0_read_word( sh4addr_t addr )
{
    return SIGNEXT16(*((int16_t *)(OCRAMPAGE0 + (addr&0x00000FFF))));
}
static int32_t FASTCALL ocram_page0_read_byte( sh4addr_t addr )
{
    return SIGNEXT8(*((int16_t *)(OCRAMPAGE0 + (addr&0x00000FFF))));
}
static void FASTCALL ocram_page0_write_long( sh4addr_t addr, uint32_t val )
{
    *(uint32_t *)(OCRAMPAGE0 + (addr&0x00000FFF)) = val;
}
static void FASTCALL ocram_page0_write_word( sh4addr_t addr, uint32_t val )
{
    *(uint16_t *)(OCRAMPAGE0 + (addr&0x00000FFF)) = (uint16_t)val;
}
static void FASTCALL ocram_page0_write_byte( sh4addr_t addr, uint32_t val )
{
    *(uint8_t *)(OCRAMPAGE0 + (addr&0x00000FFF)) = (uint8_t)val;
}
static void FASTCALL ocram_page0_read_burst( unsigned char *dest, sh4addr_t addr )
{
    memcpy( dest, OCRAMPAGE0+(addr&0x00000FFF), 32 );
}
static void FASTCALL ocram_page0_write_burst( sh4addr_t addr, unsigned char *src )
{
    memcpy( OCRAMPAGE0+(addr&0x00000FFF), src, 32 );
}

struct mem_region_fn mem_region_ocram_page0 = {
        ocram_page0_read_long, ocram_page0_write_long,
        ocram_page0_read_word, ocram_page0_write_word,
        ocram_page0_read_byte, ocram_page0_write_byte,
        ocram_page0_read_burst, ocram_page0_write_burst,
        unmapped_prefetch };

static int32_t FASTCALL ocram_page1_read_long( sh4addr_t addr )
{
    return *((int32_t *)(OCRAMPAGE1 + (addr&0x00000FFF)));
}
static int32_t FASTCALL ocram_page1_read_word( sh4addr_t addr )
{
    return SIGNEXT16(*((int16_t *)(OCRAMPAGE1 + (addr&0x00000FFF))));
}
static int32_t FASTCALL ocram_page1_read_byte( sh4addr_t addr )
{
    return SIGNEXT8(*((int16_t *)(OCRAMPAGE1 + (addr&0x00000FFF))));
}
static void FASTCALL ocram_page1_write_long( sh4addr_t addr, uint32_t val )
{
    *(uint32_t *)(OCRAMPAGE1 + (addr&0x00000FFF)) = val;
}
static void FASTCALL ocram_page1_write_word( sh4addr_t addr, uint32_t val )
{
    *(uint16_t *)(OCRAMPAGE1 + (addr&0x00000FFF)) = (uint16_t)val;
}
static void FASTCALL ocram_page1_write_byte( sh4addr_t addr, uint32_t val )
{
    *(uint8_t *)(OCRAMPAGE1 + (addr&0x00000FFF)) = (uint8_t)val;
}
static void FASTCALL ocram_page1_read_burst( unsigned char *dest, sh4addr_t addr )
{
    memcpy( dest, OCRAMPAGE1+(addr&0x00000FFF), 32 );
}
static void FASTCALL ocram_page1_write_burst( sh4addr_t addr, unsigned char *src )
{
    memcpy( OCRAMPAGE1+(addr&0x00000FFF), src, 32 );
}

struct mem_region_fn mem_region_ocram_page1 = {
        ocram_page1_read_long, ocram_page1_write_long,
        ocram_page1_read_word, ocram_page1_write_word,
        ocram_page1_read_byte, ocram_page1_write_byte,
        ocram_page1_read_burst, ocram_page1_write_burst,
        unmapped_prefetch };

/************************** Cache direct access ******************************/

static int32_t FASTCALL ccn_icache_addr_read( sh4addr_t addr )
{
    int entry = (addr & 0x00001FE0);
    return ccn_icache[entry>>5].tag;
}

static void FASTCALL ccn_icache_addr_write( sh4addr_t addr, uint32_t val )
{
    int entry = (addr & 0x00003FE0);
    struct cache_line *line = &ccn_ocache[entry>>5];
    if( addr & 0x08 ) { // Associative
        /* FIXME: implement this - requires ITLB lookups, with exception in case of multi-hit */
    } else {
        line->tag = val & 0x1FFFFC01;
        line->key = (val & 0x1FFFFC00)|(entry & 0x000003E0);
    }
}

struct mem_region_fn p4_region_icache_addr = {
        ccn_icache_addr_read, ccn_icache_addr_write,
        unmapped_read_long, unmapped_write_long,
        unmapped_read_long, unmapped_write_long,
        unmapped_read_burst, unmapped_write_burst,
        unmapped_prefetch };


static int32_t FASTCALL ccn_icache_data_read( sh4addr_t addr )
{
    int entry = (addr & 0x00001FFC);
    return *(uint32_t *)&ccn_icache_data[entry];
}

static void FASTCALL ccn_icache_data_write( sh4addr_t addr, uint32_t val )
{
    int entry = (addr & 0x00001FFC);
    *(uint32_t *)&ccn_icache_data[entry] = val;    
}

struct mem_region_fn p4_region_icache_data = {
        ccn_icache_data_read, ccn_icache_data_write,
        unmapped_read_long, unmapped_write_long,
        unmapped_read_long, unmapped_write_long,
        unmapped_read_burst, unmapped_write_burst,
        unmapped_prefetch };


static int32_t FASTCALL ccn_ocache_addr_read( sh4addr_t addr )
{
    int entry = (addr & 0x00003FE0);
    return ccn_ocache[entry>>5].tag;
}

static void FASTCALL ccn_ocache_addr_write( sh4addr_t addr, uint32_t val )
{
    int entry = (addr & 0x00003FE0);
    struct cache_line *line = &ccn_ocache[entry>>5];
    if( addr & 0x08 ) { // Associative
    } else {
        if( (line->tag & (CACHE_VALID|CACHE_DIRTY)) == (CACHE_VALID|CACHE_DIRTY) ) {
            unsigned char *cache_data = &ccn_ocache_data[entry&0x00003FE0];
            // Cache line is dirty - writeback. 
            ext_address_space[line->tag>>12]->write_burst(line->key, cache_data);
        }
        line->tag = val & 0x1FFFFC03;
        line->key = (val & 0x1FFFFC00)|(entry & 0x000003E0);
    }
}

struct mem_region_fn p4_region_ocache_addr = {
        ccn_ocache_addr_read, ccn_ocache_addr_write,
        unmapped_read_long, unmapped_write_long,
        unmapped_read_long, unmapped_write_long,
        unmapped_read_burst, unmapped_write_burst,
        unmapped_prefetch };


static int32_t FASTCALL ccn_ocache_data_read( sh4addr_t addr )
{
    int entry = (addr & 0x00003FFC);
    return *(uint32_t *)&ccn_ocache_data[entry];
}

static void FASTCALL ccn_ocache_data_write( sh4addr_t addr, uint32_t val )
{
    int entry = (addr & 0x00003FFC);
    *(uint32_t *)&ccn_ocache_data[entry] = val;
}

struct mem_region_fn p4_region_ocache_data = {
        ccn_ocache_data_read, ccn_ocache_data_write,
        unmapped_read_long, unmapped_write_long,
        unmapped_read_long, unmapped_write_long,
        unmapped_read_burst, unmapped_write_burst,
        unmapped_prefetch };


/****************** Cache control *********************/

void CCN_set_cache_control( int reg )
{
    uint32_t i;
    
    if( reg & CCR_ICI ) { /* icache invalidate */
        for( i=0; i<ICACHE_ENTRY_COUNT; i++ ) {
            ccn_icache[i].tag &= ~CACHE_VALID;
        }
    }
    
    if( reg & CCR_OCI ) { /* ocache invalidate */
        for( i=0; i<OCACHE_ENTRY_COUNT; i++ ) {
            ccn_ocache[i].tag &= ~(CACHE_VALID|CACHE_DIRTY);
        }
    }
    
    switch( reg & (CCR_OIX|CCR_ORA|CCR_OCE) ) {
    case MEM_OC_INDEX0: /* OIX=0 */
        for( i=OCRAM_START; i<OCRAM_END; i+=4 ) {
            sh4_address_space[i] = &mem_region_ocram_page0;
            sh4_address_space[i+1] = &mem_region_ocram_page0;
            sh4_address_space[i+2] = &mem_region_ocram_page1;
            sh4_address_space[i+3] = &mem_region_ocram_page1;
        }
        break;
    case MEM_OC_INDEX1: /* OIX=1 */
        for( i=OCRAM_START; i<OCRAM_MID; i++ )
            sh4_address_space[i] = &mem_region_ocram_page0;
        for( i=OCRAM_MID; i<OCRAM_END; i++ )
            sh4_address_space[i] = &mem_region_ocram_page1;
        break;
    default: /* disabled */
        for( i=OCRAM_START; i<OCRAM_END; i++ )
            sh4_address_space[i] = &mem_region_unmapped;
        break;
    }
}

/**
 * Prefetch for non-storequeue regions
 */
void FASTCALL ccn_prefetch( sh4addr_t addr )
{
    
}

/**
 * Prefetch for non-cached regions. Oddly enough, this does nothing whatsoever.
 */
void FASTCALL ccn_uncached_prefetch( sh4addr_t addr )
{
    
}
/********************************* Store-queue *******************************/
/*
 * The storequeue is strictly speaking part of the cache, but most of 
 * the complexity is actually around its addressing (ie in the MMU). The
 * methods here can assume we've already passed SQMD protection and the TLB
 * lookups (where appropriate).
 */  
void FASTCALL ccn_storequeue_write_long( sh4addr_t addr, uint32_t val )
{
    sh4r.store_queue[(addr>>2)&0xF] = val;
}
int32_t FASTCALL ccn_storequeue_read_long( sh4addr_t addr )
{
    return sh4r.store_queue[(addr>>2)&0xF];
}

/**
 * Variant used when tlb is disabled - address will be the original prefetch
 * address (ie 0xE0001234). Due to the way the SQ addressing is done, it can't
 * be hardcoded on 4K page boundaries, so we manually decode it here.
 */
void FASTCALL ccn_storequeue_prefetch( sh4addr_t addr ) 
{
    int queue = (addr&0x20)>>2;
    sh4ptr_t src = (sh4ptr_t)&sh4r.store_queue[queue];
    uint32_t hi = MMIO_READ( MMU, QACR0 + (queue>>1)) << 24;
    sh4addr_t target = (addr&0x03FFFFE0) | hi;
    ext_address_space[target>>12]->write_burst( target, src );
}

/**
 * Variant used when tlb is enabled - address in this case is already
 * mapped to the external target address.
 */
void FASTCALL ccn_storequeue_prefetch_tlb( sh4addr_t addr )
{
    int queue = (addr&0x20)>>2;
    sh4ptr_t src = (sh4ptr_t)&sh4r.store_queue[queue];
    ext_address_space[addr>>12]->write_burst( (addr & 0x1FFFFFE0), src );
}
