/**
 * $Id: mem.h,v 1.5 2005-12-25 08:24:07 nkeynes Exp $
 *
 * mem is responsible for creating and maintaining the overall system memory
 * map, as visible from the SH4 processor. (Note the ARM has a different map)
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

#ifndef dream_mem_H
#define dream_mem_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mem_region {
    uint32_t base;
    uint32_t size;
    char *name;
    char *mem;
    int flags;
} *mem_region_t;

#define MAX_IO_REGIONS 24
#define MAX_MEM_REGIONS 8

#define MEM_REGION_MAIN "System RAM"
#define MEM_REGION_VIDEO "Video RAM"
#define MEM_REGION_AUDIO "Audio RAM"
#define MEM_REGION_AUDIO_SCRATCH "Audio Scratch RAM"

#define MB * (1024 * 1024)
#define KB * 1024

void *mem_create_ram_region( uint32_t base, uint32_t size, char *name );
void *mem_load_rom( char *name, uint32_t base, uint32_t size, uint32_t crc );
void *mem_alloc_pages( int n );
char *mem_get_region( uint32_t addr );
char *mem_get_region_by_name( char *name );
int mem_has_page( uint32_t addr );
char *mem_get_page( uint32_t addr );

void mem_init( void );
void mem_reset( void );

#define ENABLE_WATCH 1

#define WATCH_WRITE 1
#define WATCH_READ  2
#define WATCH_EXEC  3  /* AKA Breakpoint :) */

#define MEM_FLAG_ROM 4 /* Mem region is ROM-based */
#define MEM_FLAG_RAM 6 

typedef struct watch_point *watch_point_t;

watch_point_t mem_new_watch( uint32_t start, uint32_t end, int flags );
void mem_delete_watch( watch_point_t watch );
watch_point_t mem_is_watched( uint32_t addr, int size, int op );

extern char **page_map;
#ifdef __cplusplus
}
#endif
#endif
