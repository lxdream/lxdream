/**
 * $Id: sh4mem.c,v 1.9 2006-03-30 11:26:45 nkeynes Exp $
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
#include "sh4core.h"
#include "sh4mmio.h"
#include "dreamcast.h"
#include "pvr2/pvr2.h"

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
#define CHECK_WRITE_WATCH( addr, size )
#endif

#define TRACE_IO( str, p, r, ... ) if(io_rgn[(uint32_t)p]->trace_flag) \
TRACE( str " [%s.%s: %s]", __VA_ARGS__, \
    MMIO_NAME_BYNUM((uint32_t)p), MMIO_REGID_BYNUM((uint32_t)p, r), \
    MMIO_REGDESC_BYNUM((uint32_t)p, r) )

extern struct mem_region mem_rgn[];
extern struct mmio_region *P4_io[];

int32_t sh4_read_p4( uint32_t addr )
{
    struct mmio_region *io = P4_io[(addr&0x1FFFFFFF)>>19];
    if( !io ) {
        ERROR( "Attempted read from unknown P4 region: %08X", addr );
        return 0;
    } else {
        return io->io_read( addr&0xFFF );
    }    
}

void sh4_write_p4( uint32_t addr, int32_t val )
{
    struct mmio_region *io = P4_io[(addr&0x1FFFFFFF)>>19];
    if( !io ) {
        if( (addr & 0xFC000000) == 0xE0000000 ) {
            /* Store queue */
            SH4_WRITE_STORE_QUEUE( addr, val );
        } else if( (addr & 0xFF000000) != 0xF4000000 ) {
	    /* OC address cache isn't implemented, but don't complain about it.
	     * Complain about anything else though */
            ERROR( "Attempted write to unknown P4 region: %08X", addr );
        }
    } else {
        io->io_write( addr&0xFFF, val );
    }
}

int32_t sh4_read_phys_word( uint32_t addr )
{
    char *page;
    if( addr >= 0xE0000000 ) /* P4 Area, handled specially */
        return SIGNEXT16(sh4_read_p4( addr ));
    
    if( (addr&0x1F800000) == 0x04000000 ) {
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
    }

    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uint32_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        if( page == NULL ) {
            ERROR( "Attempted word read to missing page: %08X",
                   addr );
            return 0;
        }
        return SIGNEXT16(io_rgn[(uint32_t)page]->io_read(addr&0xFFF));
    } else {
        return SIGNEXT16(*(int16_t *)(page+(addr&0xFFF)));
    }
}

int32_t sh4_read_long( uint32_t addr )
{
    char *page;
    
    CHECK_READ_WATCH(addr,4);

    if( addr >= 0xE0000000 ) /* P4 Area, handled specially */
        return sh4_read_p4( addr );
    
    if( (addr&0x1F800000) == 0x04000000 ) {
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
    }

    if( IS_MMU_ENABLED() ) {
        ERROR( "user-mode & mmu translation not implemented, aborting", NULL );
        sh4_stop();
        return 0;
    }

    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uint32_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        int32_t val;
        if( page == NULL ) {
            ERROR( "Attempted long read to missing page: %08X", addr );
            return 0;
        }
        val = io_rgn[(uint32_t)page]->io_read(addr&0xFFF);
        TRACE_IO( "Long read %08X <= %08X", page, (addr&0xFFF), val, addr );
        return val;
    } else {
        return *(int32_t *)(page+(addr&0xFFF));
    }
}

int32_t sh4_read_word( uint32_t addr )
{
    char *page;

    CHECK_READ_WATCH(addr,2);

    if( addr >= 0xE0000000 ) /* P4 Area, handled specially */
        return SIGNEXT16(sh4_read_p4( addr ));
    
    if( (addr&0x1F800000) == 0x04000000 ) {
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
    }

    if( IS_MMU_ENABLED() ) {
        ERROR( "user-mode & mmu translation not implemented, aborting", NULL );
        sh4_stop();
        return 0;
    }

    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uint32_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        int32_t val;
        if( page == NULL ) {
            ERROR( "Attempted word read to missing page: %08X", addr );
            return 0;
        }
        val = SIGNEXT16(io_rgn[(uint32_t)page]->io_read(addr&0xFFF));
        TRACE_IO( "Word read %04X <= %08X", page, (addr&0xFFF), val&0xFFFF, addr );
        return val;
    } else {
        return SIGNEXT16(*(int16_t *)(page+(addr&0xFFF)));
    }
}

int32_t sh4_read_byte( uint32_t addr )
{
    char *page;

    CHECK_READ_WATCH(addr,1);

    if( addr >= 0xE0000000 ) /* P4 Area, handled specially */
        return SIGNEXT8(sh4_read_p4( addr ));
    if( (addr&0x1F800000) == 0x04000000 ) {
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
    }
    
    if( IS_MMU_ENABLED() ) {
        ERROR( "user-mode & mmu translation not implemented, aborting", NULL );
        sh4_stop();
        return 0;
    }

    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uint32_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        int32_t val;
        if( page == NULL ) {
            ERROR( "Attempted byte read to missing page: %08X", addr );
            return 0;
        }
        val = SIGNEXT8(io_rgn[(uint32_t)page]->io_read(addr&0xFFF));
        TRACE_IO( "Byte read %02X <= %08X", page, (addr&0xFFF), val&0xFF, addr );
        return val;
    } else {
        return SIGNEXT8(*(int8_t *)(page+(addr&0xFFF)));
    }
}

