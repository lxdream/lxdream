/**
 * $Id: sh4mmio.c,v 1.10 2007-01-23 08:17:06 nkeynes Exp $
 * 
 * Miscellaneous and not-really-implemented SH4 peripheral modules. Also
 * responsible for including the IMPL side of the SH4 MMIO pages.
 * Most of these will eventually be split off into their own files.
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
#define MODULE sh4_module

#include "dream.h"
#include "mem.h"
#include "clock.h"
#include "sh4core.h"
#include "sh4mmio.h"
#define MMIO_IMPL
#include "sh4mmio.h"

/********************************* MMU *************************************/

MMIO_REGION_READ_DEFFN( MMU )

#define OCRAM_START (0x1C000000>>PAGE_BITS)
#define OCRAM_END   (0x20000000>>PAGE_BITS)

static char *cache = NULL;

void mmio_region_MMU_write( uint32_t reg, uint32_t val )
{
    switch(reg) {
        case CCR:
            mmu_set_cache_mode( val & (CCR_OIX|CCR_ORA) );
            break;
        default:
            break;
    }
    MMIO_WRITE( MMU, reg, val );
}


void MMU_init() 
{
    cache = mem_alloc_pages(2);
}

void MMU_reset()
{
    mmio_region_MMU_write( CCR, 0 );
}

void MMU_save_state( FILE *f )
{
    fwrite( cache, 4096, 2, f );
}

int MMU_load_state( FILE *f )
{
    /* Setup the cache mode according to the saved register value
     * (mem_load runs before this point to load all MMIO data)
     */
    mmio_region_MMU_write( CCR, MMIO_READ(MMU, CCR) );
    if( fread( cache, 4096, 2, f ) != 2 ) {
	return 1;
    }
    return 0;
}

void mmu_set_cache_mode( int mode )
{
    uint32_t i;
    switch( mode ) {
        case MEM_OC_INDEX0: /* OIX=0 */
            for( i=OCRAM_START; i<OCRAM_END; i++ )
                page_map[i] = cache + ((i&0x02)<<(PAGE_BITS-1));
            break;
        case MEM_OC_INDEX1: /* OIX=1 */
            for( i=OCRAM_START; i<OCRAM_END; i++ )
                page_map[i] = cache + ((i&0x02000000)>>(25-PAGE_BITS));
            break;
        default: /* disabled */
            for( i=OCRAM_START; i<OCRAM_END; i++ )
                page_map[i] = NULL;
            break;
    }
}


/********************************* BSC *************************************/

uint16_t bsc_output_mask_lo = 0, bsc_output_mask_hi = 0;
uint16_t bsc_input_mask_lo = 0, bsc_input_mask_hi = 0;
uint32_t bsc_output = 0, bsc_input = 0x0300;

void bsc_out( int output, int mask )
{
    /* Go figure... The BIOS won't start without this mess though */
    if( ((output | (~mask)) & 0x03) == 3 ) {
        bsc_output |= 0x03;
    } else {
        bsc_output &= ~0x03;
    }
}

void mmio_region_BSC_write( uint32_t reg, uint32_t val )
{
    int i;
    switch( reg ) {
        case PCTRA:
            bsc_input_mask_lo = bsc_output_mask_lo = 0;
            for( i=0; i<16; i++ ) {
                int bits = (val >> (i<<1)) & 0x03;
                if( bits == 2 ) bsc_input_mask_lo |= (1<<i);
                else if( bits != 0 ) bsc_output_mask_lo |= (1<<i);
            }
            bsc_output = (bsc_output&0x000F0000) |
                (MMIO_READ( BSC, PDTRA ) & bsc_output_mask_lo);
            bsc_out( MMIO_READ( BSC, PDTRA ) | ((MMIO_READ(BSC,PDTRB)<<16)),
                     bsc_output_mask_lo | (bsc_output_mask_hi<<16) );
            break;
        case PCTRB:
            bsc_input_mask_hi = bsc_output_mask_hi = 0;
            for( i=0; i<4; i++ ) {
                int bits = (val >> (i>>1)) & 0x03;
                if( bits == 2 ) bsc_input_mask_hi |= (1<<i);
                else if( bits != 0 ) bsc_output_mask_hi |= (1<<i);
            }
            bsc_output = (bsc_output&0xFFFF) |
                ((MMIO_READ( BSC, PDTRA ) & bsc_output_mask_hi)<<16);
            break;
        case PDTRA:
            bsc_output = (bsc_output&0x000F0000) |
                (val & bsc_output_mask_lo );
            bsc_out( val | ((MMIO_READ(BSC,PDTRB)<<16)),
                     bsc_output_mask_lo | (bsc_output_mask_hi<<16) );
            break;
        case PDTRB:
            bsc_output = (bsc_output&0xFFFF) |
                ( (val & bsc_output_mask_hi)<<16 );
            break;
    }
    WARN( "Write to (mostly) unimplemented BSC (%03X <= %08X) [%s: %s]",
          reg, val, MMIO_REGID(BSC,reg), MMIO_REGDESC(BSC,reg) );
    MMIO_WRITE( BSC, reg, val );
}

int32_t mmio_region_BSC_read( uint32_t reg )
{
    int32_t val;
    switch( reg ) {
        case PDTRA:
            val = (bsc_input & bsc_input_mask_lo) | (bsc_output&0xFFFF);
            break;
        case PDTRB:
            val = ((bsc_input>>16) & bsc_input_mask_hi) | (bsc_output>>16);
            break;
        default:
            val = MMIO_READ( BSC, reg );
    }
    WARN( "Read from (mostly) unimplemented BSC (%03X => %08X) [%s: %s]",
          reg, val, MMIO_REGID(BSC,reg), MMIO_REGDESC(BSC,reg) );
    return val;
}

/********************************* UBC *************************************/

MMIO_REGION_STUBFNS( UBC )


/********************************** SCI *************************************/

MMIO_REGION_STUBFNS( SCI )

