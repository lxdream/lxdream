/**
 * $Id: mem.c,v 1.4 2005-12-13 14:47:59 nkeynes Exp $
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

#include <sys/mman.h>
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
#include "modules.h"
#include "dreamcast.h"

char **page_map = NULL;

int mem_load(FILE *f);
void mem_save(FILE *f);
struct dreamcast_module mem_module =
    { "MEM", mem_init, mem_reset, NULL, NULL, mem_save, mem_load };

struct mem_region mem_rgn[MAX_MEM_REGIONS];
struct mmio_region *io_rgn[MAX_IO_REGIONS];
struct mmio_region *P4_io[4096];

int num_io_rgns = 1, num_mem_rgns = 0;

void *mem_alloc_pages( int n )
{
    void *mem = mmap( NULL, n * 4096,
                      PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0 );
    if( mem == MAP_FAILED ) {
        ERROR( "Memory allocation failure! (%s)", strerror(errno) );
        return NULL;
    }
    return mem;
}


void mem_init( void )
{
    page_map = mmap( NULL, sizeof(char *) * PAGE_TABLE_ENTRIES,
                     PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0 );
    if( page_map == MAP_FAILED ) {
        ERROR( "Unable to allocate page map! (%s)", strerror(errno) );
        page_map = NULL;
        return;
    }

    memset( page_map, 0, sizeof(uint32_t) * PAGE_TABLE_ENTRIES );
}

void mem_reset( void )
{
    /* Restore all mmio registers to their initial settings */
    int i, j;
    for( i=1; i<num_io_rgns; i++ ) {
        for( j=0; io_rgn[i]->ports[j].id != NULL; j++ ) {
            if( io_rgn[i]->ports[j].def_val != UNDEFINED &&
                io_rgn[i]->ports[j].def_val != *io_rgn[i]->ports[j].val ) {
                io_rgn[i]->io_write( io_rgn[i]->ports[j].offset,
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
	fwrite( &mem_rgn[i].flags, sizeof(int), 1, f );
	fwrite( &mem_rgn[i].size, sizeof(uint32_t), 1, f );
	fwrite( mem_rgn[i].mem, mem_rgn[i].size, 1, f );
    }

    /* All MMIO regions */
    fwrite( &num_io_rgns, sizeof(num_io_rgns), 1, f );
    for( i=0; i<num_io_rgns; i++ ) {
	fwrite_string( io_rgn[i]->id, f );
	fwrite( &io_rgn[i]->base, sizeof( uint32_t ), 1, f );
	fwrite( io_rgn[i]->mem, 4096, 1, f );
    }
}

int mem_load( FILE *f )
{
    char tmp[64];
    uint32_t len;
    int i;

    /* All memory regions */
    
}

struct mem_region *mem_map_region( void *mem, uint32_t base, uint32_t size,
                                   char *name, int flags )
{
    int i;
    mem_rgn[num_mem_rgns].base = base;
    mem_rgn[num_mem_rgns].size = size;
    mem_rgn[num_mem_rgns].flags = flags;
    mem_rgn[num_mem_rgns].name = name;
    mem_rgn[num_mem_rgns].mem = mem;
    num_mem_rgns++;

    for( i=0; i<size>>PAGE_BITS; i++ )
        page_map[(base>>PAGE_BITS)+i] = mem + (i<<PAGE_BITS);

    return &mem_rgn[num_mem_rgns-1];
}

void *mem_create_ram_region( uint32_t base, uint32_t size, char *name )
{
    char *mem;
    
    assert( (base&0xFFFFF000) == base ); /* must be page aligned */
    assert( (size&0x00000FFF) == 0 );
    assert( num_mem_rgns < MAX_MEM_REGIONS );
    assert( page_map != NULL );

    mem = mem_alloc_pages( size>>PAGE_BITS );

    mem_map_region( mem, base, size, name, 6 );
    return mem;
}

void *mem_load_rom( char *file, uint32_t base, uint32_t size, uint32_t crc )
{
    char buf[512], *mem;
    int fd;
    uint32_t calc_crc;
    snprintf( buf, 512, "%s/%s",BIOS_PATH, file );
    fd = open( buf, O_RDONLY );
    if( fd == -1 ) {
        ERROR( "Bios file not found: %s", buf );
        return NULL;
    }
    mem = mmap( NULL, size, PROT_READ, MAP_PRIVATE, fd, 0 );
    if( mem == MAP_FAILED ) {
        ERROR( "Unable to map bios file: %s (%s)", file, strerror(errno) );
        close(fd);
        return NULL;
    }
    mem_map_region( mem, base, size, file, 4 );

    /* CRC check */
    calc_crc = crc32(0L, mem, size);
    if( calc_crc != crc ) {
        WARN( "Bios CRC Mismatch in %s: %08X (expected %08X)",
              file, calc_crc, crc);
    }
    return mem;
}

char *mem_get_region_by_name( char *name )
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
    io->save_mem = io->mem + PAGE_SIZE;
    io->index = (struct mmio_port **)malloc(1024*sizeof(struct mmio_port *));
    io->trace_flag = 1;
    memset( io->index, 0, 1024*sizeof(struct mmio_port *) );
    for( i=0; io->ports[i].id != NULL; i++ ) {
        io->ports[i].val = (uint32_t *)(io->mem + io->ports[i].offset);
        *io->ports[i].val = io->ports[i].def_val;
        io->index[io->ports[i].offset>>2] = &io->ports[i];
    }
    memcpy( io->save_mem, io->mem, PAGE_SIZE );
    if( (io->base & 0xFF000000) == 0xFF000000 ) {
        /* P4 area (on-chip I/O channels */
        P4_io[(io->base&0x1FFFFFFF)>>19] = io;
    } else {
        page_map[io->base>>12] = (char *)num_io_rgns;
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
    char *page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    return page != NULL;
}

char *mem_get_page( uint32_t addr )
{
    char *page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    return page;
}

char *mem_get_region( uint32_t addr )
{
    char *page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uint32_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        return NULL;
    } else {
        return page+(addr&0xFFF);
    }
}

