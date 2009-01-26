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
#include "clock.h"
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
struct cache_line ccn_ocache[OCACHE_ENTRY_COUNT];
static unsigned char ccn_icache_data[ICACHE_ENTRY_COUNT*32];
unsigned char ccn_ocache_data[OCACHE_ENTRY_COUNT*32];


/*********************** General module requirements ********************/

void CCN_reset()
{
    /* Clear everything for consistency */
    memset( ccn_icache, 0, sizeof(ccn_icache) );
    memset( ccn_ocache, 0, sizeof(ccn_icache) );
    memset( ccn_icache_data, 0, sizeof(ccn_icache) );
    memset( ccn_ocache_data, 0, sizeof(ccn_icache) );
}

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
        unmapped_prefetch, ocram_page0_read_byte };

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
        unmapped_prefetch, ocram_page1_read_byte };

/**************************** Cache functions ********************************/
char ccn_cache_map[16 MB]; // 24 bits of address space

/**
 * Load a 32-byte cache line from external memory at the given ext address.
 * @param addr external address pre-masked to 1FFFFFFE0 
 */
sh4addr_t FASTCALL ccn_ocache_load_line( sh4addr_t addr )
{
    int entry = addr & 0x00003FE0;
    struct cache_line *line = &ccn_ocache[entry>>5];
    char *cache_data = &ccn_ocache_data[entry];
    sh4addr_t old_addr = line->tag;
    line->tag = addr & 0x1FFFFFE0;
    char oldstate = ccn_cache_map[old_addr>>5];
    ccn_cache_map[old_addr>>5] = 0;
    ccn_cache_map[addr>>5] = CACHE_VALID;
    if( oldstate == (CACHE_VALID|CACHE_DIRTY) ) {
        // Cache line is dirty - writeback. 
        ext_address_space[old_addr>>12]->write_burst(old_addr, cache_data);
    }
    ext_address_space[addr>>12]->read_burst(cache_data, addr & 0x1FFFFFE0);
    return addr;
}

/* Long read through the operand cache */
/*
int32_t FASTCALL ccn_ocache_read_long( sh4addr_t addr );
int32_t FASTCALL ccn_ocache_read_word( sh4addr_t addr );
int32_t FASTCALL ccn_ocache_read_byte( sh4addr_t addr );
void FASTCALL ccn_ocache_write_long_copyback( sh4addr_t addr, uint32_t val );
void FASTCALL ccn_ocache_write_word_copyback( sh4addr_t addr, uint32_t val );
void FASTCALL ccn_ocache_write_byte_copyback( sh4addr_t addr, uint32_t val );

*/
static int32_t FASTCALL ccn_ocache_read_long( sh4addr_t addr )
{
    addr &= 0x1FFFFFFF;
    if( (ccn_cache_map[addr>>5] & CACHE_VALID) == 0 ) {
        ccn_ocache_load_line(addr);
    }
    return *(int32_t *)&ccn_ocache_data[addr & 0x3FFF];
}

static int32_t FASTCALL ccn_ocache_read_word( sh4addr_t addr )
{
    addr &= 0x1FFFFFFF;
    if( (ccn_cache_map[addr>>5] & CACHE_VALID) == 0 ) {
        ccn_ocache_load_line(addr);
    }
    return SIGNEXT16(*(int16_t *)&ccn_ocache_data[addr&0x3FFF]);    
}

static int32_t FASTCALL ccn_ocache_read_byte( sh4addr_t addr )
{
    addr &= 0x1FFFFFFF;
    if( (ccn_cache_map[addr>>5] & CACHE_VALID) == 0 ) {
        ccn_ocache_load_line(addr);
    }
    return SIGNEXT8(ccn_ocache_data[addr&0x3FFF]);        
}

static void FASTCALL ccn_ocache_write_long_copyback( sh4addr_t addr, uint32_t value )
{
    addr &= 0x1FFFFFFF;
    if( (ccn_cache_map[addr>>5] & CACHE_VALID) == 0 ) {
        ccn_ocache_load_line(addr);
    }
    ccn_cache_map[addr>>5] |= CACHE_DIRTY;
    *(uint32_t *)&ccn_ocache_data[addr&0x3FFF] = value;
}

static void FASTCALL ccn_ocache_write_word_copyback( sh4addr_t addr, uint32_t value )
{
    addr &= 0x1FFFFFFF;
    if( (ccn_cache_map[addr>>5] & CACHE_VALID) == 0 ) {
        ccn_ocache_load_line(addr);
    }
    ccn_cache_map[addr>>5] |= CACHE_DIRTY;
    *(uint16_t *)&ccn_ocache_data[addr&0x3FFF] = (uint16_t)value;
}

static void FASTCALL ccn_ocache_write_byte_copyback( sh4addr_t addr, uint32_t value )
{
    addr &= 0x1FFFFFFF;
    if( (ccn_cache_map[addr>>5] & CACHE_VALID) == 0 ) {
        ccn_ocache_load_line(addr);
    }
    ccn_cache_map[addr>>5] |= CACHE_DIRTY;
    ccn_ocache_data[addr&0x3FFF] = (uint8_t)value;
}

static void FASTCALL ccn_ocache_prefetch( sh4addr_t addr )
{
    addr &= 0x1FFFFFFF;
    if( (ccn_cache_map[addr>>5] & CACHE_VALID) == 0 ) {
        ccn_ocache_load_line(addr);
    }
}

void FASTCALL ccn_ocache_invalidate( sh4addr_t addr )
{
    addr &= 0x1FFFFFFF;
    ccn_cache_map[addr>>5] &= ~CACHE_VALID;
}

