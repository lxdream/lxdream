/**
 * $Id: mmu.c,v 1.15 2007-11-08 11:54:16 nkeynes Exp $
 * 
 * MMU implementation
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

#include <stdio.h>
#include "sh4/sh4mmio.h"
#include "sh4/sh4core.h"
#include "mem.h"

#define OCRAM_START (0x1C000000>>PAGE_BITS)
#define OCRAM_END   (0x20000000>>PAGE_BITS)

#define ITLB_ENTRY_COUNT 4
#define UTLB_ENTRY_COUNT 64

/* Entry address */
#define TLB_VALID     0x00000100
#define TLB_USERMODE  0x00000040
#define TLB_WRITABLE  0x00000020
#define TLB_SIZE_MASK 0x00000090
#define TLB_SIZE_1K   0x00000000
#define TLB_SIZE_4K   0x00000010
#define TLB_SIZE_64K  0x00000080
#define TLB_SIZE_1M   0x00000090
#define TLB_CACHEABLE 0x00000008
#define TLB_DIRTY     0x00000004
#define TLB_SHARE     0x00000002
#define TLB_WRITETHRU 0x00000001


struct itlb_entry {
    sh4addr_t vpn; // Virtual Page Number
    uint32_t asid; // Process ID
    sh4addr_t ppn; // Physical Page Number
    uint32_t flags;
};

struct utlb_entry {
    sh4addr_t vpn; // Virtual Page Number
    uint32_t asid; // Process ID
    sh4addr_t ppn; // Physical Page Number
    uint32_t flags;
    uint32_t pcmcia; // extra pcmcia data - not used
};

static struct itlb_entry mmu_itlb[ITLB_ENTRY_COUNT];
static struct utlb_entry mmu_utlb[UTLB_ENTRY_COUNT];
static uint32_t mmu_urc;
static uint32_t mmu_urb;
static uint32_t mmu_lrui;

static sh4ptr_t cache = NULL;

static void mmu_invalidate_tlb();


int32_t mmio_region_MMU_read( uint32_t reg )
{
    switch( reg ) {
    case MMUCR:
	return MMIO_READ( MMU, MMUCR) | (mmu_urc<<10) | (mmu_urb<<18) | (mmu_lrui<<26);
    default:
	return MMIO_READ( MMU, reg );
    }
}

