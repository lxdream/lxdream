/**
 * $Id$
 *
 * SH4 MMU implementation based on address space page maps. This module
 * is responsible for all address decoding functions. 
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
#include <assert.h>
#include "sh4/sh4mmio.h"
#include "sh4/sh4core.h"
#include "sh4/sh4trans.h"
#include "dreamcast.h"
#include "mem.h"
#include "mmu.h"

#define RAISE_TLB_ERROR(code, vpn) sh4_raise_tlb_exception(code, vpn)
#define RAISE_MEM_ERROR(code, vpn) \
    MMIO_WRITE(MMU, TEA, vpn); \
    MMIO_WRITE(MMU, PTEH, ((MMIO_READ(MMU, PTEH) & 0x000003FF) | (vpn&0xFFFFFC00))); \
    sh4_raise_exception(code);
#define RAISE_TLB_MULTIHIT_ERROR(vpn) sh4_raise_tlb_multihit(vpn)

/* An entry is a 1K entry if it's one of the mmu_utlb_1k_pages entries */
#define IS_1K_PAGE_ENTRY(ent)  ( ((uintptr_t)(((struct utlb_1k_entry *)ent) - &mmu_utlb_1k_pages[0])) < UTLB_ENTRY_COUNT )

/* Primary address space (used directly by SH4 cores) */
mem_region_fn_t *sh4_address_space;
mem_region_fn_t *sh4_user_address_space;

/* Accessed from the UTLB accessor methods */
uint32_t mmu_urc;
uint32_t mmu_urb;
static gboolean mmu_urc_overflow; /* If true, urc was set >= urb */  

/* Module globals */
static struct itlb_entry mmu_itlb[ITLB_ENTRY_COUNT];
static struct utlb_entry mmu_utlb[UTLB_ENTRY_COUNT];
static struct utlb_page_entry mmu_utlb_pages[UTLB_ENTRY_COUNT];
static uint32_t mmu_lrui;
static uint32_t mmu_asid; // current asid
static struct utlb_default_regions *mmu_user_storequeue_regions;

/* Structures for 1K page handling */
static struct utlb_1k_entry mmu_utlb_1k_pages[UTLB_ENTRY_COUNT];
static int mmu_utlb_1k_free_list[UTLB_ENTRY_COUNT];
static int mmu_utlb_1k_free_index;


/* Function prototypes */
static void mmu_invalidate_tlb();
static void mmu_utlb_register_all();
static void mmu_utlb_remove_entry(int);
static void mmu_utlb_insert_entry(int);
static void mmu_register_mem_region( uint32_t start, uint32_t end, mem_region_fn_t fn );
static void mmu_register_user_mem_region( uint32_t start, uint32_t end, mem_region_fn_t fn );
static void mmu_set_tlb_enabled( int tlb_on );
static void mmu_set_tlb_asid( uint32_t asid );
static void mmu_set_storequeue_protected( int protected, int tlb_on );
static gboolean mmu_utlb_map_pages( mem_region_fn_t priv_page, mem_region_fn_t user_page, sh4addr_t start_addr, int npages );
static void mmu_utlb_remap_pages( gboolean remap_priv, gboolean remap_user, int entryNo );
static gboolean mmu_utlb_unmap_pages( gboolean unmap_priv, gboolean unmap_user, sh4addr_t start_addr, int npages );
static gboolean mmu_ext_page_remapped( sh4addr_t page, mem_region_fn_t fn, void *user_data );
static void mmu_utlb_1k_init();
static struct utlb_1k_entry *mmu_utlb_1k_alloc();
static void mmu_utlb_1k_free( struct utlb_1k_entry *entry );
static int mmu_read_urc();

static void FASTCALL tlb_miss_read( sh4addr_t addr, void *exc );
static int32_t FASTCALL tlb_protected_read( sh4addr_t addr, void *exc );
static void FASTCALL tlb_protected_write( sh4addr_t addr, uint32_t val, void *exc );
static void FASTCALL tlb_initial_write( sh4addr_t addr, uint32_t val, void *exc );
static uint32_t get_tlb_size_mask( uint32_t flags );
static uint32_t get_tlb_size_pages( uint32_t flags );

#define DEFAULT_REGIONS 0
#define DEFAULT_STOREQUEUE_REGIONS 1
#define DEFAULT_STOREQUEUE_SQMD_REGIONS 2

static struct utlb_default_regions mmu_default_regions[3] = {
        { &mem_region_tlb_miss, &mem_region_tlb_protected, &mem_region_tlb_multihit },
        { &p4_region_storequeue_miss, &p4_region_storequeue_protected, &p4_region_storequeue_multihit },
        { &p4_region_storequeue_sqmd_miss, &p4_region_storequeue_sqmd_protected, &p4_region_storequeue_sqmd_multihit } };

#define IS_STOREQUEUE_PROTECTED() (mmu_user_storequeue_regions == &mmu_default_regions[DEFAULT_STOREQUEUE_SQMD_REGIONS])

/*********************** Module public functions ****************************/

/**
 * Allocate memory for the address space maps, and initialize them according
 * to the default (reset) values. (TLB is disabled by default)
 */
                           
void MMU_init()
{
    sh4_address_space = mem_alloc_pages( sizeof(mem_region_fn_t) * 256 );
    sh4_user_address_space = mem_alloc_pages( sizeof(mem_region_fn_t) * 256 );
    mmu_user_storequeue_regions = &mmu_default_regions[DEFAULT_STOREQUEUE_REGIONS];
    
    mmu_set_tlb_enabled(0);
    mmu_register_user_mem_region( 0x80000000, 0x00000000, &mem_region_address_error );
    mmu_register_user_mem_region( 0xE0000000, 0xE4000000, &p4_region_storequeue );                                
    
    /* Setup P4 tlb/cache access regions */
    mmu_register_mem_region( 0xE0000000, 0xE4000000, &p4_region_storequeue );
    mmu_register_mem_region( 0xE4000000, 0xF0000000, &mem_region_unmapped );
    mmu_register_mem_region( 0xF0000000, 0xF1000000, &p4_region_icache_addr );
    mmu_register_mem_region( 0xF1000000, 0xF2000000, &p4_region_icache_data );
    mmu_register_mem_region( 0xF2000000, 0xF3000000, &p4_region_itlb_addr );
    mmu_register_mem_region( 0xF3000000, 0xF4000000, &p4_region_itlb_data );
    mmu_register_mem_region( 0xF4000000, 0xF5000000, &p4_region_ocache_addr );
    mmu_register_mem_region( 0xF5000000, 0xF6000000, &p4_region_ocache_data );
    mmu_register_mem_region( 0xF6000000, 0xF7000000, &p4_region_utlb_addr );
    mmu_register_mem_region( 0xF7000000, 0xF8000000, &p4_region_utlb_data );
    mmu_register_mem_region( 0xF8000000, 0x00000000, &mem_region_unmapped );
    
    /* Setup P4 control region */
    mmu_register_mem_region( 0xFF000000, 0xFF001000, &mmio_region_MMU.fn );
    mmu_register_mem_region( 0xFF100000, 0xFF101000, &mmio_region_PMM.fn );
    mmu_register_mem_region( 0xFF200000, 0xFF201000, &mmio_region_UBC.fn );
    mmu_register_mem_region( 0xFF800000, 0xFF801000, &mmio_region_BSC.fn );
    mmu_register_mem_region( 0xFF900000, 0xFFA00000, &mem_region_unmapped ); // SDMR2 + SDMR3
    mmu_register_mem_region( 0xFFA00000, 0xFFA01000, &mmio_region_DMAC.fn );
    mmu_register_mem_region( 0xFFC00000, 0xFFC01000, &mmio_region_CPG.fn );
    mmu_register_mem_region( 0xFFC80000, 0xFFC81000, &mmio_region_RTC.fn );
    mmu_register_mem_region( 0xFFD00000, 0xFFD01000, &mmio_region_INTC.fn );
    mmu_register_mem_region( 0xFFD80000, 0xFFD81000, &mmio_region_TMU.fn );
    mmu_register_mem_region( 0xFFE00000, 0xFFE01000, &mmio_region_SCI.fn );
    mmu_register_mem_region( 0xFFE80000, 0xFFE81000, &mmio_region_SCIF.fn );
    mmu_register_mem_region( 0xFFF00000, 0xFFF01000, &mem_region_unmapped ); // H-UDI
    
    register_mem_page_remapped_hook( mmu_ext_page_remapped, NULL );
    mmu_utlb_1k_init();
    
    /* Ensure the code regions are executable (64-bit only). Although it might
     * be more portable to mmap these at runtime rather than using static decls
     */
#if SIZEOF_VOID_P == 8
    mem_unprotect( mmu_utlb_pages, sizeof(mmu_utlb_pages) );
    mem_unprotect( mmu_utlb_1k_pages, sizeof(mmu_utlb_1k_pages) );
#endif
}

