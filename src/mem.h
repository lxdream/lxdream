/**
 * $Id$
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
#include "lxdream.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mem_region {
    uint32_t base;
    uint32_t size;
    const char *name;
    sh4ptr_t mem;
    uint32_t flags;
} *mem_region_t;

#define MAX_IO_REGIONS 24
#define MAX_MEM_REGIONS 8

#define MEM_REGION_BIOS "Bios ROM"
#define MEM_REGION_MAIN "System RAM"
#define MEM_REGION_VIDEO "Video RAM"
#define MEM_REGION_AUDIO "Audio RAM"
#define MEM_REGION_AUDIO_SCRATCH "Audio Scratch RAM"
#define MEM_REGION_FLASH "System Flash"

void *mem_create_ram_region( uint32_t base, uint32_t size, const char *name );
void *mem_create_repeating_ram_region( uint32_t base, uint32_t size, const char *name, 
				       uint32_t repeat_offset, uint32_t last_repeat );
/**
 * Load a ROM image from the specified filename. If the memory region has not
 * been allocated, it is created now, otherwise the existing region is reused.
 * If the CRC check fails, a warning will be printed.
 * @return TRUE if the image was loaded successfully (irrespective of CRC failure).
 */
gboolean mem_load_rom( const gchar *filename, uint32_t base, uint32_t size, 
		       uint32_t crc, const gchar *region_name );
void *mem_alloc_pages( int n );
sh4ptr_t mem_get_region( uint32_t addr );
sh4ptr_t mem_get_region_by_name( const char *name );
int mem_has_page( uint32_t addr );
sh4ptr_t mem_get_page( uint32_t addr );
int mem_load_block( const gchar *filename, uint32_t base, uint32_t size );
int mem_save_block( const gchar *filename, uint32_t base, uint32_t size );
void mem_set_trace( const gchar *tracelist, int flag );
void mem_init( void );
void mem_reset( void );
void mem_copy_from_sh4( sh4ptr_t dest, sh4addr_t src, size_t count );
void mem_copy_to_sh4( sh4addr_t dest, sh4ptr_t src, size_t count );

#define ENABLE_DEBUG_MODE 1

typedef enum { BREAK_NONE=0, BREAK_ONESHOT=1, BREAK_KEEP=2 } breakpoint_type_t;

struct breakpoint_struct {
    uint32_t address;
    breakpoint_type_t type;
};

#define MAX_BREAKPOINTS 32


#define MEM_FLAG_ROM 4 /* Mem region is ROM-based */
#define MEM_FLAG_RAM 6 

#define WATCH_WRITE 1
#define WATCH_READ  2
#define WATCH_EXEC  3  /* AKA Breakpoint :) */

typedef struct watch_point *watch_point_t;

watch_point_t mem_new_watch( uint32_t start, uint32_t end, int flags );
void mem_delete_watch( watch_point_t watch );
watch_point_t mem_is_watched( uint32_t addr, int size, int op );

extern sh4ptr_t *page_map;
#ifdef __cplusplus
}
#endif
#endif
