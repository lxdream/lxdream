/**
 * $Id$
 * 
 * UTLB unit test support
 *
 * Copyright (c) 2006 Nathan Keynes.
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

#include <../lib.h>

#ifndef TEST_UTLB
#define TEST_UTLB 1

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

void set_tlb_enabled( int flag );
void invalidate_tlb();
void set_sv_enabled( int flag );
void set_storequeue_protected( int flag ); 
void set_asid( int asid );
void load_utlb_entry( int entryNo, uint32_t vpn, uint32_t ppn, int asid, uint32_t mode );

#define ACCESS_OK        0
#define ACCESS_READONLY  1
#define ACCESS_PRIVONLY  2
#define ACCESS_USERMISS  4
void check_utlb_access( uint32_t addr, uint32_t direct_addr, int mode ); 

#endif /* !TEST_UTLB */