void MMU_reset()
{
    mmio_region_MMU_write( CCR, 0 );
    mmio_region_MMU_write( MMUCR, 0 );
}

void MMU_save_state( FILE *f )
{
    mmu_read_urc();   
    fwrite( &mmu_itlb, sizeof(mmu_itlb), 1, f );
    fwrite( &mmu_utlb, sizeof(mmu_utlb), 1, f );
    fwrite( &mmu_urc, sizeof(mmu_urc), 1, f );
    fwrite( &mmu_urb, sizeof(mmu_urb), 1, f );
    fwrite( &mmu_lrui, sizeof(mmu_lrui), 1, f );
    fwrite( &mmu_asid, sizeof(mmu_asid), 1, f );
}

int MMU_load_state( FILE *f )
{
    if( fread( &mmu_itlb, sizeof(mmu_itlb), 1, f ) != 1 ) {
        return 1;
    }
    if( fread( &mmu_utlb, sizeof(mmu_utlb), 1, f ) != 1 ) {
        return 1;
    }
    if( fread( &mmu_urc, sizeof(mmu_urc), 1, f ) != 1 ) {
        return 1;
    }
    if( fread( &mmu_urc, sizeof(mmu_urb), 1, f ) != 1 ) {
        return 1;
    }
    if( fread( &mmu_lrui, sizeof(mmu_lrui), 1, f ) != 1 ) {
        return 1;
    }
    if( fread( &mmu_asid, sizeof(mmu_asid), 1, f ) != 1 ) {
        return 1;
    }

    uint32_t mmucr = MMIO_READ(MMU,MMUCR);
    mmu_urc_overflow = mmu_urc >= mmu_urb;
    mmu_set_tlb_enabled(mmucr&MMUCR_AT);
    mmu_set_storequeue_protected(mmucr&MMUCR_SQMD, mmucr&MMUCR_AT);
    return 0;
}

/**
 * LDTLB instruction implementation. Copies PTEH, PTEL and PTEA into the UTLB
 * entry identified by MMUCR.URC. Does not modify MMUCR or the ITLB.
 */
void MMU_ldtlb()
{
    int urc = mmu_read_urc();
    if( mmu_utlb[urc].flags & TLB_VALID )
        mmu_utlb_remove_entry( urc );
    mmu_utlb[urc].vpn = MMIO_READ(MMU, PTEH) & 0xFFFFFC00;
    mmu_utlb[urc].asid = MMIO_READ(MMU, PTEH) & 0x000000FF;
    mmu_utlb[urc].ppn = MMIO_READ(MMU, PTEL) & 0x1FFFFC00;
    mmu_utlb[urc].flags = MMIO_READ(MMU, PTEL) & 0x00001FF;
    mmu_utlb[urc].pcmcia = MMIO_READ(MMU, PTEA);
    mmu_utlb[urc].mask = get_tlb_size_mask(mmu_utlb[urc].flags);
    if( mmu_utlb[urc].flags & TLB_VALID )
        mmu_utlb_insert_entry( urc );
}


MMIO_REGION_READ_FN( MMU, reg )
{
    reg &= 0xFFF;
    switch( reg ) {
    case MMUCR:
        return MMIO_READ( MMU, MMUCR) | (mmu_read_urc()<<10) | ((mmu_urb&0x3F)<<18) | (mmu_lrui<<26);
    default:
        return MMIO_READ( MMU, reg );
    }
}

MMIO_REGION_WRITE_FN( MMU, reg, val )
{
    uint32_t tmp;
    reg &= 0xFFF;
    switch(reg) {
    case SH4VER:
        return;
    case PTEH:
        val &= 0xFFFFFCFF;
        if( (val & 0xFF) != mmu_asid ) {
            mmu_set_tlb_asid( val&0xFF );
            sh4_icache.page_vma = -1; // invalidate icache as asid has changed
        }
        break;
    case PTEL:
        val &= 0x1FFFFDFF;
        break;
    case PTEA:
        val &= 0x0000000F;
        break;
    case TRA:
        val &= 0x000003FC;
        break;
    case EXPEVT:
    case INTEVT:
        val &= 0x00000FFF;
        break;
    case MMUCR:
        if( val & MMUCR_TI ) {
            mmu_invalidate_tlb();
        }
        mmu_urc = (val >> 10) & 0x3F;
        mmu_urb = (val >> 18) & 0x3F;
        if( mmu_urb == 0 ) {
            mmu_urb = 0x40;
        } else if( mmu_urc >= mmu_urb ) {
            mmu_urc_overflow = TRUE;
        }
        mmu_lrui = (val >> 26) & 0x3F;
        val &= 0x00000301;
        tmp = MMIO_READ( MMU, MMUCR );
        if( (val ^ tmp) & (MMUCR_SQMD) ) {
            mmu_set_storequeue_protected( val & MMUCR_SQMD, val&MMUCR_AT );
        }
        if( (val ^ tmp) & (MMUCR_AT) ) {
            // AT flag has changed state - flush the xlt cache as all bets
            // are off now. We also need to force an immediate exit from the
            // current block
            mmu_set_tlb_enabled( val & MMUCR_AT );
            MMIO_WRITE( MMU, MMUCR, val );
            sh4_core_exit( CORE_EXIT_FLUSH_ICACHE );
            xlat_flush_cache(); // If we're not running, flush the cache anyway
        }
        break;
    case CCR:
        CCN_set_cache_control( val );
        val &= 0x81A7;
        break;
    case MMUUNK1:
        /* Note that if the high bit is set, this appears to reset the machine.
         * Not emulating this behaviour yet until we know why...
         */
        val &= 0x00010007;
        break;
    case QACR0:
    case QACR1:
        val &= 0x0000001C;
        break;
    case PMCR1:
        PMM_write_control(0, val);
        val &= 0x0000C13F;
        break;
    case PMCR2:
        PMM_write_control(1, val);
        val &= 0x0000C13F;
        break;
    default:
        break;
    }
    MMIO_WRITE( MMU, reg, val );
}

/********************** 1K Page handling ***********************/
/* Since we use 4K pages as our native page size, 1K pages need a bit of extra
 * effort to manage - we justify this on the basis that most programs won't
 * actually use 1K pages, so we may as well optimize for the common case.
 * 
 * Implementation uses an intermediate page entry (the utlb_1k_entry) that
 * redirects requests to the 'real' page entry. These are allocated on an
 * as-needed basis, and returned to the pool when all subpages are empty.
 */ 
static void mmu_utlb_1k_init()
{
    int i;
    for( i=0; i<UTLB_ENTRY_COUNT; i++ ) {
        mmu_utlb_1k_free_list[i] = i;
        mmu_utlb_1k_init_vtable( &mmu_utlb_1k_pages[i] );
    }
    mmu_utlb_1k_free_index = 0;
}

static struct utlb_1k_entry *mmu_utlb_1k_alloc()
{
    assert( mmu_utlb_1k_free_index < UTLB_ENTRY_COUNT );
    struct utlb_1k_entry *entry = &mmu_utlb_1k_pages[mmu_utlb_1k_free_list[mmu_utlb_1k_free_index++]];
    return entry;
}    

static void mmu_utlb_1k_free( struct utlb_1k_entry *ent )
{
    unsigned int entryNo = ent - &mmu_utlb_1k_pages[0];
    assert( entryNo < UTLB_ENTRY_COUNT );
    assert( mmu_utlb_1k_free_index > 0 );
    mmu_utlb_1k_free_list[--mmu_utlb_1k_free_index] = entryNo;
}


/********************** Address space maintenance *************************/

