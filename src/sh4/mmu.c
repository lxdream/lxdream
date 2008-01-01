/**
 * $Id$
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
#define TLB_USERWRITABLE (TLB_WRITABLE|TLB_USERMODE)
#define TLB_SIZE_MASK 0x00000090
#define TLB_SIZE_1K   0x00000000
#define TLB_SIZE_4K   0x00000010
#define TLB_SIZE_64K  0x00000080
#define TLB_SIZE_1M   0x00000090
#define TLB_CACHEABLE 0x00000008
#define TLB_DIRTY     0x00000004
#define TLB_SHARE     0x00000002
#define TLB_WRITETHRU 0x00000001

#define MASK_1K  0xFFFFFC00
#define MASK_4K  0xFFFFF000
#define MASK_64K 0xFFFF0000
#define MASK_1M  0xFFF00000

struct itlb_entry {
    sh4addr_t vpn; // Virtual Page Number
    uint32_t asid; // Process ID
    uint32_t mask;
    sh4addr_t ppn; // Physical Page Number
    uint32_t flags;
};

struct utlb_entry {
    sh4addr_t vpn; // Virtual Page Number
    uint32_t mask; // Page size mask
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


static uint32_t get_mask_for_flags( uint32_t flags )
{
    switch( flags & TLB_SIZE_MASK ) {
    case TLB_SIZE_1K: return MASK_1K;
    case TLB_SIZE_4K: return MASK_4K;
    case TLB_SIZE_64K: return MASK_64K;
    case TLB_SIZE_1M: return MASK_1M;
    }
}

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
    fwrite( &mmu_urc, sizeof(mmu_urc), 1, f );
    fwrite( &mmu_urb, sizeof(mmu_urb), 1, f );
    fwrite( &mmu_lrui, sizeof(mmu_lrui), 1, f );
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
    if( fread( &mmu_urc, sizeof(mmu_urc), 1, f ) != 1 ) {
	return 1;
    }
    if( fread( &mmu_urc, sizeof(mmu_urb), 1, f ) != 1 ) {
	return 1;
    }
    if( fread( &mmu_lrui, sizeof(mmu_lrui), 1, f ) != 1 ) {
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
    mmu_utlb[mmu_urc].mask = get_mask_for_flags(mmu_utlb[mmu_urc].flags);
}

static inline void mmu_flush_pages( struct utlb_entry *ent )
{
    unsigned int vpn;
    switch( ent->flags & TLB_SIZE_MASK ) {
    case TLB_SIZE_1K: xlat_flush_page( ent->vpn ); break;
    case TLB_SIZE_4K: xlat_flush_page( ent->vpn ); break;
    case TLB_SIZE_64K: 
	for( vpn = ent->vpn; vpn < ent->vpn + 0x10000; vpn += 0x1000 ) {
	    xlat_flush_page( vpn );
	}
	break;
    case TLB_SIZE_1M:
	for( vpn = ent->vpn; vpn < ent->vpn + 0x100000; vpn += 0x1000 ) {
	    xlat_flush_page( vpn );
	}
	break;
    }
}

/**
 * The translations are excessively complicated, but unfortunately it's a 
 * complicated system. It can undoubtedly be better optimized too.
 */

/**
 * Perform the actual utlb lookup.
 * Possible utcomes are:
 *   0..63 Single match - good, return entry found
 *   -1 No match - raise a tlb data miss exception
 *   -2 Multiple matches - raise a multi-hit exception (reset)
 * @param vpn virtual address to resolve
 * @param asid Address space identifier
 * @param use_asid whether to require an asid match on non-shared pages.
 * @return the resultant UTLB entry, or an error.
 */
