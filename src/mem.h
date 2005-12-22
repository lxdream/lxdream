#ifndef dream_sh4_mem_H
#define dream_sh4_mem_H

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
