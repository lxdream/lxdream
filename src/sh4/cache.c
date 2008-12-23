/**
 * $Id$
 * Implements the on-chip operand cache and instruction caches
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
        ocram_page0_read_burst, ocram_page0_write_burst };

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
        ocram_page1_read_burst, ocram_page1_write_burst };

/****************** Cache control *********************/

void CCN_set_cache_control( int reg )
{
    uint32_t i;
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