/**
 * $Id$
 * 
 * x86-specific MMU code - this emits simple TLB stubs for TLB indirection.
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

#include "lxdream.h"
#include "mem.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"
#include "sh4/sh4trans.h"
#include "sh4/mmu.h"
#include "xlat/x86/x86op.h"

#if SIZEOF_VOID_P == 8
#define XLAT(addr_space, reg) \
    MOVQ_imm64_r64( (uintptr_t)addr_space, REG_RAX ); \
    MOVP_sib_rptr( 3, reg, REG_RAX, 0, reg );
#define ADDP_imms_ptr(imm,p) \
    MOVQ_imm64_r64((uintptr_t)p, REG_EAX ); \
    ADDL_imms_r32disp(imm, REG_EAX, 0);
#else
#define XLAT(addr_space, reg) \
    MOVP_sib_rptr( 2, reg, -1, (uintptr_t)addr_space, reg ); 
#define ADDP_imms_ptr(imm,p) \
    ADDL_imms_r32disp(imm, -1, (uintptr_t)p);
#endif

void mmu_utlb_init_vtable( struct utlb_entry *ent, struct utlb_page_entry *page, gboolean writable )
{
    uint32_t mask = ent->mask;
    uint32_t vpn = ent->vpn & mask;
    uint32_t ppn = ent->ppn & mask;
    struct mem_region_fn **addr_space;
    uint8_t **out = (uint8_t **)&page->fn;
    uint8_t **fn;
    int inc = writable ? 1 : 2; 
    int i;
    
    xlat_output = page->code;
    if( (ppn & 0x1FFFFFFF) >= 0x1C000000 ) {
        /* SH4 control region */
        ppn |= 0xE0000000;
        addr_space = sh4_address_space;
    } else {
        addr_space = ext_address_space;
    }
    fn = (uint8_t **)addr_space[ppn>>12];
    
    for( i=0; i<10; i+= inc, fn += inc, out += inc ) {
        *out = xlat_output;
        if( i != 9 ) { /* read_byte_for_write doesn't increment mmu_urc, everything else does */
            ADDP_imms_ptr(1, &mmu_urc);
        }
        ADDL_imms_r32( ppn-vpn, REG_ARG1 ); // 6
        if( ent->mask >= 0xFFFFF000 ) {
            // Maps to a single page, so jump directly there
            int rel = (*fn - xlat_output);
            JMP_prerel( rel ); // 5
        } else {
            MOVL_r32_r32( REG_ARG1, REG_ECX ); // 2
            SHRL_imm_r32( 12, REG_ECX );  // 3
            XLAT(addr_space, REG_ECX);                   // 14
            JMP_r32disp(REG_ECX, (((uintptr_t)out) - ((uintptr_t)&page->fn)) );    // 3
        }
    }
    
    page->fn.prefetch = unmapped_prefetch; // FIXME
}

void mmu_utlb_init_storequeue_vtable( struct utlb_entry *ent, struct utlb_page_entry *page )
{
    uint32_t mask = ent->mask;
    uint32_t vpn = ent->vpn & mask;
    uint32_t ppn = ent->ppn & mask;

    xlat_output = page->code;

    memcpy( page, &p4_region_storequeue, sizeof(struct mem_region_fn) );

    page->fn.prefetch = (mem_prefetch_fn_t)xlat_output;
    ADDP_imms_ptr(1, &mmu_urc);
    ADDL_imms_r32( ppn-vpn, REG_ARG1 );
    int rel = ((uint8_t *)ccn_storequeue_prefetch_tlb) - xlat_output;
    JMP_prerel( rel );
}

void mmu_utlb_1k_init_vtable( struct utlb_1k_entry *entry )
{
    xlat_output = entry->code;
    int i;
    uint8_t **out = (uint8_t **)&entry->fn;
    
    for( i=0; i<9; i++, out++ ) {
        *out = xlat_output;
        MOVL_r32_r32( REG_ARG1, REG_ECX );
        SHRL_imm_r32( 10, REG_ECX );
        ANDL_imms_r32( 0x3, REG_ECX );
        XLAT( (uintptr_t)&entry->subpages[0], REG_ECX );
        JMP_r32disp(REG_ECX, (((uintptr_t)out) - ((uintptr_t)&entry->fn)) );    // 3
    }

    out = (uint8_t **)&entry->user_fn;
    for( i=0; i<9; i++, out++ ) {
        *out = xlat_output;
        MOVL_r32_r32( REG_ARG1, REG_ECX );
        SHRL_imm_r32( 10, REG_ECX );
        ANDL_imms_r32( 0x3, REG_ECX );
        XLAT( (uintptr_t)&entry->user_subpages[0], REG_ECX );
        JMP_r32disp(REG_ECX, (((uintptr_t)out) - ((uintptr_t)&entry->user_fn)) );    // 3
    }

}