/**
 * MMU accessor functions just increment URC - fixup here if necessary
 */
static int mmu_read_urc()
{
    if( mmu_urc_overflow ) {
        if( mmu_urc >= 0x40 ) {
            mmu_urc_overflow = FALSE;
            mmu_urc -= 0x40;
            mmu_urc %= mmu_urb;
        }
    } else {
        mmu_urc %= mmu_urb;
    }
    return mmu_urc;
}

static void mmu_register_mem_region( uint32_t start, uint32_t end, mem_region_fn_t fn )
{
    int count = (end - start) >> 12;
    mem_region_fn_t *ptr = &sh4_address_space[start>>12];
    while( count-- > 0 ) {
        *ptr++ = fn;
    }
}
static void mmu_register_user_mem_region( uint32_t start, uint32_t end, mem_region_fn_t fn )
{
    int count = (end - start) >> 12;
    mem_region_fn_t *ptr = &sh4_user_address_space[start>>12];
    while( count-- > 0 ) {
        *ptr++ = fn;
    }
}

static gboolean mmu_ext_page_remapped( sh4addr_t page, mem_region_fn_t fn, void *user_data )
{
    int i;
    if( (MMIO_READ(MMU,MMUCR)) & MMUCR_AT ) {
        /* TLB on */
        sh4_address_space[(page|0x80000000)>>12] = fn; /* Direct map to P1 and P2 */
        sh4_address_space[(page|0xA0000000)>>12] = fn;
        /* Scan UTLB and update any direct-referencing entries */
    } else {
        /* Direct map to U0, P0, P1, P2, P3 */
        for( i=0; i<= 0xC0000000; i+= 0x20000000 ) {
            sh4_address_space[(page|i)>>12] = fn;
        }
        for( i=0; i < 0x80000000; i+= 0x20000000 ) {
            sh4_user_address_space[(page|i)>>12] = fn;
        }
    }
    return TRUE;
}

static void mmu_set_tlb_enabled( int tlb_on )
{
    mem_region_fn_t *ptr, *uptr;
    int i;
    
    /* Reset the storequeue area */

    if( tlb_on ) {
        mmu_register_mem_region(0x00000000, 0x80000000, &mem_region_tlb_miss );
        mmu_register_mem_region(0xC0000000, 0xE0000000, &mem_region_tlb_miss );
        mmu_register_user_mem_region(0x00000000, 0x80000000, &mem_region_tlb_miss );
        
        /* Default SQ prefetch goes to TLB miss (?) */
        mmu_register_mem_region( 0xE0000000, 0xE4000000, &p4_region_storequeue_miss );
        mmu_register_user_mem_region( 0xE0000000, 0xE4000000, mmu_user_storequeue_regions->tlb_miss );
        mmu_utlb_register_all();
    } else {
        for( i=0, ptr = sh4_address_space; i<7; i++, ptr += LXDREAM_PAGE_TABLE_ENTRIES ) {
            memcpy( ptr, ext_address_space, sizeof(mem_region_fn_t) * LXDREAM_PAGE_TABLE_ENTRIES );
        }
        for( i=0, ptr = sh4_user_address_space; i<4; i++, ptr += LXDREAM_PAGE_TABLE_ENTRIES ) {
            memcpy( ptr, ext_address_space, sizeof(mem_region_fn_t) * LXDREAM_PAGE_TABLE_ENTRIES );
        }

        mmu_register_mem_region( 0xE0000000, 0xE4000000, &p4_region_storequeue );
        if( IS_STOREQUEUE_PROTECTED() ) {
            mmu_register_user_mem_region( 0xE0000000, 0xE4000000, &p4_region_storequeue_sqmd );
        } else {
            mmu_register_user_mem_region( 0xE0000000, 0xE4000000, &p4_region_storequeue );
        }
    }
    
}

/**
 * Flip the SQMD switch - this is rather expensive, so will need to be changed if
 * anything expects to do this frequently.
 */
static void mmu_set_storequeue_protected( int protected, int tlb_on ) 
{
    mem_region_fn_t nontlb_region;
    int i;

    if( protected ) {
        mmu_user_storequeue_regions = &mmu_default_regions[DEFAULT_STOREQUEUE_SQMD_REGIONS];
        nontlb_region = &p4_region_storequeue_sqmd;
    } else {
        mmu_user_storequeue_regions = &mmu_default_regions[DEFAULT_STOREQUEUE_REGIONS];
        nontlb_region = &p4_region_storequeue; 
    }

    if( tlb_on ) {
        mmu_register_user_mem_region( 0xE0000000, 0xE4000000, mmu_user_storequeue_regions->tlb_miss );
        for( i=0; i<UTLB_ENTRY_COUNT; i++ ) {
            if( (mmu_utlb[i].vpn & 0xFC000000) == 0xE0000000 ) {
                mmu_utlb_insert_entry(i);
            }
        }
    } else {
        mmu_register_user_mem_region( 0xE0000000, 0xE4000000, nontlb_region ); 
    }
    
}

static void mmu_set_tlb_asid( uint32_t asid )
{
    /* Scan for pages that need to be remapped */
    int i;
    if( IS_SV_ENABLED() ) {
        for( i=0; i<UTLB_ENTRY_COUNT; i++ ) {
            if( mmu_utlb[i].asid == mmu_asid && 
                (mmu_utlb[i].flags & (TLB_VALID|TLB_SHARE)) == (TLB_VALID) ) {
                // Matches old ASID - unmap out
                if( !mmu_utlb_unmap_pages( FALSE, TRUE, mmu_utlb[i].vpn&mmu_utlb[i].mask,
                        get_tlb_size_pages(mmu_utlb[i].flags) ) )
                    mmu_utlb_remap_pages( FALSE, TRUE, i );
            }
        }
        for( i=0; i<UTLB_ENTRY_COUNT; i++ ) {
            if( mmu_utlb[i].asid == asid && 
                (mmu_utlb[i].flags & (TLB_VALID|TLB_SHARE)) == (TLB_VALID) ) {
                // Matches new ASID - map in
                mmu_utlb_map_pages( NULL, mmu_utlb_pages[i].user_fn, 
                        mmu_utlb[i].vpn&mmu_utlb[i].mask, 
                        get_tlb_size_pages(mmu_utlb[i].flags) );
            }
        }
    } else {
        // Remap both Priv+user pages
        for( i=0; i<UTLB_ENTRY_COUNT; i++ ) {
            if( mmu_utlb[i].asid == mmu_asid &&
                (mmu_utlb[i].flags & (TLB_VALID|TLB_SHARE)) == (TLB_VALID) ) {
                if( !mmu_utlb_unmap_pages( TRUE, TRUE, mmu_utlb[i].vpn&mmu_utlb[i].mask,
                        get_tlb_size_pages(mmu_utlb[i].flags) ) )
                    mmu_utlb_remap_pages( TRUE, TRUE, i );
            }
        }
        for( i=0; i<UTLB_ENTRY_COUNT; i++ ) {
            if( mmu_utlb[i].asid == asid &&
                (mmu_utlb[i].flags & (TLB_VALID|TLB_SHARE)) == (TLB_VALID) ) {
                mmu_utlb_map_pages( &mmu_utlb_pages[i].fn, mmu_utlb_pages[i].user_fn, 
                        mmu_utlb[i].vpn&mmu_utlb[i].mask, 
                        get_tlb_size_pages(mmu_utlb[i].flags) );  
            }
        }
    }
    
    mmu_asid = asid;
}

static uint32_t get_tlb_size_mask( uint32_t flags )
{
    switch( flags & TLB_SIZE_MASK ) {
    case TLB_SIZE_1K: return MASK_1K;
    case TLB_SIZE_4K: return MASK_4K;
    case TLB_SIZE_64K: return MASK_64K;
    case TLB_SIZE_1M: return MASK_1M;
    default: return 0; /* Unreachable */
    }
}
static uint32_t get_tlb_size_pages( uint32_t flags )
{
    switch( flags & TLB_SIZE_MASK ) {
    case TLB_SIZE_1K: return 0;
    case TLB_SIZE_4K: return 1;
    case TLB_SIZE_64K: return 16;
    case TLB_SIZE_1M: return 256;
    default: return 0; /* Unreachable */
    }
}

