#ifndef dream_sh4_mem_H
#define dream_sh4_mem_H

#include <stdint.h>
#include "sh4mmio.h"

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
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

int32_t mem_read_long( uint32_t addr );
int32_t mem_read_word( uint32_t addr );
int32_t mem_read_byte( uint32_t addr );
void mem_write_long( uint32_t addr, uint32_t val );
void mem_write_word( uint32_t addr, uint32_t val );
void mem_write_byte( uint32_t addr, uint32_t val );

int32_t mem_read_phys_word( uint32_t addr );
void *mem_create_ram_region( uint32_t base, uint32_t size, char *name );
void *mem_load_rom( char *name, uint32_t base, uint32_t size, uint32_t crc );
char *mem_get_region( uint32_t addr );
char *mem_get_region_by_name( char *name );
void mem_set_cache_mode( int );
int mem_has_page( uint32_t addr );

void mem_init( void );
void mem_reset( void );

#define ENABLE_WATCH 1

#define WATCH_WRITE 1
#define WATCH_READ  2
#define WATCH_EXEC  3  /* AKA Breakpoint :) */

typedef struct watch_point *watch_point_t;

watch_point_t mem_new_watch( uint32_t start, uint32_t end, int flags );
void mem_delete_watch( watch_point_t watch );
watch_point_t mem_is_watched( uint32_t addr, int size, int op );

/* mmucr register bits */
#define MMUCR_AT   0x00000001 /* Address Translation enabled */
#define MMUCR_TI   0x00000004 /* TLB invalidate (always read as 0) */
#define MMUCR_SV   0x00000100 /* Single Virtual mode=1 / multiple virtual=0 */
#define MMUCR_SQMD 0x00000200 /* Store queue mode bit (0=user, 1=priv only) */
#define MMUCR_URC  0x0000FC00 /* UTLB access counter */
#define MMUCR_URB  0x00FC0000 /* UTLB entry boundary */
#define MMUCR_LRUI 0xFC000000 /* Least recently used ITLB */
#define MMUCR_MASK 0xFCFCFF05
#define MMUCR_RMASK 0xFCFCFF01 /* Read mask */

#define IS_MMU_ENABLED() (MMIO_READ(MMU, MMUCR)&MMUCR_AT)

/* ccr register bits */
#define CCR_IIX    0x00008000 /* IC index enable */
#define CCR_ICI    0x00000800 /* IC invalidation (always read as 0) */
#define CCR_ICE    0x00000100 /* IC enable */
#define CCR_OIX    0x00000080 /* OC index enable */
#define CCR_ORA    0x00000020 /* OC RAM enable */
#define CCR_OCI    0x00000008 /* OC invalidation (always read as 0) */
#define CCR_CB     0x00000004 /* Copy-back (P1 area cache write mode) */
#define CCR_WT     0x00000002 /* Write-through (P0,U0,P3 write mode) */
#define CCR_OCE    0x00000001 /* OC enable */
#define CCR_MASK   0x000089AF
#define CCR_RMASK  0x000081A7 /* Read mask */

#define MEM_OC_DISABLED 0
#define MEM_OC_INDEX0   CCR_ORA
#define MEM_OC_INDEX1   CCR_ORA|CCR_OIX

#ifdef __cplusplus
}
#endif
#endif
