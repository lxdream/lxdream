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

#ifndef lxdream_mem_H
#define lxdream_mem_H 1

#include <stdint.h>
#include "lxdream.h"
#include "hook.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef FASTCALL int32_t (*mem_read_fn_t)(sh4addr_t);
typedef FASTCALL void (*mem_write_fn_t)(sh4addr_t, uint32_t);
typedef FASTCALL void (*mem_read_burst_fn_t)(unsigned char *,sh4addr_t);
typedef FASTCALL void (*mem_write_burst_fn_t)(sh4addr_t,unsigned char *);
typedef FASTCALL void (*mem_prefetch_fn_t)(sh4addr_t);

typedef FASTCALL int32_t (*mem_read_exc_fn_t)(sh4addr_t, void *);
typedef FASTCALL void (*mem_write_exc_fn_t)(sh4addr_t, uint32_t, void *);
typedef FASTCALL void (*mem_read_burst_exc_fn_t)(unsigned char *,sh4addr_t, void *);
typedef FASTCALL void (*mem_write_burst_exc_fn_t)(sh4addr_t,unsigned char *, void *);
typedef FASTCALL void (*mem_prefetch_exc_fn_t)(sh4addr_t, void *);

/**
 * Basic memory region vtable - read/write at byte, word, long, and burst 
 * (32-byte) sizes.
 */
typedef struct mem_region_fn {
    mem_read_fn_t read_long;
    mem_write_fn_t write_long;
    mem_read_fn_t read_word;
    mem_write_fn_t write_word;
    mem_read_fn_t read_byte;
    mem_write_fn_t write_byte;
    mem_read_burst_fn_t read_burst;
    mem_write_burst_fn_t write_burst;
    /* Prefetch is provided as a convenience for the SH4 - external memory 
     * spaces are automatically forced to unmapped_prefetch by mem.c
     */
    mem_prefetch_fn_t prefetch;
    /* Convenience for SH4 byte read/modify/write instructions */
    mem_read_fn_t read_byte_for_write;
} *mem_region_fn_t;

int32_t FASTCALL unmapped_read_long( sh4addr_t addr );
void FASTCALL unmapped_write_long( sh4addr_t addr, uint32_t val );
void FASTCALL unmapped_read_burst( unsigned char *dest, sh4addr_t addr );
void FASTCALL unmapped_write_burst( sh4addr_t addr, unsigned char *src );
void FASTCALL unmapped_prefetch( sh4addr_t addr );
extern struct mem_region_fn mem_region_unmapped;

typedef struct mem_region {
    uint32_t base;
    uint32_t size;
    const char *name;
    sh4ptr_t mem;
    uint32_t flags;
    mem_region_fn_t fn;
} *mem_region_t;

#define MAX_IO_REGIONS 24
#define MAX_MEM_REGIONS 16

#define MEM_REGION_BIOS "Bios ROM"
#define MEM_REGION_MAIN "System RAM"
#define MEM_REGION_VIDEO "Video RAM"
#define MEM_REGION_VIDEO64 "Video RAM 64-bit"
#define MEM_REGION_AUDIO "Audio RAM"
#define MEM_REGION_AUDIO_SCRATCH "Audio Scratch RAM"
#define MEM_REGION_FLASH "System Flash"
#define MEM_REGION_PVR2TA "PVR2 TA Command"
#define MEM_REGION_PVR2YUV "PVR2 YUV Decode"
#define MEM_REGION_PVR2VDMA1 "PVR2 VRAM DMA 1"
#define MEM_REGION_PVR2VDMA2 "PVR2 VRAM DMA 2"

typedef gboolean (*mem_page_remapped_hook_t)(sh4addr_t page, mem_region_fn_t newfn, void *user_data);
DECLARE_HOOK( mem_page_remapped_hook, mem_page_remapped_hook_t );

struct mem_region *mem_map_region( void *mem, uint32_t base, uint32_t size,
                                   const char *name, mem_region_fn_t fn, int flags, uint32_t repeat_offset,
                                   uint32_t repeat_until );

/**
 * Load a ROM image from the specified filename. If the memory region has not
 * been allocated, it is created now, otherwise the existing region is reused.
 * If the CRC check fails, a warning will be printed.
 * @return TRUE if the image was loaded successfully (irrespective of CRC failure).
 */
gboolean mem_load_rom( void *output, const gchar *filename, uint32_t size, uint32_t crc ); 
void *mem_alloc_pages( int n );
sh4ptr_t mem_get_region( uint32_t addr );
sh4ptr_t mem_get_region_by_name( const char *name );
gboolean mem_has_page( uint32_t addr );
int mem_load_block( const gchar *filename, uint32_t base, uint32_t size );
int mem_save_block( const gchar *filename, uint32_t base, uint32_t size );
void mem_set_trace( const gchar *tracelist, int flag );
void mem_init( void );
void mem_reset( void );
void mem_copy_from_sh4( sh4ptr_t dest, sh4addr_t src, size_t count );
void mem_copy_to_sh4( sh4addr_t dest, sh4ptr_t src, size_t count );

/**
 * Write a long value directly to SH4-addressable memory.
 * @param dest a valid, writable physical memory address, relative to the SH4
 * @param value the value to write.
 */
void mem_write_long( sh4addr_t dest, uint32_t value );

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

extern mem_region_fn_t *ext_address_space;

#define SIGNEXT4(n) ((((int32_t)(n))<<28)>>28)
#define SIGNEXT8(n) ((int32_t)((int8_t)(n)))
#define SIGNEXT12(n) ((((int32_t)(n))<<20)>>20)
#define SIGNEXT16(n) ((int32_t)((int16_t)(n)))
#define SIGNEXT32(n) ((int64_t)((int32_t)(n)))
#define SIGNEXT48(n) ((((int64_t)(n))<<16)>>16)
#define ZEROEXT32(n) ((int64_t)((uint64_t)((uint32_t)(n))))

/* Ensure the given region allows all of read/write/execute. If not 
 * page-aligned, some surrounding regions will similarly be unprotected.
 */
void mem_unprotect( void *ptr, uint32_t size );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_mem_H */