/**
 * Add a new TLB entry mapping to the address space table. If any of the pages
 * are already mapped, they are mapped to the TLB multi-hit page instead.
 * @return FALSE if a TLB multihit situation was detected, otherwise TRUE.
 */ 
static gboolean mmu_utlb_map_pages( mem_region_fn_t priv_page, mem_region_fn_t user_page, sh4addr_t start_addr, int npages )
{
    mem_region_fn_t *ptr = &sh4_address_space[start_addr >> 12];
    mem_region_fn_t *uptr = &sh4_user_address_space[start_addr >> 12];
    struct utlb_default_regions *privdefs = &mmu_default_regions[DEFAULT_REGIONS];
    struct utlb_default_regions *userdefs = privdefs;    
    
    gboolean mapping_ok = TRUE;
    int i;
    
    if( (start_addr & 0xFC000000) == 0xE0000000 ) {
        /* Storequeue mapping */
        privdefs = &mmu_default_regions[DEFAULT_STOREQUEUE_REGIONS];
        userdefs = mmu_user_storequeue_regions;
    } else if( (start_addr & 0xE0000000) == 0xC0000000 ) {
        user_page = NULL; /* No user access to P3 region */
    } else if( start_addr >= 0x80000000 ) {
        return TRUE; // No mapping - legal but meaningless
    }

    if( npages == 0 ) {
        struct utlb_1k_entry *ent;
        int i, idx = (start_addr >> 10) & 0x03;
        if( IS_1K_PAGE_ENTRY(*ptr) ) {
            ent = (struct utlb_1k_entry *)*ptr;
        } else {
            ent = mmu_utlb_1k_alloc();
            /* New 1K struct - init to previous contents of region */
            for( i=0; i<4; i++ ) {
                ent->subpages[i] = *ptr;
                ent->user_subpages[i] = *uptr;
            }
            *ptr = &ent->fn;
            *uptr = &ent->user_fn;
        }
        
        if( priv_page != NULL ) {
            if( ent->subpages[idx] == privdefs->tlb_miss ) {
                ent->subpages[idx] = priv_page;
            } else {
                mapping_ok = FALSE;
                ent->subpages[idx] = privdefs->tlb_multihit;
            }
        }
        if( user_page != NULL ) {
            if( ent->user_subpages[idx] == userdefs->tlb_miss ) {
                ent->user_subpages[idx] = user_page;
            } else {
                mapping_ok = FALSE;
                ent->user_subpages[idx] = userdefs->tlb_multihit;
            }
        }
        
    } else {
        if( priv_page != NULL ) {
            /* Privileged mapping only */
            for( i=0; i<npages; i++ ) {
                if( *ptr == privdefs->tlb_miss ) {
                    *ptr++ = priv_page;
                } else {
                    mapping_ok = FALSE;
                    *ptr++ = privdefs->tlb_multihit;
                }
            }
        }
        if( user_page != NULL ) {
            /* User mapping only (eg ASID change remap w/ SV=1) */
            for( i=0; i<npages; i++ ) {
                if( *uptr == userdefs->tlb_miss ) {
                    *uptr++ = user_page;
                } else {
                    mapping_ok = FALSE;
                    *uptr++ = userdefs->tlb_multihit;
                }
            }        
        }
    }

    return mapping_ok;
}

/**
 * Remap any pages within the region covered by entryNo, but not including 
 * entryNo itself. This is used to reestablish pages that were previously
 * covered by a multi-hit exception region when one of the pages is removed.
 */
static void mmu_utlb_remap_pages( gboolean remap_priv, gboolean remap_user, int entryNo )
{
    int mask = mmu_utlb[entryNo].mask;
    uint32_t remap_addr = mmu_utlb[entryNo].vpn & mask;
    int i;
    
    for( i=0; i<UTLB_ENTRY_COUNT; i++ ) {
        if( i != entryNo && (mmu_utlb[i].vpn & mask) == remap_addr && (mmu_utlb[i].flags & TLB_VALID) ) {
            /* Overlapping region */
            mem_region_fn_t priv_page = (remap_priv ? &mmu_utlb_pages[i].fn : NULL);
            mem_region_fn_t user_page = (remap_priv ? mmu_utlb_pages[i].user_fn : NULL);
            uint32_t start_addr;
            int npages;

            if( mmu_utlb[i].mask >= mask ) {
                /* entry is no larger than the area we're replacing - map completely */
                start_addr = mmu_utlb[i].vpn & mmu_utlb[i].mask;
                npages = get_tlb_size_pages( mmu_utlb[i].flags );
            } else {
                /* Otherwise map subset - region covered by removed page */
                start_addr = remap_addr;
                npages = get_tlb_size_pages( mmu_utlb[entryNo].flags );
            }

            if( (mmu_utlb[i].flags & TLB_SHARE) || mmu_utlb[i].asid == mmu_asid ) { 
                mmu_utlb_map_pages( priv_page, user_page, start_addr, npages );
            } else if( IS_SV_ENABLED() ) {
                mmu_utlb_map_pages( priv_page, NULL, start_addr, npages );
            }

        }
    }
}

/**
 * Remove a previous TLB mapping (replacing them with the TLB miss region).
 * @return FALSE if any pages were previously mapped to the TLB multihit page, 
 * otherwise TRUE. In either case, all pages in the region are cleared to TLB miss.
 */
static gboolean mmu_utlb_unmap_pages( gboolean unmap_priv, gboolean unmap_user, sh4addr_t start_addr, int npages )
{
    mem_region_fn_t *ptr = &sh4_address_space[start_addr >> 12];
    mem_region_fn_t *uptr = &sh4_user_address_space[start_addr >> 12];
    struct utlb_default_regions *privdefs = &mmu_default_regions[DEFAULT_REGIONS];
    struct utlb_default_regions *userdefs = privdefs;

    gboolean unmapping_ok = TRUE;
    int i;
    
    if( (start_addr & 0xFC000000) == 0xE0000000 ) {
        /* Storequeue mapping */
        privdefs = &mmu_default_regions[DEFAULT_STOREQUEUE_REGIONS];
        userdefs = mmu_user_storequeue_regions;
    } else if( (start_addr & 0xE0000000) == 0xC0000000 ) {
        unmap_user = FALSE;
    } else if( start_addr >= 0x80000000 ) {
        return TRUE; // No mapping - legal but meaningless
    }

    if( npages == 0 ) { // 1K page
        assert( IS_1K_PAGE_ENTRY( *ptr ) );
        struct utlb_1k_entry *ent = (struct utlb_1k_entry *)*ptr;
        int i, idx = (start_addr >> 10) & 0x03, mergeable=1;
        if( ent->subpages[idx] == privdefs->tlb_multihit ) {
            unmapping_ok = FALSE;
        }
        if( unmap_priv )
            ent->subpages[idx] = privdefs->tlb_miss;
        if( unmap_user )
            ent->user_subpages[idx] = userdefs->tlb_miss;

        /* If all 4 subpages have the same content, merge them together and
         * release the 1K entry
         */
        mem_region_fn_t priv_page = ent->subpages[0];
        mem_region_fn_t user_page = ent->user_subpages[0];
        for( i=1; i<4; i++ ) {
            if( priv_page != ent->subpages[i] || user_page != ent->user_subpages[i] ) {
                mergeable = 0;
                break;
            }
        }
        if( mergeable ) {
            mmu_utlb_1k_free(ent);
            *ptr = priv_page;
            *uptr = user_page;
        }
    } else {
        if( unmap_priv ) {
            /* Privileged (un)mapping */
            for( i=0; i<npages; i++ ) {
                if( *ptr == privdefs->tlb_multihit ) {
                    unmapping_ok = FALSE;
                }
                *ptr++ = privdefs->tlb_miss;
            }
        }
        if( unmap_user ) {
            /* User (un)mapping */
            for( i=0; i<npages; i++ ) {
                if( *uptr == userdefs->tlb_multihit ) {
                    unmapping_ok = FALSE;
                }
                *uptr++ = userdefs->tlb_miss;
            }            
        }
    }
    
    return unmapping_ok;
}

