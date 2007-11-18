/**
 * $Id: sh4mem.c,v 1.31 2007-11-08 12:01:57 nkeynes Exp $
 * sh4mem.c is responsible for the SH4's access to memory (including memory
 * mapped I/O), using the page maps created in mem.c
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
#include "sh4/xltcache.h"
#include "pvr2/pvr2.h"
#include "asic.h"

#define OC_BASE 0x1C000000
#define OC_TOP  0x20000000

#define TRANSLATE_VIDEO_64BIT_ADDRESS(a)  ( (((a)&0x00FFFFF8)>>1)|(((a)&0x00000004)<<20)|((a)&0x03)|0x05000000 )

#ifdef ENABLE_WATCH
#define CHECK_READ_WATCH( addr, size ) \
    if( mem_is_watched(addr,size,WATCH_READ) != NULL ) { \
        WARN( "Watch triggered at %08X by %d byte read", addr, size ); \
        dreamcast_stop(); \
    }
#define CHECK_WRITE_WATCH( addr, size, val )                  \
    if( mem_is_watched(addr,size,WATCH_WRITE) != NULL ) { \
        WARN( "Watch triggered at %08X by %d byte write <= %0*X", addr, size, size*2, val ); \
        dreamcast_stop(); \
    }
#else
#define CHECK_READ_WATCH( addr, size )
#define CHECK_WRITE_WATCH( addr, size, val )
#endif

#ifdef ENABLE_TRACE_IO
#define TRACE_IO( str, p, r, ... ) if(io_rgn[(uint32_t)p]->trace_flag && !MMIO_NOTRACE_BYNUM((uint32_t)p,r)) \
    TRACE( str " [%s.%s: %s]", __VA_ARGS__,			       \
    MMIO_NAME_BYNUM((uint32_t)p), MMIO_REGID_BYNUM((uint32_t)p, r), \
    MMIO_REGDESC_BYNUM((uint32_t)p, r) )
#define TRACE_P4IO( str, io, r, ... ) if(io->trace_flag && !MMIO_NOTRACE_IOBYNUM(io,r)) \
TRACE( str " [%s.%s: %s]", __VA_ARGS__, \
    io->id, MMIO_REGID_IOBYNUM(io, r), \
    MMIO_REGDESC_IOBYNUM(io, r) )
#else
#define TRACE_IO( str, p, r, ... )
#define TRACE_P4IO( str, io, r, ... )
#endif

extern struct mem_region mem_rgn[];
extern struct mmio_region *P4_io[];
sh4ptr_t sh4_main_ram;

int32_t sh4_read_p4( sh4addr_t addr )
{
    struct mmio_region *io = P4_io[(addr&0x1FFFFFFF)>>19];
    if( !io ) {
        if( (addr & 0xFF000000) != 0xF4000000 ) {
	    /* OC address cache isn't implemented, but don't complain about it.
	     * Complain about anything else though */
            WARN( "Attempted read from unknown P4 region: %08X", addr );
        }
        return 0;
    } else {
	int32_t val = io->io_read( addr&0xFFF );
	TRACE_P4IO( "Long read %08X <= %08X", io, (addr&0xFFF), val, addr );
        return val;
    }    
}

void sh4_write_p4( sh4addr_t addr, int32_t val )
{
    struct mmio_region *io = P4_io[(addr&0x1FFFFFFF)>>19];
    if( !io ) {
        if( (addr & 0xFC000000) == 0xE0000000 ) {
            /* Store queue */
            SH4_WRITE_STORE_QUEUE( addr, val );
        } else if( (addr & 0xFF000000) != 0xF4000000 ) {
	    /* OC address cache isn't implemented, but don't complain about it.
	     * Complain about anything else though */
            WARN( "Attempted write to unknown P4 region: %08X", addr );
        }
    } else {
	TRACE_P4IO( "Long write %08X => %08X", io, (addr&0xFFF), val, addr );
        io->io_write( addr&0xFFF, val );
    }
}

int32_t sh4_read_phys_word( sh4addr_t addr )
{
    sh4ptr_t page;
    if( addr >= 0xE0000000 ) /* P4 Area, handled specially */
        return SIGNEXT16(sh4_read_p4( addr ));
    
    if( (addr&0x1F800000) == 0x04000000 ) {
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
    }

    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uintptr_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        if( page == NULL ) {
            WARN( "Attempted word read to missing page: %08X",
                   addr );
            return 0;
        }
        return SIGNEXT16(io_rgn[(uintptr_t)page]->io_read(addr&0xFFF));
    } else {
        return SIGNEXT16(*(int16_t *)(page+(addr&0xFFF)));
    }
}

/**
 * Convenience function to read a quad-word (implemented as two long reads).
 */
