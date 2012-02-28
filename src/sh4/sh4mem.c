/**
 * $Id$
 * 
 * This is a deprecated module that is not yet completely extricated from the
 * surrounding code.
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

#include <string.h>
#include <zlib.h>
#include "dream.h"
#include "mem.h"
#include "mmio.h"
#include "dreamcast.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"
#include "sh4/mmu.h"
#include "pvr2/pvr2.h"
#include "xlat/xltcache.h"

/************** Obsolete methods ***************/

/* FIXME: Handle all the many special cases when the range doesn't fall cleanly
 * into the same memory block
 */
void mem_copy_from_sh4( sh4ptr_t dest, sh4addr_t srcaddr, size_t count ) {
    if( srcaddr >= 0x04000000 && srcaddr < 0x05000000 ) {
        pvr2_vram64_read( dest, srcaddr, count );
    } else {
        sh4ptr_t src = mem_get_region(srcaddr);
        if( src == NULL ) {
            WARN( "Attempted block read from unknown address %08X", srcaddr );
        } else {
            memcpy( dest, src, count );
        }
    }
}

void mem_copy_to_sh4( sh4addr_t destaddr, sh4ptr_t src, size_t count ) {
    if( destaddr >= 0x10000000 && destaddr < 0x14000000 ) {
        pvr2_dma_write( destaddr, src, count );
        return;
    } else if( (destaddr & 0x1F800000) == 0x05000000 ) {
        pvr2_render_buffer_invalidate( destaddr, TRUE );
    } else if( (destaddr & 0x1F800000) == 0x04000000 ) {
        pvr2_vram64_write( destaddr, src, count );
        return;
    }
    sh4ptr_t dest = mem_get_region(destaddr);
    if( dest == NULL )
        WARN( "Attempted block write to unknown address %08X", destaddr );
    else {
        xlat_invalidate_block( destaddr, count );
        memcpy( dest, src, count );
    }
}