void sh4_write_long( uint32_t addr, uint32_t val )
{
    char *page;
    
    CHECK_WRITE_WATCH(addr,4,val);

    if( addr >= 0xE0000000 ) {
        sh4_write_p4( addr, val );
        return;
    }
    if( (addr&0x1F800000) == 0x04000000 || 
	(addr&0x1F800000) == 0x11000000 ) {
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
    }

    if( IS_MMU_ENABLED() ) {
        ERROR( "user-mode & mmu translation not implemented, aborting", NULL );
        sh4_stop();
        return;
    }
    if( (addr&0x1FFFFFFF) < 0x200000 ) {
        ERROR( "Attempted write to read-only memory: %08X => %08X", val, addr);
        sh4_stop();
        return;
    }
    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uint32_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        if( page == NULL ) {
            ERROR( "Long write to missing page: %08X => %08X", val, addr );
            return;
        }
        TRACE_IO( "Long write %08X => %08X", page, (addr&0xFFF), val, addr );
        io_rgn[(uint32_t)page]->io_write(addr&0xFFF, val);
    } else {
        *(uint32_t *)(page+(addr&0xFFF)) = val;
    }
}

void sh4_write_word( uint32_t addr, uint32_t val )
{
    char *page;

    CHECK_WRITE_WATCH(addr,2,val);

    if( addr >= 0xE0000000 ) {
        sh4_write_p4( addr, (int16_t)val );
        return;
    }
    if( (addr&0x1F800000) == 0x04000000 ||
	(addr&0x1F800000) == 0x11000000 ) {
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
    }
    if( IS_MMU_ENABLED() ) {
        ERROR( "user-mode & mmu translation not implemented, aborting", NULL );
        sh4_stop();
        return;
    }
    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uint32_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        if( page == NULL ) {
            ERROR( "Attempted word write to missing page: %08X", addr );
            return;
        }
        TRACE_IO( "Word write %04X => %08X", page, (addr&0xFFF), val&0xFFFF, addr );
        io_rgn[(uint32_t)page]->io_write(addr&0xFFF, val);
    } else {
        *(uint16_t *)(page+(addr&0xFFF)) = val;
    }
}

void sh4_write_byte( uint32_t addr, uint32_t val )
{
    char *page;
    
    CHECK_WRITE_WATCH(addr,1,val);

    if( addr >= 0xE0000000 ) {
        sh4_write_p4( addr, (int8_t)val );
        return;
    }
    if( (addr&0x1F800000) == 0x04000000 ||
	(addr&0x1F800000) == 0x11000000 ) {
        addr = TRANSLATE_VIDEO_64BIT_ADDRESS(addr);
    }
    
    if( IS_MMU_ENABLED() ) {
        ERROR( "user-mode & mmu translation not implemented, aborting", NULL );
        sh4_stop();
        return;
    }
    page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uint32_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        if( page == NULL ) {
            ERROR( "Attempted byte write to missing page: %08X", addr );
            return;
        }
        TRACE_IO( "Byte write %02X => %08X", page, (addr&0xFFF), val&0xFF, addr );
        io_rgn[(uint32_t)page]->io_write( (addr&0xFFF), val);
    } else {
        *(uint8_t *)(page+(addr&0xFFF)) = val;
    }
}



/* FIXME: Handle all the many special cases when the range doesn't fall cleanly
 * into the same memory black
 */
void mem_copy_from_sh4( char *dest, uint32_t srcaddr, size_t count ) {
    if( srcaddr >= 0x04000000 && srcaddr < 0x05000000 ) {
	pvr2_vram64_read( dest, srcaddr, count );
    } else {
	char *src = mem_get_region(srcaddr);
	if( src == NULL ) {
	    ERROR( "Attempted block read from unknown address %08X", srcaddr );
	} else {
	    memcpy( dest, src, count );
	}
    }
}

void mem_copy_to_sh4( uint32_t destaddr, char *src, size_t count ) {
    if( destaddr >= 0x10000000 && destaddr < 0x11000000 ) {
	pvr2_ta_write( src, count );
    } else if( destaddr >= 0x04000000 && destaddr < 0x05000000 ||
	       destaddr >= 0x11000000 && destaddr < 0x12000000 ) {
	pvr2_vram64_write( destaddr, src, count );
    } else {
	char *dest = mem_get_region(destaddr);
	if( dest == NULL )
	    ERROR( "Attempted block write to unknown address %08X", destaddr );
	else {
	    memcpy( dest, src, count );
	}
    }
}