void FASTCALL ccn_ocache_purge( sh4addr_t addr )
{
    addr &= 0x1FFFFFE0;
    int oldflags = ccn_cache_map[addr>>5]; 
    ccn_cache_map[addr>>5] &= ~CACHE_VALID;
    if( oldflags == (CACHE_VALID|CACHE_DIRTY) ) {
        char *cache_data = &ccn_ocache_data[addr & 0x3FE0];
        ext_address_space[addr>>12]->write_burst(addr, cache_data);
    }
}

void FASTCALL ccn_ocache_writeback( sh4addr_t addr )
{
    addr &= 0x1FFFFFE0;
    if( ccn_cache_map[addr>>5] == (CACHE_VALID|CACHE_DIRTY) ) {
        ccn_cache_map[addr>>5] &= ~CACHE_DIRTY;
        char *cache_data = &ccn_ocache_data[addr & 0x3FE0];
        ext_address_space[addr>>12]->write_burst(addr, cache_data);
    }
}

struct mem_region_fn ccn_ocache_cb_region = {
    ccn_ocache_read_long, ccn_ocache_write_long_copyback,
    ccn_ocache_read_word, ccn_ocache_write_word_copyback,
    ccn_ocache_read_byte, ccn_ocache_write_byte_copyback,
    unmapped_read_burst, unmapped_write_burst,
    ccn_ocache_prefetch, ccn_ocache_read_byte };


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
        unmapped_prefetch, unmapped_read_long };


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
        unmapped_prefetch, unmapped_read_long };

static int32_t FASTCALL ccn_ocache_addr_read( sh4addr_t addr )
{
    int entry = (addr & 0x00003FE0);
    sh4addr_t tag = ccn_ocache[entry>>5].tag;
    return (tag&0x1FFFFC00) | ccn_cache_map[tag>>5];
}

static void FASTCALL ccn_ocache_addr_write( sh4addr_t addr, uint32_t val )
{
    int entry = (addr & 0x00003FE0);
    struct cache_line *line = &ccn_ocache[entry>>5];
    if( addr & 0x08 ) { // Associative
    } else {
        sh4addr_t tag = line->tag;
        if( ccn_cache_map[tag>>5] == (CACHE_VALID|CACHE_DIRTY) ) {
            // Cache line is dirty - writeback. 
            unsigned char *cache_data = &ccn_ocache_data[entry];
            ext_address_space[tag>>12]->write_burst(tag, cache_data);
        }
        line->tag = tag = (val & 0x1FFFFC00) | (addr & 0x3E0);
        ccn_cache_map[tag>>5] = val & 0x03;
    }
}

struct mem_region_fn p4_region_ocache_addr = {
        ccn_ocache_addr_read, ccn_ocache_addr_write,
        unmapped_read_long, unmapped_write_long,
        unmapped_read_long, unmapped_write_long,
        unmapped_read_burst, unmapped_write_burst,
        unmapped_prefetch, unmapped_read_long };


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
        unmapped_prefetch, unmapped_read_long };


/****************** Cache control *********************/

void CCN_set_cache_control( int reg )
{
    uint32_t i;
    
    if( reg & CCR_ICI ) { /* icache invalidate */
        for( i=0; i<ICACHE_ENTRY_COUNT; i++ ) {
            ccn_icache[i].key = -1;
            ccn_icache[i].tag &= ~CACHE_VALID;
        }
    }
    
    if( reg & CCR_OCI ) { /* ocache invalidate */
        for( i=0; i<OCACHE_ENTRY_COUNT; i++ ) {
            ccn_icache[i].key = -1;
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

/************************** Uncached memory access ***************************/
int32_t FASTCALL ccn_uncached_read_long( sh4addr_t addr )
{
    sh4r.slice_cycle += (4*sh4_bus_period);
    addr &= 0x1FFFFFFF;
    return ext_address_space[addr>>12]->read_long(addr);
}
int32_t FASTCALL ccn_uncached_read_word( sh4addr_t addr )
{
    sh4r.slice_cycle += (4*sh4_bus_period);
    addr &= 0x1FFFFFFF;
    return ext_address_space[addr>>12]->read_word(addr);
}
int32_t FASTCALL ccn_uncached_read_byte( sh4addr_t addr )
{
    sh4r.slice_cycle += (4*sh4_bus_period);
    addr &= 0x1FFFFFFF;
    return ext_address_space[addr>>12]->read_byte(addr);
}
void FASTCALL ccn_uncached_write_long( sh4addr_t addr, uint32_t val )
{
    sh4r.slice_cycle += (4*sh4_bus_period);
    addr &= 0x1FFFFFFF;
    return ext_address_space[addr>>12]->write_long(addr, val);
}
void FASTCALL ccn_uncached_write_word( sh4addr_t addr, uint32_t val )
{
    sh4r.slice_cycle += (4*sh4_bus_period);
    addr &= 0x1FFFFFFF;
    return ext_address_space[addr>>12]->write_word(addr, val);
}
void FASTCALL ccn_uncached_write_byte( sh4addr_t addr, uint32_t val )
{
    sh4r.slice_cycle += (4*sh4_bus_period);
    addr &= 0x1FFFFFFF;
    return ext_address_space[addr>>12]->write_byte(addr, val);
}
void FASTCALL ccn_uncached_prefetch( sh4addr_t addr )
{
}

struct mem_region_fn ccn_uncached_region = {
        ccn_uncached_read_long, ccn_uncached_write_long,
        ccn_uncached_read_word, ccn_uncached_write_word,
        ccn_uncached_read_byte, ccn_uncached_write_byte,
        unmapped_read_burst, unmapped_write_burst,
        ccn_uncached_prefetch, ccn_uncached_read_byte };


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