int64_t sh4_read_quad( sh4addr_t addr )
{
    return ((int64_t)((uint32_t)sh4_read_long(addr))) |
	(((int64_t)((uint32_t)sh4_read_long(addr+4))) << 32);
}

int32_t sh4_read_long( sh4addr_t addr )
{
    sh4ptr_t page;
    
    CHECK_READ_WATCH(addr,4);

    if( addr >= 0xE0000000 ) { /* P4 Area, handled specially */
        return sh4_read_p4( addr );
    } else if( (addr&0x1C000000) == 0x0C000000 ) {
	return *(int32_t *)(sh4_main_ram + (addr&0x00FFFFFF));
    } else if( (addr&0x1F800000) == 0x04000000 ) {
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
	pvr2_render_buffer_invalidate(addr, FALSE);
    } else if( (addr&0x1F800000) == 0x05000000 ) {
	pvr2_render_buffer_invalidate(addr, FALSE);
    }

    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uintptr_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        int32_t val;
        if( page == NULL ) {
            WARN( "Attempted long read to missing page: %08X", addr );
            return 0;
        }
        val = io_rgn[(uintptr_t)page]->io_read(addr&0xFFF);
        TRACE_IO( "Long read %08X <= %08X", page, (addr&0xFFF), val, addr );
        return val;
    } else {
        return *(int32_t *)(page+(addr&0xFFF));
    }
}

int32_t sh4_read_word( sh4addr_t addr )
{
    sh4ptr_t page;

    CHECK_READ_WATCH(addr,2);

    if( addr >= 0xE0000000 ) { /* P4 Area, handled specially */
        return SIGNEXT16(sh4_read_p4( addr ));
    } else if( (addr&0x1C000000) == 0x0C000000 ) {
	return SIGNEXT16(*(int16_t *)(sh4_main_ram + (addr&0x00FFFFFF)));
    } else if( (addr&0x1F800000) == 0x04000000 ) {
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
	pvr2_render_buffer_invalidate(addr, FALSE);
    } else if( (addr&0x1F800000) == 0x05000000 ) {
	pvr2_render_buffer_invalidate(addr, FALSE);
    }
    
    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uintptr_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        int32_t val;
        if( page == NULL ) {
	    WARN( "Attempted word read to missing page: %08X", addr );
            return 0;
        }
        val = SIGNEXT16(io_rgn[(uintptr_t)page]->io_read(addr&0xFFF));
        TRACE_IO( "Word read %04X <= %08X", page, (addr&0xFFF), val&0xFFFF, addr );
        return val;
    } else {
        return SIGNEXT16(*(int16_t *)(page+(addr&0xFFF)));
    }
}

int32_t sh4_read_byte( sh4addr_t addr )
{
    sh4ptr_t page;

    CHECK_READ_WATCH(addr,1);

    if( addr >= 0xE0000000 ) { /* P4 Area, handled specially */
        return SIGNEXT8(sh4_read_p4( addr ));
    } else if( (addr&0x1C000000) == 0x0C000000 ) {
	return SIGNEXT8(*(int8_t *)(sh4_main_ram + (addr&0x00FFFFFF)));
    } else if( (addr&0x1F800000) == 0x04000000 ) {
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
    	pvr2_render_buffer_invalidate(addr, FALSE);
    } else if( (addr&0x1F800000) == 0x05000000 ) {
	pvr2_render_buffer_invalidate(addr, FALSE);
    }

    
    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uintptr_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        int32_t val;
        if( page == NULL ) {
            WARN( "Attempted byte read to missing page: %08X", addr );
            return 0;
        }
        val = SIGNEXT8(io_rgn[(uintptr_t)page]->io_read(addr&0xFFF));
        TRACE_IO( "Byte read %02X <= %08X", page, (addr&0xFFF), val&0xFF, addr );
        return val;
    } else {
        return SIGNEXT8(*(int8_t *)(page+(addr&0xFFF)));
    }
}

/**
 * Convenience function to write a quad-word (implemented as two long writes).
 */
void sh4_write_quad( sh4addr_t addr, uint64_t val )
{
    sh4_write_long( addr, (uint32_t)val );
    sh4_write_long( addr+4, (uint32_t)(val>>32) );
}