void mmio_region_MMU_write( uint32_t reg, uint32_t val )
{
    switch(reg) {
    case PTEH:
	val &= 0xFFFFFCFF;
	break;
    case PTEL:
	val &= 0x1FFFFDFF;
	break;
    case PTEA:
	val &= 0x0000000F;
	break;
    case MMUCR:
	if( val & MMUCR_TI ) {
	    mmu_invalidate_tlb();
	}
	mmu_urc = (val >> 10) & 0x3F;
	mmu_urb = (val >> 18) & 0x3F;
	mmu_lrui = (val >> 26) & 0x3F;
	val &= 0x00000301;
	break;
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
    fwrite( &mmu_itlb, sizeof(mmu_itlb), 1, f );
    fwrite( &mmu_utlb, sizeof(mmu_utlb), 1, f );
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
    if( fread( &mmu_itlb, sizeof(mmu_itlb), 1, f ) != 1 ) {
	return 1;
    }
    if( fread( &mmu_utlb, sizeof(mmu_utlb), 1, f ) != 1 ) {
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

/* TLB maintanence */

/**
 * LDTLB instruction implementation. Copies PTEH, PTEL and PTEA into the UTLB
 * entry identified by MMUCR.URC. Does not modify MMUCR or the ITLB.
 */
void MMU_ldtlb()
{
    mmu_utlb[mmu_urc].vpn = MMIO_READ(MMU, PTEH) & 0xFFFFFC00;
    mmu_utlb[mmu_urc].asid = MMIO_READ(MMU, PTEH) & 0x000000FF;
    mmu_utlb[mmu_urc].ppn = MMIO_READ(MMU, PTEL) & 0x1FFFFC00;
    mmu_utlb[mmu_urc].flags = MMIO_READ(MMU, PTEL) & 0x00001FF;
    mmu_utlb[mmu_urc].pcmcia = MMIO_READ(MMU, PTEA);
}

uint64_t mmu_translate_read( sh4addr_t addr )
{
    uint32_t mmucr = MMIO_READ(MMU,MMUCR);
    if( IS_SH4_PRIVMODE() ) {
	switch( addr & 0xE0000000 ) {
	case 0x80000000: case 0xA0000000:
	    /* Non-translated read P1,P2 */
	    break;
	case 0xE0000000:
	    /* Non-translated read P4 */
	    break;
	default:
	    if( mmucr&MMUCR_AT ) {
	    } else {
		// direct read
	    }
	}
    } else {
	if( addr & 0x80000000 ) {
	    if( ((addr&0xFC000000) == 0xE0000000 ) &&
		((mmucr&MMUCR_SQMD) == 0) ) { 
		// Store queue
		return 0;
	    }
//	    MMU_READ_ADDR_ERROR();
	}
	if( mmucr&MMUCR_AT ) {
	    uint32_t vpn = addr & 0xFFFFFC00;
	    uint32_t asid = MMIO_READ(MMU,PTEH)&0xFF;
	} else {
	    // direct read
	}
    }
}

static void mmu_invalidate_tlb()
{
    int i;
    for( i=0; i<ITLB_ENTRY_COUNT; i++ ) {
	mmu_itlb[i].flags &= (~TLB_VALID);
    }
    for( i=0; i<UTLB_ENTRY_COUNT; i++ ) {
	mmu_utlb[i].flags &= (~TLB_VALID);
    }
}

#define ITLB_ENTRY(addr) ((addr>>7)&0x03)

int32_t mmu_itlb_addr_read( sh4addr_t addr )
{
    struct itlb_entry *ent = &mmu_itlb[ITLB_ENTRY(addr)];
    return ent->vpn | ent->asid | (ent->flags & TLB_VALID);
}
int32_t mmu_itlb_data_read( sh4addr_t addr )
{
    struct itlb_entry *ent = &mmu_itlb[ITLB_ENTRY(addr)];
    return ent->ppn | ent->flags;
}

void mmu_itlb_addr_write( sh4addr_t addr, uint32_t val )
{
    struct itlb_entry *ent = &mmu_itlb[ITLB_ENTRY(addr)];
    ent->vpn = val & 0xFFFFFC00;
    ent->asid = val & 0x000000FF;
    ent->flags = (ent->flags & ~(TLB_VALID)) | (val&TLB_VALID);
}

void mmu_itlb_data_write( sh4addr_t addr, uint32_t val )
{
    struct itlb_entry *ent = &mmu_itlb[ITLB_ENTRY(addr)];
    ent->ppn = val & 0x1FFFFC00;
    ent->flags = val & 0x00001DA;
}

#define UTLB_ENTRY(addr) ((addr>>8)&0x3F)
#define UTLB_ASSOC(addr) (addr&0x80)
#define UTLB_DATA2(addr) (addr&0x00800000)

int32_t mmu_utlb_addr_read( sh4addr_t addr )
{
    struct utlb_entry *ent = &mmu_utlb[UTLB_ENTRY(addr)];
    return ent->vpn | ent->asid | (ent->flags & TLB_VALID) |
	((ent->flags & TLB_DIRTY)<<7);
}
int32_t mmu_utlb_data_read( sh4addr_t addr )
{
    struct utlb_entry *ent = &mmu_utlb[UTLB_ENTRY(addr)];
    if( UTLB_DATA2(addr) ) {
	return ent->pcmcia;
    } else {
	return ent->ppn | ent->flags;
    }
}

void mmu_utlb_addr_write( sh4addr_t addr, uint32_t val )
{
    if( UTLB_ASSOC(addr) ) {
    } else {
	struct utlb_entry *ent = &mmu_utlb[UTLB_ENTRY(addr)];
	ent->vpn = (val & 0xFFFFFC00);
	ent->asid = (val & 0xFF);
	ent->flags = (ent->flags & ~(TLB_DIRTY|TLB_VALID));
	ent->flags |= (val & TLB_VALID);
	ent->flags |= ((val & 0x200)>>7);
    }
}

void mmu_utlb_data_write( sh4addr_t addr, uint32_t val )
{
    struct utlb_entry *ent = &mmu_utlb[UTLB_ENTRY(addr)];
    if( UTLB_DATA2(addr) ) {
	ent->pcmcia = val & 0x0000000F;
    } else {
	ent->ppn = (val & 0x1FFFFC00);
	ent->flags = (val & 0x000001FF);
    }
}

/* Cache access - not implemented */

int32_t mmu_icache_addr_read( sh4addr_t addr )
{
    return 0; // not implemented
}
int32_t mmu_icache_data_read( sh4addr_t addr )
{
    return 0; // not implemented
}
int32_t mmu_ocache_addr_read( sh4addr_t addr )
{
    return 0; // not implemented
}
int32_t mmu_ocache_data_read( sh4addr_t addr )
{
    return 0; // not implemented
}

void mmu_icache_addr_write( sh4addr_t addr, uint32_t val )
{
}

void mmu_icache_data_write( sh4addr_t addr, uint32_t val )
{
}

void mmu_ocache_addr_write( sh4addr_t addr, uint32_t val )
{
}

void mmu_ocache_data_write( sh4addr_t addr, uint32_t val )
{
}
