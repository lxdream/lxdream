/**
 * $Id: utlb.c 831 2008-08-13 10:32:00Z nkeynes $
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

#include <assert.h>
#include "utlb.h"
#include "../lib.h"

#define TLB_VALID     0x00000100
#define TLB_USERMODE  0x00000040
#define TLB_WRITABLE  0x00000020
#define TLB_SIZE_1K   0x00000000
#define TLB_SIZE_4K   0x00000010
#define TLB_SIZE_64K  0x00000080
#define TLB_SIZE_1M   0x00000090
#define TLB_CACHEABLE 0x00000008
#define TLB_DIRTY     0x00000004
#define TLB_SHARE     0x00000002
#define TLB_WRITETHRU 0x00000001

#define PTEH  0xFF000000
#define PTEL  0xFF000004
#define TEA   0xFF00000C
#define MMUCR 0xFF000010

void set_tlb_enabled( int flag )
{
    uint32_t val = long_read( MMUCR );
    if( flag ) {
        val |= 1;
    } else {
        val &= ~1;
    }
    long_write( MMUCR, val );
}

void invalidate_tlb()
{
    uint32_t val = long_read( MMUCR );
    long_write( MMUCR, val | 4 );
}

void set_sv_enabled( int flag )
{
    uint32_t val = long_read( MMUCR );
    if( flag ) {
        val |= 0x100;
    } else {
        val &= ~0x100;
    }
    long_write( MMUCR, val );
}

void set_storequeue_protected( int flag ) 
{
    uint32_t val = long_read( MMUCR );
    if( flag ) {
        val |= 0x200;
    } else {
        val &= ~0x200;
    }
    long_write( MMUCR, val );
}
    

void set_asid( int asid )
{
    uint32_t val = long_read( PTEH ) & 0xFFFFFF00;
    long_write( PTEH, val | asid );
}


void load_utlb_entry( int entryNo, uint32_t vpn, uint32_t ppn, int asid, uint32_t mode )
{
    long_write( (0xF6000000 | (entryNo<<8)), vpn | asid );
    long_write( (0xF7000000 | (entryNo<<8)), ppn | mode );
}


void check_utlb_access( uint32_t addr, uint32_t direct_addr, int mode ) 
{
    
    
}