static void mmu_utlb_insert_entry( int entry )
{
    struct utlb_entry *ent = &mmu_utlb[entry];
    mem_region_fn_t page = &mmu_utlb_pages[entry].fn;
    mem_region_fn_t upage;
    sh4addr_t start_addr = ent->vpn & ent->mask;
    int npages = get_tlb_size_pages(ent->flags);

    if( (start_addr & 0xFC000000) == 0xE0000000 ) {
        /* Store queue mappings are a bit different - normal access is fixed to
         * the store queue register block, and we only map prefetches through
         * the TLB 
         */
        mmu_utlb_init_storequeue_vtable( ent, &mmu_utlb_pages[entry] );

        if( (ent->flags & TLB_USERMODE) == 0 ) {
            upage = mmu_user_storequeue_regions->tlb_prot;
        } else if( IS_STOREQUEUE_PROTECTED() ) {
            upage = &p4_region_storequeue_sqmd;
        } else {
            upage = page;
        }

    }  else {

        if( (ent->flags & TLB_USERMODE) == 0 ) {
            upage = &mem_region_tlb_protected;
        } else {        
            upage = page;
        }

        if( (ent->flags & TLB_WRITABLE) == 0 ) {
            page->write_long = (mem_write_fn_t)tlb_protected_write;
            page->write_word = (mem_write_fn_t)tlb_protected_write;
            page->write_byte = (mem_write_fn_t)tlb_protected_write;
            page->write_burst = (mem_write_burst_fn_t)tlb_protected_write;
            mmu_utlb_init_vtable( ent, &mmu_utlb_pages[entry], FALSE );
        } else if( (ent->flags & TLB_DIRTY) == 0 ) {
            page->write_long = (mem_write_fn_t)tlb_initial_write;
            page->write_word = (mem_write_fn_t)tlb_initial_write;
            page->write_byte = (mem_write_fn_t)tlb_initial_write;
            page->write_burst = (mem_write_burst_fn_t)tlb_initial_write;
            mmu_utlb_init_vtable( ent, &mmu_utlb_pages[entry], FALSE );
        } else {
            mmu_utlb_init_vtable( ent, &mmu_utlb_pages[entry], TRUE );
        }
    }
    
    mmu_utlb_pages[entry].user_fn = upage;

    /* Is page visible? */
    if( (ent->flags & TLB_SHARE) || ent->asid == mmu_asid ) { 
        mmu_utlb_map_pages( page, upage, start_addr, npages );
    } else if( IS_SV_ENABLED() ) {
        mmu_utlb_map_pages( page, NULL, start_addr, npages );
    }
}

static void mmu_utlb_remove_entry( int entry )
{
    int i, j;
    struct utlb_entry *ent = &mmu_utlb[entry];
    sh4addr_t start_addr = ent->vpn&ent->mask;
    mem_region_fn_t *ptr = &sh4_address_space[start_addr >> 12];
    mem_region_fn_t *uptr = &sh4_user_address_space[start_addr >> 12];
    gboolean unmap_user;
    int npages = get_tlb_size_pages(ent->flags);
    
    if( (ent->flags & TLB_SHARE) || ent->asid == mmu_asid ) {
        unmap_user = TRUE;
    } else if( IS_SV_ENABLED() ) {
        unmap_user = FALSE;
    } else {
        return; // Not mapped
    }
    
    gboolean clean_unmap = mmu_utlb_unmap_pages( TRUE, unmap_user, start_addr, npages );
    
    if( !clean_unmap ) {
        mmu_utlb_remap_pages( TRUE, unmap_user, entry );
    }
}

static void mmu_utlb_register_all()
{
    int i;
    for( i=0; i<UTLB_ENTRY_COUNT; i++ ) {
        if( mmu_utlb[i].flags & TLB_VALID ) 
            mmu_utlb_insert_entry( i );
    }
}

static void mmu_invalidate_tlb()
{
    int i;
    for( i=0; i<ITLB_ENTRY_COUNT; i++ ) {
        mmu_itlb[i].flags &= (~TLB_VALID);
    }
    if( IS_TLB_ENABLED() ) {
        for( i=0; i<UTLB_ENTRY_COUNT; i++ ) {
            if( mmu_utlb[i].flags & TLB_VALID ) {
                mmu_utlb_remove_entry( i );
            }
        }
    }
    for( i=0; i<UTLB_ENTRY_COUNT; i++ ) {
        mmu_utlb[i].flags &= (~TLB_VALID);
    }
}

/******************************************************************************/
/*                        MMU TLB address translation                         */
/******************************************************************************/

/**
 * Translate a 32-bit address into a UTLB entry number. Does not check for
 * page protection etc.
 * @return the entryNo if found, -1 if not found, and -2 for a multi-hit.
 */
int mmu_utlb_entry_for_vpn( uint32_t vpn )
{
    mem_region_fn_t fn = sh4_address_space[vpn>>12];
    if( fn >= &mmu_utlb_pages[0].fn && fn < &mmu_utlb_pages[UTLB_ENTRY_COUNT].fn ) {
        return ((struct utlb_page_entry *)fn) - &mmu_utlb_pages[0];
    } else if( fn == &mem_region_tlb_multihit ) {
        return -2;
    } else {
        return -1;
    }
}


/**
 * Perform the actual utlb lookup w/ asid matching.
 * Possible utcomes are:
 *   0..63 Single match - good, return entry found
 *   -1 No match - raise a tlb data miss exception
 *   -2 Multiple matches - raise a multi-hit exception (reset)
 * @param vpn virtual address to resolve
 * @return the resultant UTLB entry, or an error.
 */
static inline int mmu_utlb_lookup_vpn_asid( uint32_t vpn )
{
    int result = -1;
    unsigned int i;

    mmu_urc++;
    if( mmu_urc == mmu_urb || mmu_urc == 0x40 ) {
        mmu_urc = 0;
    }

    for( i = 0; i < UTLB_ENTRY_COUNT; i++ ) {
        if( (mmu_utlb[i].flags & TLB_VALID) &&
                ((mmu_utlb[i].flags & TLB_SHARE) || mmu_asid == mmu_utlb[i].asid) &&
                ((mmu_utlb[i].vpn ^ vpn) & mmu_utlb[i].mask) == 0 ) {
            if( result != -1 ) {
                return -2;
            }
            result = i;
        }
    }
    return result;
}

/**
 * Perform the actual utlb lookup matching on vpn only
 * Possible utcomes are:
 *   0..63 Single match - good, return entry found
 *   -1 No match - raise a tlb data miss exception
 *   -2 Multiple matches - raise a multi-hit exception (reset)
 * @param vpn virtual address to resolve
 * @return the resultant UTLB entry, or an error.
 */
static inline int mmu_utlb_lookup_vpn( uint32_t vpn )
{
    int result = -1;
    unsigned int i;

    mmu_urc++;
    if( mmu_urc == mmu_urb || mmu_urc == 0x40 ) {
        mmu_urc = 0;
    }

    for( i = 0; i < UTLB_ENTRY_COUNT; i++ ) {
        if( (mmu_utlb[i].flags & TLB_VALID) &&
                ((mmu_utlb[i].vpn ^ vpn) & mmu_utlb[i].mask) == 0 ) {
            if( result != -1 ) {
                return -2;
            }
            result = i;
        }
    }

    return result;
}

/**
 * Update the ITLB by replacing the LRU entry with the specified UTLB entry.
 * @return the number (0-3) of the replaced entry.
 */
static int inline mmu_itlb_update_from_utlb( int entryNo )
{
    int replace;
    /* Determine entry to replace based on lrui */
    if( (mmu_lrui & 0x38) == 0x38 ) {
        replace = 0;
        mmu_lrui = mmu_lrui & 0x07;
    } else if( (mmu_lrui & 0x26) == 0x06 ) {
        replace = 1;
        mmu_lrui = (mmu_lrui & 0x19) | 0x20;
    } else if( (mmu_lrui & 0x15) == 0x01 ) {
        replace = 2;
        mmu_lrui = (mmu_lrui & 0x3E) | 0x14;
    } else { // Note - gets invalid entries too
        replace = 3;
        mmu_lrui = (mmu_lrui | 0x0B);
    }

    mmu_itlb[replace].vpn = mmu_utlb[entryNo].vpn;
    mmu_itlb[replace].mask = mmu_utlb[entryNo].mask;
    mmu_itlb[replace].ppn = mmu_utlb[entryNo].ppn;
    mmu_itlb[replace].asid = mmu_utlb[entryNo].asid;
    mmu_itlb[replace].flags = mmu_utlb[entryNo].flags & 0x01DA;
    return replace;
}

