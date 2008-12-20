/**
 * $Id$
 * mem.c is responsible for creating and maintaining the overall system memory
 * map, as visible from the SH4 processor. 
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
#define MODULE mem_module

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <glib/gstrfuncs.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <zlib.h>
#include "dream.h"
#include "mem.h"
#include "mmio.h"
#include "dreamcast.h"

sh4ptr_t *page_map = NULL;

int mem_load(FILE *f);
void mem_save(FILE *f);
struct dreamcast_module mem_module =
{ "MEM", mem_init, mem_reset, NULL, NULL, NULL, mem_save, mem_load };

struct mem_region mem_rgn[MAX_MEM_REGIONS];
struct mmio_region *io_rgn[MAX_IO_REGIONS];
struct mmio_region *P4_io[4096];

uint32_t num_io_rgns = 0, num_mem_rgns = 0;

void *mem_alloc_pages( int n )
{
    void *mem = mmap( NULL, n * 4096,
            PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0 );
    if( mem == MAP_FAILED ) {
        ERROR( "Memory allocation failure! (%s)", strerror(errno) );
        return NULL;
    }
    return mem;
}


void mem_init( void )
{
    page_map = mmap( NULL, sizeof(sh4ptr_t) * LXDREAM_PAGE_TABLE_ENTRIES,
            PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0 );
    if( page_map == MAP_FAILED ) {
        ERROR( "Unable to allocate page map! (%s)", strerror(errno) );
        page_map = NULL;
        return;
    }

    memset( page_map, 0, sizeof(sh4ptr_t) * LXDREAM_PAGE_TABLE_ENTRIES );
}

void mem_reset( void )
{
    /* Restore all mmio registers to their initial settings */
    int i, j;
    for( i=1; i<num_io_rgns; i++ ) {
        for( j=0; io_rgn[i]->ports[j].id != NULL; j++ ) {
            if( io_rgn[i]->ports[j].def_val != UNDEFINED &&
                    io_rgn[i]->ports[j].def_val != *io_rgn[i]->ports[j].val ) {
                io_rgn[i]->fn.write_long( io_rgn[i]->ports[j].offset,
                        io_rgn[i]->ports[j].def_val );
            }
        }
    }
}

void mem_save( FILE *f ) 
{
    int i;
    uint32_t len;

    /* All memory regions */
    fwrite( &num_mem_rgns, sizeof(num_mem_rgns), 1, f );
    for( i=0; i<num_mem_rgns; i++ ) {
        fwrite_string( mem_rgn[i].name, f );
        fwrite( &mem_rgn[i].base, sizeof(uint32_t), 1, f );
        fwrite( &mem_rgn[i].flags, sizeof(uint32_t), 1, f );
        fwrite( &mem_rgn[i].size, sizeof(uint32_t), 1, f );
        if( mem_rgn[i].flags != MEM_FLAG_ROM )
            fwrite_gzip( mem_rgn[i].mem, mem_rgn[i].size, 1, f );
    }

    /* All MMIO regions */
    fwrite( &num_io_rgns, sizeof(num_io_rgns), 1, f );
    for( i=0; i<num_io_rgns; i++ ) {
        fwrite_string( io_rgn[i]->id, f );
        fwrite( &io_rgn[i]->base, sizeof( uint32_t ), 1, f );
        len = 4096;
        fwrite( &len, sizeof(len), 1, f );
        fwrite_gzip( io_rgn[i]->mem, len, 1, f );
    }
}

int mem_load( FILE *f )
{
    char tmp[64];
    uint32_t len;
    uint32_t base, size;
    uint32_t flags;
    int i;

    /* All memory regions */
    fread( &len, sizeof(len), 1, f );
    if( len != num_mem_rgns )
        return -1;
    for( i=0; i<len; i++ ) {
        fread_string( tmp, sizeof(tmp), f );
        fread( &base, sizeof(base), 1, f );
        fread( &flags, sizeof(flags), 1, f );
        fread( &size, sizeof(size), 1, f );
        if( strcmp( mem_rgn[i].name, tmp ) != 0 ||
                base != mem_rgn[i].base ||
                flags != mem_rgn[i].flags ||
                size != mem_rgn[i].size ) {
            ERROR( "Bad memory region %d %s", i, tmp );
            return -1;
        }
        if( flags != MEM_FLAG_ROM )
            fread_gzip( mem_rgn[i].mem, size, 1, f );
    }

    /* All MMIO regions */
    fread( &len, sizeof(len), 1, f );
    if( len != num_io_rgns ) 
        return -1;
    for( i=0; i<len; i++ ) {
        fread_string( tmp, sizeof(tmp), f );
        fread( &base, sizeof(base), 1, f );
        fread( &size, sizeof(size), 1, f );
        if( strcmp( io_rgn[i]->id, tmp ) != 0 ||
                base != io_rgn[i]->base ||
                size != 4096 ) {
            ERROR( "Bad MMIO region %d %s", i, tmp );
            return -1;
        }
        fread_gzip( io_rgn[i]->mem, size, 1, f );
    }
    return 0;
}