static inline int mmu_utlb_lookup_vpn( uint32_t vpn, uint32_t asid, int use_asid )
{
    int result = -1;
    unsigned int i;

    mmu_urc++;
    if( mmu_urc == mmu_urb || mmu_urc == 0x40 ) {
	mmu_urc = 0;
    }

    if( use_asid ) {
	for( i = 0; i < UTLB_ENTRY_COUNT; i++ ) {
	    if( (mmu_utlb[i].flags & TLB_VALID) &&
	        ((mmu_utlb[i].flags & TLB_SHARE) || asid == mmu_utlb[i].asid) && 
		((mmu_utlb[i].vpn ^ vpn) & mmu_utlb[i].mask) == 0 ) {
		if( result != -1 ) {
		    return -2;
		}
		result = i;
	    }
	}
    } else {
	for( i = 0; i < UTLB_ENTRY_COUNT; i++ ) {
	    if( (mmu_utlb[i].flags & TLB_VALID) &&
		((mmu_utlb[i].vpn ^ vpn) & mmu_utlb[i].mask) == 0 ) {
		if( result != -1 ) {
		    return -2;
		}
		result = i;
	    }
	}
    }
    return result;
}

/**
 * Find a UTLB entry for the associative TLB write - same as the normal
 * lookup but ignores the valid bit.
 */