/**
 * Perform the actual itlb lookup w/ asid protection
 * Possible utcomes are:
 *   0..63 Single match - good, return entry found
 *   -1 No match - raise a tlb data miss exception
 *   -2 Multiple matches - raise a multi-hit exception (reset)
 * @param vpn virtual address to resolve
 * @return the resultant ITLB entry, or an error.
 */
static inline int mmu_itlb_lookup_vpn_asid( uint32_t vpn )
{
    int result = -1;
    unsigned int i;

    for( i = 0; i < ITLB_ENTRY_COUNT; i++ ) {
        if( (mmu_itlb[i].flags & TLB_VALID) &&
                ((mmu_itlb[i].flags & TLB_SHARE) || mmu_asid == mmu_itlb[i].asid) &&
                ((mmu_itlb[i].vpn ^ vpn) & mmu_itlb[i].mask) == 0 ) {
            if( result != -1 ) {
                return -2;
            }
            result = i;
        }
    }

    if( result == -1 ) {
        int utlbEntry = mmu_utlb_entry_for_vpn( vpn );
        if( utlbEntry < 0 ) {
            return utlbEntry;
        } else {
            return mmu_itlb_update_from_utlb( utlbEntry );
        }
    }

    switch( result ) {
    case 0: mmu_lrui = (mmu_lrui & 0x07); break;
    case 1: mmu_lrui = (mmu_lrui & 0x19) | 0x20; break;
    case 2: mmu_lrui = (mmu_lrui & 0x3E) | 0x14; break;
    case 3: mmu_lrui = (mmu_lrui | 0x0B); break;
    }

    return result;
}

/**
 * Perform the actual itlb lookup on vpn only
 * Possible utcomes are:
 *   0..63 Single match - good, return entry found
 *   -1 No match - raise a tlb data miss exception
 *   -2 Multiple matches - raise a multi-hit exception (reset)
 * @param vpn virtual address to resolve
 * @return the resultant ITLB entry, or an error.
 */
static inline int mmu_itlb_lookup_vpn( uint32_t vpn )
{
    int result = -1;
    unsigned int i;

    for( i = 0; i < ITLB_ENTRY_COUNT; i++ ) {
        if( (mmu_itlb[i].flags & TLB_VALID) &&
                ((mmu_itlb[i].vpn ^ vpn) & mmu_itlb[i].mask) == 0 ) {
            if( result != -1 ) {
                return -2;
            }
            result = i;
        }
    }

    if( result == -1 ) {
        int utlbEntry = mmu_utlb_lookup_vpn( vpn );
        if( utlbEntry < 0 ) {
            return utlbEntry;
        } else {
            return mmu_itlb_update_from_utlb( utlbEntry );
        }
    }

    switch( result ) {
    case 0: mmu_lrui = (mmu_lrui & 0x07); break;
    case 1: mmu_lrui = (mmu_lrui & 0x19) | 0x20; break;
    case 2: mmu_lrui = (mmu_lrui & 0x3E) | 0x14; break;
    case 3: mmu_lrui = (mmu_lrui | 0x0B); break;
    }

    return result;
}

/**
 * Update the icache for an untranslated address
 */
static inline void mmu_update_icache_phys( sh4addr_t addr )
{
    if( (addr & 0x1C000000) == 0x0C000000 ) {
        /* Main ram */
        sh4_icache.page_vma = addr & 0xFF000000;
        sh4_icache.page_ppa = 0x0C000000;
        sh4_icache.mask = 0xFF000000;
        sh4_icache.page = dc_main_ram;
    } else if( (addr & 0x1FE00000) == 0 ) {
        /* BIOS ROM */
        sh4_icache.page_vma = addr & 0xFFE00000;
        sh4_icache.page_ppa = 0;
        sh4_icache.mask = 0xFFE00000;
        sh4_icache.page = dc_boot_rom;
    } else {
        /* not supported */
        sh4_icache.page_vma = -1;
    }
}

/**
 * Update the sh4_icache structure to describe the page(s) containing the
 * given vma. If the address does not reference a RAM/ROM region, the icache
 * will be invalidated instead.
 * If AT is on, this method will raise TLB exceptions normally
 * (hence this method should only be used immediately prior to execution of
 * code), and otherwise will set the icache according to the matching TLB entry.
 * If AT is off, this method will set the entire referenced RAM/ROM region in
 * the icache.
 * @return TRUE if the update completed (successfully or otherwise), FALSE
 * if an exception was raised.
 */
gboolean FASTCALL mmu_update_icache( sh4vma_t addr )
{
    int entryNo;
    if( IS_SH4_PRIVMODE()  ) {
        if( addr & 0x80000000 ) {
            if( addr < 0xC0000000 ) {
                /* P1, P2 and P4 regions are pass-through (no translation) */
                mmu_update_icache_phys(addr);
                return TRUE;
            } else if( addr >= 0xE0000000 && addr < 0xFFFFFF00 ) {
                RAISE_MEM_ERROR(EXC_DATA_ADDR_READ, addr);
                return FALSE;
            }
        }

        uint32_t mmucr = MMIO_READ(MMU,MMUCR);
        if( (mmucr & MMUCR_AT) == 0 ) {
            mmu_update_icache_phys(addr);
            return TRUE;
        }

        if( (mmucr & MMUCR_SV) == 0 )
        	entryNo = mmu_itlb_lookup_vpn_asid( addr );
        else
        	entryNo = mmu_itlb_lookup_vpn( addr );
    } else {
        if( addr & 0x80000000 ) {
            RAISE_MEM_ERROR(EXC_DATA_ADDR_READ, addr);
            return FALSE;
        }

        uint32_t mmucr = MMIO_READ(MMU,MMUCR);
        if( (mmucr & MMUCR_AT) == 0 ) {
            mmu_update_icache_phys(addr);
            return TRUE;
        }

        entryNo = mmu_itlb_lookup_vpn_asid( addr );

        if( entryNo != -1 && (mmu_itlb[entryNo].flags & TLB_USERMODE) == 0 ) {
            RAISE_MEM_ERROR(EXC_TLB_PROT_READ, addr);
            return FALSE;
        }
    }

    switch(entryNo) {
    case -1:
    RAISE_TLB_ERROR(EXC_TLB_MISS_READ, addr);
    return FALSE;
    case -2:
    RAISE_TLB_MULTIHIT_ERROR(addr);
    return FALSE;
    default:
        sh4_icache.page_ppa = mmu_itlb[entryNo].ppn & mmu_itlb[entryNo].mask;
        sh4_icache.page = mem_get_region( sh4_icache.page_ppa );
        if( sh4_icache.page == NULL ) {
            sh4_icache.page_vma = -1;
        } else {
            sh4_icache.page_vma = mmu_itlb[entryNo].vpn & mmu_itlb[entryNo].mask;
            sh4_icache.mask = mmu_itlb[entryNo].mask;
        }
        return TRUE;
    }
}

/**
 * Translate address for disassembly purposes (ie performs an instruction
 * lookup) - does not raise exceptions or modify any state, and ignores
 * protection bits. Returns the translated address, or MMU_VMA_ERROR
 * on translation failure.
 */
sh4addr_t FASTCALL mmu_vma_to_phys_disasm( sh4vma_t vma )
{
    if( vma & 0x80000000 ) {
        if( vma < 0xC0000000 ) {
            /* P1, P2 and P4 regions are pass-through (no translation) */
            return VMA_TO_EXT_ADDR(vma);
        } else if( vma >= 0xE0000000 && vma < 0xFFFFFF00 ) {
            /* Not translatable */
            return MMU_VMA_ERROR;
        }
    }

    uint32_t mmucr = MMIO_READ(MMU,MMUCR);
    if( (mmucr & MMUCR_AT) == 0 ) {
        return VMA_TO_EXT_ADDR(vma);
    }

    int entryNo = mmu_itlb_lookup_vpn( vma );
    if( entryNo == -2 ) {
        entryNo = mmu_itlb_lookup_vpn_asid( vma );
    }
    if( entryNo < 0 ) {
        return MMU_VMA_ERROR;
    } else {
        return (mmu_itlb[entryNo].ppn & mmu_itlb[entryNo].mask) |
        (vma & (~mmu_itlb[entryNo].mask));
    }
}