int mem_save_block( const gchar *file, uint32_t start, uint32_t length )
{
    sh4ptr_t region;
    int len = 4096, total = 0;
    uint32_t addr = start;
    FILE *f = fopen(file,"w");

    if( f == NULL )
        return errno;

    while( total < length ) {
        region = mem_get_region(addr);
        len = 4096 - (addr & 0x0FFF);
        if( len > (length-total) ) 
            len = (length-total);
        if( fwrite( region, len, 1, f ) != 1 ) {
            ERROR( "Unexpected error writing blocks: %d (%s)", len, strerror(errno) );
            break;
        }

        addr += len;
        total += len;
    }
    fclose( f );
    INFO( "Saved %d of %d bytes to %08X", total, length, start );
    return 0;
}

int mem_load_block( const gchar *file, uint32_t start, uint32_t length )
{
    sh4ptr_t region;
    int len = 4096, total = 0;
    uint32_t addr = start;
    struct stat st;
    FILE *f = fopen(file,"r");

    if( f == NULL ) {
        WARN( "Unable to load file '%s': %s", file, strerror(errno) );
        return -1;
    }
    fstat( fileno(f), &st );
    if( length == 0 || length == -1 || length > st.st_size )
        length = st.st_size;

    while( total < length ) {
        region = mem_get_region(addr);
        len = 4096 - (addr & 0x0FFF);
        if( len > (length-total) ) 
            len = (length-total);
        if( fread( region, len, 1, f ) != 1 ) {
            ERROR( "Unexpected error reading: %d (%s)", len, strerror(errno) );
            break;
        }

        addr += len;
        total += len;
    }
    fclose( f );
    INFO( "Loaded %d of %d bytes to %08X", total, length, start );
    return 0;
}

struct mem_region *mem_map_region( void *mem, uint32_t base, uint32_t size,
                                   const char *name, mem_region_fn_t fn, int flags, uint32_t repeat_offset,
                                   uint32_t repeat_until )
{
    int i;
    mem_rgn[num_mem_rgns].base = base;
    mem_rgn[num_mem_rgns].size = size;
    mem_rgn[num_mem_rgns].flags = flags;
    mem_rgn[num_mem_rgns].name = name;
    mem_rgn[num_mem_rgns].mem = mem;
    mem_rgn[num_mem_rgns].fn = fn;
    num_mem_rgns++;

    do {
        for( i=0; i<size>>LXDREAM_PAGE_BITS; i++ )
            page_map[(base>>LXDREAM_PAGE_BITS)+i] = mem + (i<<LXDREAM_PAGE_BITS);
        base += repeat_offset;	
    } while( base <= repeat_until );

    return &mem_rgn[num_mem_rgns-1];
}

void *mem_create_ram_region( uint32_t base, uint32_t size, const char *name, mem_region_fn_t fn )
{
    return mem_create_repeating_ram_region( base, size, name, fn, size, base );
}

void *mem_create_repeating_ram_region( uint32_t base, uint32_t size, const char *name,
                                       mem_region_fn_t fn,
                                       uint32_t repeat_offset, uint32_t repeat_until )
{
    char *mem;

    assert( (base&0xFFFFF000) == base ); /* must be page aligned */
    assert( (size&0x00000FFF) == 0 );
    assert( num_mem_rgns < MAX_MEM_REGIONS );
    assert( page_map != NULL );

    mem = mem_alloc_pages( size>>LXDREAM_PAGE_BITS );

    mem_map_region( mem, base, size, name, fn, MEM_FLAG_RAM, repeat_offset, repeat_until );

    return mem;
}

