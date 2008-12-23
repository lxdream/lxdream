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

struct utlb_sort_entry {
    sh4addr_t key; // Masked VPN + ASID
    uint32_t mask; // Mask + 0x00FF
    int entryNo;
};
    
#ifdef __cplusplus
}
#endif
#endif /* !lxdream_sh4_mmu_H */