/********************** TLB Direct-Access Regions ***************************/
#ifdef HAVE_FRAME_ADDRESS
#define EXCEPTION_EXIT() do{ *(((void **)__builtin_frame_address(0))+1) = exc; } while(0)
#else
#define EXCEPTION_EXIT() sh4_core_exit(CORE_EXIT_EXCEPTION)
#endif


#define ITLB_ENTRY(addr) ((addr>>7)&0x03)

int32_t FASTCALL mmu_itlb_addr_read( sh4addr_t addr )
{
    struct itlb_entry *ent = &mmu_itlb[ITLB_ENTRY(addr)];
    return ent->vpn | ent->asid | (ent->flags & TLB_VALID);
}

void FASTCALL mmu_itlb_addr_write( sh4addr_t addr, uint32_t val )
{
    struct itlb_entry *ent = &mmu_itlb[ITLB_ENTRY(addr)];
    ent->vpn = val & 0xFFFFFC00;
    ent->asid = val & 0x000000FF;
    ent->flags = (ent->flags & ~(TLB_VALID)) | (val&TLB_VALID);
}

int32_t FASTCALL mmu_itlb_data_read( sh4addr_t addr )
{
    struct itlb_entry *ent = &mmu_itlb[ITLB_ENTRY(addr)];
    return (ent->ppn & 0x1FFFFC00) | ent->flags;
}

void FASTCALL mmu_itlb_data_write( sh4addr_t addr, uint32_t val )
{
    struct itlb_entry *ent = &mmu_itlb[ITLB_ENTRY(addr)];
    ent->ppn = val & 0x1FFFFC00;
    ent->flags = val & 0x00001DA;
    ent->mask = get_tlb_size_mask(val);
    if( ent->ppn >= 0x1C000000 )
        ent->ppn |= 0xE0000000;
}

#define UTLB_ENTRY(addr) ((addr>>8)&0x3F)
#define UTLB_ASSOC(addr) (addr&0x80)
#define UTLB_DATA2(addr) (addr&0x00800000)

int32_t FASTCALL mmu_utlb_addr_read( sh4addr_t addr )
{
    struct utlb_entry *ent = &mmu_utlb[UTLB_ENTRY(addr)];
    return ent->vpn | ent->asid | (ent->flags & TLB_VALID) |
    ((ent->flags & TLB_DIRTY)<<7);
}
int32_t FASTCALL mmu_utlb_data_read( sh4addr_t addr )
{
    struct utlb_entry *ent = &mmu_utlb[UTLB_ENTRY(addr)];
    if( UTLB_DATA2(addr) ) {
        return ent->pcmcia;
    } else {
        return (ent->ppn&0x1FFFFC00) | ent->flags;
    }
}

/**
 * Find a UTLB entry for the associative TLB write - same as the normal
 * lookup but ignores the valid bit.
 */
static inline int mmu_utlb_lookup_assoc( uint32_t vpn, uint32_t asid )
{
    int result = -1;
    unsigned int i;
    for( i = 0; i < UTLB_ENTRY_COUNT; i++ ) {
        if( (mmu_utlb[i].flags & TLB_VALID) &&
                ((mmu_utlb[i].flags & TLB_SHARE) || asid == mmu_utlb[i].asid) &&
                ((mmu_utlb[i].vpn ^ vpn) & mmu_utlb[i].mask) == 0 ) {
            if( result != -1 ) {
                fprintf( stderr, "TLB Multi hit: %d %d\n", result, i );
                return -2;
            }
            result = i;
        }
    }
    return result;
}

/**
 * Find a ITLB entry for the associative TLB write - same as the normal
 * lookup but ignores the valid bit.
 */
static inline int mmu_itlb_lookup_assoc( uint32_t vpn, uint32_t asid )
{
    int result = -1;
    unsigned int i;
    for( i = 0; i < ITLB_ENTRY_COUNT; i++ ) {
        if( (mmu_itlb[i].flags & TLB_VALID) &&
                ((mmu_itlb[i].flags & TLB_SHARE) || asid == mmu_itlb[i].asid) &&
                ((mmu_itlb[i].vpn ^ vpn) & mmu_itlb[i].mask) == 0 ) {
            if( result != -1 ) {
                return -2;
            }
            result = i;
        }
    }
    return result;
}

void FASTCALL mmu_utlb_addr_write( sh4addr_t addr, uint32_t val, void *exc )
{
    if( UTLB_ASSOC(addr) ) {
        int utlb = mmu_utlb_lookup_assoc( val, mmu_asid );
        if( utlb >= 0 ) {
            struct utlb_entry *ent = &mmu_utlb[utlb];
            uint32_t old_flags = ent->flags;
            ent->flags = ent->flags & ~(TLB_DIRTY|TLB_VALID);
            ent->flags |= (val & TLB_VALID);
            ent->flags |= ((val & 0x200)>>7);
            if( ((old_flags^ent->flags) & (TLB_VALID|TLB_DIRTY)) != 0 ) {
                if( old_flags & TLB_VALID )
                    mmu_utlb_remove_entry( utlb );
                if( ent->flags & TLB_VALID )
                    mmu_utlb_insert_entry( utlb );
            }
        }

        int itlb = mmu_itlb_lookup_assoc( val, mmu_asid );
        if( itlb >= 0 ) {
            struct itlb_entry *ent = &mmu_itlb[itlb];
            ent->flags = (ent->flags & (~TLB_VALID)) | (val & TLB_VALID);
        }

        if( itlb == -2 || utlb == -2 ) {
            RAISE_TLB_MULTIHIT_ERROR(addr);
            EXCEPTION_EXIT();
            return;
        }
    } else {
        struct utlb_entry *ent = &mmu_utlb[UTLB_ENTRY(addr)];
        if( ent->flags & TLB_VALID ) 
            mmu_utlb_remove_entry( UTLB_ENTRY(addr) );
        ent->vpn = (val & 0xFFFFFC00);
        ent->asid = (val & 0xFF);
        ent->flags = (ent->flags & ~(TLB_DIRTY|TLB_VALID));
        ent->flags |= (val & TLB_VALID);
        ent->flags |= ((val & 0x200)>>7);
        if( ent->flags & TLB_VALID ) 
            mmu_utlb_insert_entry( UTLB_ENTRY(addr) );
    }
}

void FASTCALL mmu_utlb_data_write( sh4addr_t addr, uint32_t val )
{
    struct utlb_entry *ent = &mmu_utlb[UTLB_ENTRY(addr)];
    if( UTLB_DATA2(addr) ) {
        ent->pcmcia = val & 0x0000000F;
    } else {
        if( ent->flags & TLB_VALID ) 
            mmu_utlb_remove_entry( UTLB_ENTRY(addr) );
        ent->ppn = (val & 0x1FFFFC00);
        ent->flags = (val & 0x000001FF);
        ent->mask = get_tlb_size_mask(val);
        if( ent->flags & TLB_VALID ) 
            mmu_utlb_insert_entry( UTLB_ENTRY(addr) );
    }
}

struct mem_region_fn p4_region_itlb_addr = {
        mmu_itlb_addr_read, mmu_itlb_addr_write,
        mmu_itlb_addr_read, mmu_itlb_addr_write,
        mmu_itlb_addr_read, mmu_itlb_addr_write,
        unmapped_read_burst, unmapped_write_burst,
        unmapped_prefetch };
struct mem_region_fn p4_region_itlb_data = {
        mmu_itlb_data_read, mmu_itlb_data_write,
        mmu_itlb_data_read, mmu_itlb_data_write,
        mmu_itlb_data_read, mmu_itlb_data_write,
        unmapped_read_burst, unmapped_write_burst,
        unmapped_prefetch };
struct mem_region_fn p4_region_utlb_addr = {
        mmu_utlb_addr_read, (mem_write_fn_t)mmu_utlb_addr_write,
        mmu_utlb_addr_read, (mem_write_fn_t)mmu_utlb_addr_write,
        mmu_utlb_addr_read, (mem_write_fn_t)mmu_utlb_addr_write,
        unmapped_read_burst, unmapped_write_burst,
        unmapped_prefetch };