gboolean mem_load_rom( const gchar *file, uint32_t base, uint32_t size, uint32_t crc,
                       const gchar *region_name, mem_region_fn_t fn )
{
    sh4ptr_t mem;
    uint32_t calc_crc;
    int status;

    mem = mem_get_region(base);
    if( mem == NULL ) {
        mem = mmap( NULL, size, PROT_WRITE|PROT_READ, MAP_ANON|MAP_PRIVATE, -1, 0 );
        if( mem == MAP_FAILED ) {
            ERROR( "Unable to allocate ROM memory: %s (%s)", file, strerror(errno) );
            return FALSE;
        }
        mem_map_region( mem, base, size, region_name, fn, MEM_FLAG_ROM, size, base );
    } else {
        mprotect( mem, size, PROT_READ|PROT_WRITE );
    }

    if( file != NULL && file[0] != '\0' ) {
        status = mem_load_block( file, base, size );
        mprotect( mem, size, PROT_READ );

        if( status == 0 ) {
            /* CRC check only if we loaded something */
            calc_crc = crc32(0L, (sh4ptr_t)mem, size);
            if( calc_crc != crc ) {
                WARN( "Bios CRC Mismatch in %s: %08X (expected %08X)",
                        file, calc_crc, crc);
            }
            return TRUE;
        }
    }
    return FALSE;
}

sh4ptr_t mem_get_region_by_name( const char *name )
{
    int i;
    for( i=0; i<num_mem_rgns; i++ ) {
        if( strcmp( mem_rgn[i].name, name ) == 0 )
            return mem_rgn[i].mem;
    }
    return NULL;
}

void register_io_region( struct mmio_region *io )
{
    int i;

    assert(io);
    io->mem = mem_alloc_pages(2);
    io->save_mem = io->mem + LXDREAM_PAGE_SIZE;
    io->index = (struct mmio_port **)malloc(1024*sizeof(struct mmio_port *));
    io->trace_flag = 0;
    memset( io->index, 0, 1024*sizeof(struct mmio_port *) );
    for( i=0; io->ports[i].id != NULL; i++ ) {
        io->ports[i].val = (uint32_t *)(io->mem + io->ports[i].offset);
        *io->ports[i].val = io->ports[i].def_val;
        io->index[io->ports[i].offset>>2] = &io->ports[i];
    }
    memcpy( io->save_mem, io->mem, LXDREAM_PAGE_SIZE );
    if( (io->base & 0xFF000000) == 0xFF000000 ) {
        /* P4 area (on-chip I/O channels */
        P4_io[(io->base&0x1FFFFFFF)>>19] = io;
    } else {
        page_map[io->base>>12] = (sh4ptr_t)(uintptr_t)num_io_rgns;
    }
    io_rgn[num_io_rgns] = io;
    num_io_rgns++;
}

void register_io_regions( struct mmio_region **io )
{
    while( *io ) register_io_region( *io++ );
}

int mem_has_page( uint32_t addr )
{
    sh4ptr_t page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    return page != NULL;
}

sh4ptr_t mem_get_page( uint32_t addr )
{
    sh4ptr_t page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    return page;
}

sh4ptr_t mem_get_region( uint32_t addr )
{
    sh4ptr_t page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uintptr_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        return NULL;
    } else {
        return page+(addr&0xFFF);
    }
}

void mem_write_long( sh4addr_t addr, uint32_t value )
{
    sh4ptr_t ptr = mem_get_region(addr);
    assert(ptr != NULL);
    *((uint32_t *)ptr) = value;
}

struct mmio_region *mem_get_io_region( uint32_t addr )
{
    if( addr > 0xFF000000 ) {
        return P4_io[(addr&0x00FFFFFF)>>12];
    }
    sh4ptr_t page = page_map[(addr&0x1FFFFFFF)>>12];
    if( ((uintptr_t)page) < MAX_IO_REGIONS ) {
        return io_rgn[(uintptr_t)page];
    } else {
        return NULL;
    }
}

struct mmio_region *mem_get_io_region_by_name( const gchar *name )
{
    int i;
    for( i=0; i<num_io_rgns; i++ ) {
        if( strcasecmp(io_rgn[i]->id, name) == 0 ) {
            return io_rgn[i];
        }
    }
    return NULL;
}

void mem_set_trace( const gchar *tracelist, gboolean flag )
{
    if( tracelist != NULL ) {
        gchar ** tracev = g_strsplit_set( tracelist, ",:; \t\r\n", 0 );
        int i;
        for( i=0; tracev[i] != NULL; i++ ) {
            // Special case "all" - trace everything
            if( strcasecmp(tracev[i], "all") == 0 ) {
                int j;
                for( j=0; j<num_io_rgns; j++ ) {
                    io_rgn[j]->trace_flag = flag ? 1 : 0;
                }
                break;
            }
            struct mmio_region *region = mem_get_io_region_by_name( tracev[i] );
            if( region == NULL ) {
                WARN( "Unknown IO region '%s'", tracev[i] );
            } else {
                region->trace_flag = flag ? 1 : 0;
            }
        }
        g_strfreev( tracev );
    }
}

