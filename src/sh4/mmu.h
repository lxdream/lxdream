/**
 * $Id$
 *
 * MMU/TLB definitions.
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


#ifndef lxdream_sh4_mmu_H
#define lxdream_sh4_mmu_H 1

#include "lxdream.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VMA_TO_EXT_ADDR(vma) ((vma)&0x1FFFFFFF)

/************************** UTLB/ITLB Definitions ***************************/
/* mmucr register bits */
#define MMUCR_AT   0x00000001 /* Address Translation enabled */
#define MMUCR_TI   0x00000004 /* TLB invalidate (always read as 0) */
#define MMUCR_SV   0x00000100 /* Single Virtual mode=1 / multiple virtual=0 */
#define MMUCR_SQMD 0x00000200 /* Store queue mode bit (0=user, 1=priv only) */
#define MMUCR_URC  0x0000FC00 /* UTLB access counter */
#define MMUCR_URB  0x00FC0000 /* UTLB entry boundary */
#define MMUCR_LRUI 0xFC000000 /* Least recently used ITLB */
#define MMUCR_MASK 0xFCFCFF05
#define MMUCR_RMASK 0xFCFCFF01 /* Read mask */
    
#define IS_TLB_ENABLED() (MMIO_READ(MMU, MMUCR)&MMUCR_AT)
#define IS_SV_ENABLED() (MMIO_READ(MMU,MMUCR)&MMUCR_SV)

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
    uint32_t pcmcia; // extra pcmcia data - not used in this implementation
};

#define TLB_FUNC_SIZE 48

struct utlb_page_entry {
    struct mem_region_fn fn;
    struct mem_region_fn *user_fn;
    mem_region_fn_t target;
    unsigned char code[TLB_FUNC_SIZE*9];
};

struct utlb_1k_entry {
    struct mem_region_fn fn;
    struct mem_region_fn user_fn;
    struct mem_region_fn *subpages[4];
    struct mem_region_fn *user_subpages[4];
    unsigned char code[TLB_FUNC_SIZE*18];
};

struct utlb_default_regions {
    mem_region_fn_t tlb_miss;
    mem_region_fn_t tlb_prot;
    mem_region_fn_t tlb_multihit;
};

/** Set the MMU's target external address space
 * @return the previous address space.
 */
mem_region_fn_t *mmu_set_ext_address_space( mem_region_fn_t *space );

/* Address translation functions */
sh4addr_t FASTCALL mmu_vma_to_phys_disasm( sh4vma_t vma );
mem_region_fn_t FASTCALL mmu_get_region_for_vma_read( sh4vma_t *addr );
mem_region_fn_t FASTCALL mmu_get_region_for_vma_write( sh4vma_t *addr );
mem_region_fn_t FASTCALL mmu_get_region_for_vma_prefetch( sh4vma_t *addr );

/* Translator provided helpers */
void mmu_utlb_init_vtable( struct utlb_entry *ent, struct utlb_page_entry *page, gboolean writable ); 
void mmu_utlb_1k_init_vtable( struct utlb_1k_entry *ent ); 
void mmu_utlb_init_storequeue_vtable( struct utlb_entry *ent, struct utlb_page_entry *page );

extern uint32_t mmu_urc;
extern uint32_t mmu_urb;

/** Primary SH4 address space (privileged and user access)
 * Page map (4KB) of the entire 32-bit address space
 * Note: only callable from the SH4 cores as it depends on the caller setting
 * up an appropriate exception environment. 
 **/
extern struct mem_region_fn **sh4_address_space;
extern struct mem_region_fn **sh4_user_address_space;

/************ Storequeue/cache functions ***********/
void FASTCALL ccn_storequeue_write_long( sh4addr_t addr, uint32_t val );
int32_t FASTCALL ccn_storequeue_read_long( sh4addr_t addr );

/** Default storequeue prefetch when TLB is disabled */
void FASTCALL ccn_storequeue_prefetch( sh4addr_t addr ); 

/** TLB-enabled variant of the storequeue prefetch */
void FASTCALL ccn_storequeue_prefetch_tlb( sh4addr_t addr );

/** Non-storequeue prefetch */
void FASTCALL ccn_prefetch( sh4addr_t addr );

/** Non-cached prefetch (ie, no-op) */
void FASTCALL ccn_uncached_prefetch( sh4addr_t addr );


extern struct mem_region_fn mem_region_address_error;
extern struct mem_region_fn mem_region_tlb_miss;
extern struct mem_region_fn mem_region_tlb_multihit;
extern struct mem_region_fn mem_region_tlb_protected;

extern struct mem_region_fn p4_region_storequeue; 
extern struct mem_region_fn p4_region_storequeue_multihit; 
extern struct mem_region_fn p4_region_storequeue_miss; 
extern struct mem_region_fn p4_region_storequeue_protected; 
extern struct mem_region_fn p4_region_storequeue_sqmd; 
extern struct mem_region_fn p4_region_storequeue_sqmd_miss; 
extern struct mem_region_fn p4_region_storequeue_sqmd_multihit; 
extern struct mem_region_fn p4_region_storequeue_sqmd_protected; 

#ifdef __cplusplus
}
#endif
#endif /* !lxdream_sh4_mmu_H */