static inline mmu_utlb_lookup_assoc( uint32_t vpn, uint32_t asid )
{
    int result = -1;
    unsigned int i;
    for( i = 0; i < UTLB_ENTRY_COUNT; i++ ) {
	if( ((mmu_utlb[i].flags & TLB_SHARE) || asid == mmu_utlb[i].asid) && 
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
 * Perform the actual itlb lookup.
 * Possible utcomes are:
 *   0..63 Single match - good, return entry found
 *   -1 No match - raise a tlb data miss exception
 *   -2 Multiple matches - raise a multi-hit exception (reset)
 * @param vpn virtual address to resolve
 * @param asid Address space identifier
 * @param use_asid whether to require an asid match on non-shared pages.
 * @return the resultant ITLB entry, or an error.
 */
static inline int mmu_itlb_lookup_vpn( uint32_t vpn, uint32_t asid, int use_asid )
{
    int result = -1;
    unsigned int i;
    if( use_asid ) {
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
    } else {
	for( i = 0; i < ITLB_ENTRY_COUNT; i++ ) {
	    if( (mmu_itlb[i].flags & TLB_VALID) &&
		((mmu_itlb[i].vpn ^ vpn) & mmu_itlb[i].mask) == 0 ) {
		if( result != -1 ) {
		    return -2;
		}
		result = i;
	    }
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

static int inline mmu_itlb_update_from_utlb( int entryNo )
{
    int replace;
    /* Determine entry to replace based on lrui */
    if( mmu_lrui & 0x38 == 0x38 ) {
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
 * Find a ITLB entry for the associative TLB write - same as the normal
 * lookup but ignores the valid bit.
 */
static inline mmu_itlb_lookup_assoc( uint32_t vpn, uint32_t asid )
{
    int result = -1;
    unsigned int i;
    for( i = 0; i < ITLB_ENTRY_COUNT; i++ ) {
	if( ((mmu_itlb[i].flags & TLB_SHARE) || asid == mmu_itlb[i].asid) && 
	    ((mmu_itlb[i].vpn ^ vpn) & mmu_itlb[i].mask) == 0 ) {
	    if( result != -1 ) {
		return -2;
	    }
	    result = i;
	}
    }
    return result;
}

#define RAISE_TLB_ERROR(code, vpn) \
    MMIO_WRITE(MMU, TEA, vpn); \
    MMIO_WRITE(MMU, PTEH, ((MMIO_READ(MMU, PTEH) & 0x000003FF) | (vpn&0xFFFFFC00))); \
    sh4_raise_tlb_exception(code); \
    return (((uint64_t)code)<<32)

#define RAISE_MEM_ERROR(code, vpn) \
    MMIO_WRITE(MMU, TEA, vpn); \
    MMIO_WRITE(MMU, PTEH, ((MMIO_READ(MMU, PTEH) & 0x000003FF) | (vpn&0xFFFFFC00))); \
    sh4_raise_exception(code); \
    return (((uint64_t)code)<<32)

#define RAISE_OTHER_ERROR(code) \
    sh4_raise_exception(code); \
    return (((uint64_t)EXV_EXCEPTION)<<32)

/**
 * Abort with a non-MMU address error. Caused by user-mode code attempting
 * to access privileged regions, or alignment faults.
 */
#define MMU_READ_ADDR_ERROR() RAISE_OTHER_ERROR(EXC_DATA_ADDR_READ)
#define MMU_WRITE_ADDR_ERROR() RAISE_OTHER_ERROR(EXC_DATA_ADDR_WRITE)

#define MMU_TLB_READ_MISS_ERROR(vpn) RAISE_TLB_ERROR(EXC_TLB_MISS_READ, vpn)
#define MMU_TLB_WRITE_MISS_ERROR(vpn) RAISE_TLB_ERROR(EXC_TLB_MISS_WRITE, vpn)
#define MMU_TLB_INITIAL_WRITE_ERROR(vpn) RAISE_MEM_ERROR(EXC_INIT_PAGE_WRITE, vpn)
#define MMU_TLB_READ_PROT_ERROR(vpn) RAISE_MEM_ERROR(EXC_TLB_PROT_READ, vpn)
#define MMU_TLB_WRITE_PROT_ERROR(vpn) RAISE_MEM_ERROR(EXC_TLB_PROT_WRITE, vpn)
#define MMU_TLB_MULTI_HIT_ERROR(vpn) sh4_raise_reset(EXC_TLB_MULTI_HIT); \
    MMIO_WRITE(MMU, TEA, vpn); \
    MMIO_WRITE(MMU, PTEH, ((MMIO_READ(MMU, PTEH) & 0x000003FF) | (vpn&0xFFFFFC00))); \
    return (((uint64_t)EXC_TLB_MULTI_HIT)<<32)

uint64_t mmu_vma_to_phys_write( sh4addr_t addr )
{
    uint32_t mmucr = MMIO_READ(MMU,MMUCR);
    if( addr & 0x80000000 ) {
	if( IS_SH4_PRIVMODE() ) {
	    if( addr < 0xC0000000 || addr >= 0xE0000000 ) {
		/* P1, P2 and P4 regions are pass-through (no translation) */
		return (uint64_t)addr;
	    }
	} else {
	    if( addr >= 0xE0000000 && addr < 0xE4000000 &&
		((mmucr&MMUCR_SQMD) == 0) ) {
		/* Conditional user-mode access to the store-queue (no translation) */
		return (uint64_t)addr;
	    }
	    MMU_WRITE_ADDR_ERROR();
	}
    }
    
    if( (mmucr & MMUCR_AT) == 0 ) {
	return (uint64_t)addr;
    }

    /* If we get this far, translation is required */

    int use_asid = ((mmucr & MMUCR_SV) == 0) || !IS_SH4_PRIVMODE();
    uint32_t asid = MMIO_READ( MMU, PTEH ) & 0xFF;
    
    int entryNo = mmu_utlb_lookup_vpn( addr, asid, use_asid );

    switch(entryNo) {
    case -1:
	MMU_TLB_WRITE_MISS_ERROR(addr);
	break;
    case -2:
	MMU_TLB_MULTI_HIT_ERROR(addr);
	break;
    default:
	if( IS_SH4_PRIVMODE() ? ((mmu_utlb[entryNo].flags & TLB_WRITABLE) == 0)
	    : ((mmu_utlb[entryNo].flags & TLB_USERWRITABLE) != TLB_USERWRITABLE) ) {
	    /* protection violation */
	    MMU_TLB_WRITE_PROT_ERROR(addr);
	}

	if( (mmu_utlb[entryNo].flags & TLB_DIRTY) == 0 ) {
	    MMU_TLB_INITIAL_WRITE_ERROR(addr);
	}

	/* finally generate the target address */
	return (mmu_utlb[entryNo].ppn & mmu_utlb[entryNo].mask) | 
	    (addr & (~mmu_utlb[entryNo].mask));
    }
    return -1;

}

uint64_t mmu_vma_to_phys_exec( sh4addr_t addr )
{
    uint32_t mmucr = MMIO_READ(MMU,MMUCR);
    if( addr & 0x80000000 ) {
	if( IS_SH4_PRIVMODE()  ) {
	    if( addr < 0xC0000000 ) {
		/* P1, P2 and P4 regions are pass-through (no translation) */
		return (uint64_t)addr;
	    } else if( addr >= 0xE0000000 ) {
		MMU_READ_ADDR_ERROR();
	    }
	} else {
	    MMU_READ_ADDR_ERROR();
	}
    }
    
    if( (mmucr & MMUCR_AT) == 0 ) {
	return (uint64_t)addr;
    }

    /* If we get this far, translation is required */
    int use_asid = ((mmucr & MMUCR_SV) == 0) || !IS_SH4_PRIVMODE();
    uint32_t asid = MMIO_READ( MMU, PTEH ) & 0xFF;
    
    int entryNo = mmu_itlb_lookup_vpn( addr, asid, use_asid );
    if( entryNo == -1 ) {
	entryNo = mmu_utlb_lookup_vpn( addr, asid, use_asid );
	if( entryNo >= 0 ) {
	    entryNo = mmu_itlb_update_from_utlb( entryNo );
	}
    }
    switch(entryNo) {
    case -1:
	MMU_TLB_READ_MISS_ERROR(addr);
	break;
    case -2:
	MMU_TLB_MULTI_HIT_ERROR(addr);
	break;
    default:
	if( (mmu_itlb[entryNo].flags & TLB_USERMODE) == 0 &&
	    !IS_SH4_PRIVMODE() ) {
	    /* protection violation */
	    MMU_TLB_READ_PROT_ERROR(addr);
	}

	/* finally generate the target address */
	return (mmu_itlb[entryNo].ppn & mmu_itlb[entryNo].mask) | 
	    (addr & (~mmu_itlb[entryNo].mask));
    }
    return -1;
}

uint64_t mmu_vma_to_phys_read_noexc( sh4addr_t addr ) {


}


uint64_t mmu_vma_to_phys_read( sh4addr_t addr )
{
    uint32_t mmucr = MMIO_READ(MMU,MMUCR);
    if( addr & 0x80000000 ) {
	if( IS_SH4_PRIVMODE() ) {
	    if( addr < 0xC0000000 || addr >= 0xE0000000 ) {
		/* P1, P2 and P4 regions are pass-through (no translation) */
		return (uint64_t)addr;
	    }
	} else {
	    if( addr >= 0xE0000000 && addr < 0xE4000000 &&
		((mmucr&MMUCR_SQMD) == 0) ) {
		/* Conditional user-mode access to the store-queue (no translation) */
		return (uint64_t)addr;
	    }
	    MMU_READ_ADDR_ERROR();
	}
    }
    
    if( (mmucr & MMUCR_AT) == 0 ) {
	return (uint64_t)addr;
    }

    /* If we get this far, translation is required */

    int use_asid = ((mmucr & MMUCR_SV) == 0) || !IS_SH4_PRIVMODE();
    uint32_t asid = MMIO_READ( MMU, PTEH ) & 0xFF;
    
    int entryNo = mmu_utlb_lookup_vpn( addr, asid, use_asid );

    switch(entryNo) {
    case -1:
	MMU_TLB_READ_MISS_ERROR(addr);
	break;
    case -2:
	MMU_TLB_MULTI_HIT_ERROR(addr);
	break;
    default:
	if( (mmu_utlb[entryNo].flags & TLB_USERMODE) == 0 &&
	    !IS_SH4_PRIVMODE() ) {
	    /* protection violation */
	    MMU_TLB_READ_PROT_ERROR(addr);
	}

	/* finally generate the target address */
	return (mmu_utlb[entryNo].ppn & mmu_utlb[entryNo].mask) | 
	    (addr & (~mmu_utlb[entryNo].mask));
    }
    return -1;
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
    ent->mask = get_mask_for_flags(val);
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
	uint32_t asid = MMIO_READ( MMU, PTEH ) & 0xFF;
	int entryNo = mmu_utlb_lookup_assoc( val, asid );
	if( entryNo >= 0 ) {
	    struct utlb_entry *ent = &mmu_utlb[entryNo];
	    ent->flags = ent->flags & ~(TLB_DIRTY|TLB_VALID);
	    ent->flags |= (val & TLB_VALID);
	    ent->flags |= ((val & 0x200)>>7);
	} else if( entryNo == -2 ) {
	    MMU_TLB_MULTI_HIT_ERROR(addr);
	}
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
	ent->mask = get_mask_for_flags(val);
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