void sh4_write_long( sh4addr_t addr, uint32_t val )
{
    sh4ptr_t page;

    CHECK_WRITE_WATCH(addr,4,val);

    if( addr >= 0xE0000000 ) {
        sh4_write_p4( addr, val );
        return;
    } else if( (addr&0x1C000000) == 0x0C000000 ) {
	*(uint32_t *)(sh4_main_ram + (addr&0x00FFFFFF)) = val;
	xlat_invalidate_long(addr);
	return;
    } else if( (addr&0x1F800000) == 0x04000000 || 
	       (addr&0x1F800000) == 0x11000000 ) {
	texcache_invalidate_page(addr& 0x7FFFFF);
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
	pvr2_render_buffer_invalidate(addr, TRUE);
    } else if( (addr&0x1F800000) == 0x05000000 ) {
	pvr2_render_buffer_invalidate(addr, TRUE);
    }

    if( (addr&0x1FFFFFFF) < 0x200000 ) {
        WARN( "Attempted write to read-only memory: %08X => %08X", val, addr);
        sh4_stop();
        return;
    }
    if( (addr&0x1F800000) == 0x00800000 )
	asic_g2_write_word();

    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uintptr_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        if( page == NULL ) {
	    if( (addr & 0x1F000000) >= 0x04000000 &&
		(addr & 0x1F000000) < 0x07000000 )
		return;
            WARN( "Long write to missing page: %08X => %08X", val, addr );
            return;
        }
        TRACE_IO( "Long write %08X => %08X", page, (addr&0xFFF), val, addr );
        io_rgn[(uintptr_t)page]->io_write(addr&0xFFF, val);
    } else {
        *(uint32_t *)(page+(addr&0xFFF)) = val;
    }
}

void sh4_write_word( sh4addr_t addr, uint32_t val )
{
    sh4ptr_t page;

    CHECK_WRITE_WATCH(addr,2,val);

    if( addr >= 0xE0000000 ) {
        sh4_write_p4( addr, (int16_t)val );
        return;
    } else if( (addr&0x1C000000) == 0x0C000000 ) {
	*(uint16_t *)(sh4_main_ram + (addr&0x00FFFFFF)) = val;
	xlat_invalidate_word(addr);
	return;
    } else if( (addr&0x1F800000) == 0x04000000 ||
	(addr&0x1F800000) == 0x11000000 ) {
	texcache_invalidate_page(addr& 0x7FFFFF);
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
	pvr2_render_buffer_invalidate(addr, TRUE);
    } else if( (addr&0x1F800000) == 0x05000000 ) {
	pvr2_render_buffer_invalidate(addr, TRUE);
    }

    if( (addr&0x1FFFFFFF) < 0x200000 ) {
        WARN( "Attempted write to read-only memory: %08X => %08X", val, addr);
        sh4_stop();
        return;
    }
    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uintptr_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        if( page == NULL ) {
            WARN( "Attempted word write to missing page: %08X", addr );
            return;
        }
        TRACE_IO( "Word write %04X => %08X", page, (addr&0xFFF), val&0xFFFF, addr );
        io_rgn[(uintptr_t)page]->io_write(addr&0xFFF, val);
    } else {
        *(uint16_t *)(page+(addr&0xFFF)) = val;
    }
}

void sh4_write_byte( sh4addr_t addr, uint32_t val )
{
    sh4ptr_t page;
    
    CHECK_WRITE_WATCH(addr,1,val);

    if( addr >= 0xE0000000 ) {
        sh4_write_p4( addr, (int8_t)val );
        return;
    } else if( (addr&0x1C000000) == 0x0C000000 ) {
	*(uint8_t *)(sh4_main_ram + (addr&0x00FFFFFF)) = val;
	xlat_invalidate_word(addr);
	return;
    } else if( (addr&0x1F800000) == 0x04000000 ||
	       (addr&0x1F800000) == 0x11000000 ) {
	texcache_invalidate_page(addr& 0x7FFFFF);
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
	pvr2_render_buffer_invalidate(addr, TRUE);
    } else if( (addr&0x1F800000) == 0x05000000 ) {
	pvr2_render_buffer_invalidate(addr, TRUE);
    }
    
    if( (addr&0x1FFFFFFF) < 0x200000 ) {
        WARN( "Attempted write to read-only memory: %08X => %08X", val, addr);
        sh4_stop();
        return;
    }
    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uintptr_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        if( page == NULL ) {
            WARN( "Attempted byte write to missing page: %08X", addr );
            return;
        }
        TRACE_IO( "Byte write %02X => %08X", page, (addr&0xFFF), val&0xFF, addr );
        io_rgn[(uintptr_t)page]->io_write( (addr&0xFFF), val);
    } else {
        *(uint8_t *)(page+(addr&0xFFF)) = val;
    }
}



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

void sh4_flush_store_queue( sh4addr_t addr )
{
    /* Store queue operation */
    int queue = (addr&0x20)>>2;
    sh4ptr_t src = (sh4ptr_t)&sh4r.store_queue[queue];
    uint32_t hi = (MMIO_READ( MMU, (queue == 0 ? QACR0 : QACR1) ) & 0x1C) << 24;
    uint32_t target = (addr&0x03FFFFE0) | hi;
    mem_copy_to_sh4( target, src, 32 );
}