struct mem_region_fn p4_region_utlb_data = {
        mmu_utlb_data_read, mmu_utlb_data_write,
        mmu_utlb_data_read, mmu_utlb_data_write,
        mmu_utlb_data_read, mmu_utlb_data_write,
        unmapped_read_burst, unmapped_write_burst,
        unmapped_prefetch };

/********************** Error regions **************************/

static void FASTCALL address_error_read( sh4addr_t addr, void *exc ) 
{
    RAISE_MEM_ERROR(EXC_DATA_ADDR_READ, addr);
    EXCEPTION_EXIT();
}

static void FASTCALL address_error_read_burst( unsigned char *dest, sh4addr_t addr, void *exc ) 
{
    RAISE_MEM_ERROR(EXC_DATA_ADDR_READ, addr);
    EXCEPTION_EXIT();
}

static void FASTCALL address_error_write( sh4addr_t addr, uint32_t val, void *exc )
{
    RAISE_MEM_ERROR(EXC_DATA_ADDR_WRITE, addr);
    EXCEPTION_EXIT();
}

static void FASTCALL tlb_miss_read( sh4addr_t addr, void *exc )
{
    RAISE_TLB_ERROR(EXC_TLB_MISS_READ, addr);
    EXCEPTION_EXIT();
}

static void FASTCALL tlb_miss_read_burst( unsigned char *dest, sh4addr_t addr, void *exc )
{
    RAISE_TLB_ERROR(EXC_TLB_MISS_READ, addr);
    EXCEPTION_EXIT();
}

static void FASTCALL tlb_miss_write( sh4addr_t addr, uint32_t val, void *exc )
{
    RAISE_TLB_ERROR(EXC_TLB_MISS_WRITE, addr);
    EXCEPTION_EXIT();
}    

static int32_t FASTCALL tlb_protected_read( sh4addr_t addr, void *exc )
{
    RAISE_MEM_ERROR(EXC_TLB_PROT_READ, addr);
    EXCEPTION_EXIT();
    return 0; 
}

static int32_t FASTCALL tlb_protected_read_burst( unsigned char *dest, sh4addr_t addr, void *exc )
{
    RAISE_MEM_ERROR(EXC_TLB_PROT_READ, addr);
    EXCEPTION_EXIT();
    return 0;
}

static void FASTCALL tlb_protected_write( sh4addr_t addr, uint32_t val, void *exc )
{
    RAISE_MEM_ERROR(EXC_TLB_PROT_WRITE, addr);
    EXCEPTION_EXIT();
}

static void FASTCALL tlb_initial_write( sh4addr_t addr, uint32_t val, void *exc )
{
    RAISE_MEM_ERROR(EXC_INIT_PAGE_WRITE, addr);
    EXCEPTION_EXIT();
}
    
static int32_t FASTCALL tlb_multi_hit_read( sh4addr_t addr, void *exc )
{
    sh4_raise_tlb_multihit(addr);
    EXCEPTION_EXIT();
    return 0; 
}

static int32_t FASTCALL tlb_multi_hit_read_burst( unsigned char *dest, sh4addr_t addr, void *exc )
{
    sh4_raise_tlb_multihit(addr);
    EXCEPTION_EXIT();
    return 0; 
}
static void FASTCALL tlb_multi_hit_write( sh4addr_t addr, uint32_t val, void *exc )
{
    sh4_raise_tlb_multihit(addr);
    EXCEPTION_EXIT();
}

/**
 * Note: Per sec 4.6.4 of the SH7750 manual, SQ 
 */
struct mem_region_fn mem_region_address_error = {
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_burst_fn_t)address_error_read_burst, (mem_write_burst_fn_t)address_error_write,
        unmapped_prefetch };

struct mem_region_fn mem_region_tlb_miss = {
        (mem_read_fn_t)tlb_miss_read, (mem_write_fn_t)tlb_miss_write,
        (mem_read_fn_t)tlb_miss_read, (mem_write_fn_t)tlb_miss_write,
        (mem_read_fn_t)tlb_miss_read, (mem_write_fn_t)tlb_miss_write,
        (mem_read_burst_fn_t)tlb_miss_read_burst, (mem_write_burst_fn_t)tlb_miss_write,
        unmapped_prefetch };

struct mem_region_fn mem_region_tlb_protected = {
        (mem_read_fn_t)tlb_protected_read, (mem_write_fn_t)tlb_protected_write,
        (mem_read_fn_t)tlb_protected_read, (mem_write_fn_t)tlb_protected_write,
        (mem_read_fn_t)tlb_protected_read, (mem_write_fn_t)tlb_protected_write,
        (mem_read_burst_fn_t)tlb_protected_read_burst, (mem_write_burst_fn_t)tlb_protected_write,
        unmapped_prefetch };

struct mem_region_fn mem_region_tlb_multihit = {
        (mem_read_fn_t)tlb_multi_hit_read, (mem_write_fn_t)tlb_multi_hit_write,
        (mem_read_fn_t)tlb_multi_hit_read, (mem_write_fn_t)tlb_multi_hit_write,
        (mem_read_fn_t)tlb_multi_hit_read, (mem_write_fn_t)tlb_multi_hit_write,
        (mem_read_burst_fn_t)tlb_multi_hit_read_burst, (mem_write_burst_fn_t)tlb_multi_hit_write,
        (mem_prefetch_fn_t)tlb_multi_hit_read };
        

/* Store-queue regions */
/* These are a bit of a pain - the first 8 fields are controlled by SQMD, while 
 * the final (prefetch) is controlled by the actual TLB settings (plus SQMD in
 * some cases), in contrast to the ordinary fields above.
 * 
 * There is probably a simpler way to do this.
 */

struct mem_region_fn p4_region_storequeue = { 
        ccn_storequeue_read_long, ccn_storequeue_write_long,
        unmapped_read_long, unmapped_write_long, /* TESTME: Officially only long access is supported */
        unmapped_read_long, unmapped_write_long,
        unmapped_read_burst, unmapped_write_burst,
        ccn_storequeue_prefetch }; 

struct mem_region_fn p4_region_storequeue_miss = { 
        ccn_storequeue_read_long, ccn_storequeue_write_long,
        unmapped_read_long, unmapped_write_long, /* TESTME: Officially only long access is supported */
        unmapped_read_long, unmapped_write_long,
        unmapped_read_burst, unmapped_write_burst,
        (mem_prefetch_fn_t)tlb_miss_read }; 

struct mem_region_fn p4_region_storequeue_multihit = { 
        ccn_storequeue_read_long, ccn_storequeue_write_long,
        unmapped_read_long, unmapped_write_long, /* TESTME: Officially only long access is supported */
        unmapped_read_long, unmapped_write_long,
        unmapped_read_burst, unmapped_write_burst,
        (mem_prefetch_fn_t)tlb_multi_hit_read }; 

struct mem_region_fn p4_region_storequeue_protected = {
        ccn_storequeue_read_long, ccn_storequeue_write_long,
        unmapped_read_long, unmapped_write_long,
        unmapped_read_long, unmapped_write_long,
        unmapped_read_burst, unmapped_write_burst,
        (mem_prefetch_fn_t)tlb_protected_read };

struct mem_region_fn p4_region_storequeue_sqmd = {
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_burst_fn_t)address_error_read_burst, (mem_write_burst_fn_t)address_error_write,
        (mem_prefetch_fn_t)address_error_read };        
        
struct mem_region_fn p4_region_storequeue_sqmd_miss = { 
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_burst_fn_t)address_error_read_burst, (mem_write_burst_fn_t)address_error_write,
        (mem_prefetch_fn_t)tlb_miss_read }; 

struct mem_region_fn p4_region_storequeue_sqmd_multihit = {
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_burst_fn_t)address_error_read_burst, (mem_write_burst_fn_t)address_error_write,
        (mem_prefetch_fn_t)tlb_multi_hit_read };        
        
struct mem_region_fn p4_region_storequeue_sqmd_protected = {
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_fn_t)address_error_read, (mem_write_fn_t)address_error_write,
        (mem_read_burst_fn_t)address_error_read_burst, (mem_write_burst_fn_t)address_error_write,
        (mem_prefetch_fn_t)tlb_protected_read };

