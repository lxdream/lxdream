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
#include "sh4core.h"
#include "mem.h"
#include "dreamcast.h"

#define OC_BASE 0x1C000000
#define OC_TOP  0x20000000

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

#define TRANSLATE_VIDEO_64BIT_ADDRESS(a)  ( (((a)&0x00FFFFF8)>>1)|(((a)&0x00000004)<<20)|((a)&0x03)|0x05000000 );

static char **page_map = NULL;
static char *cache = NULL;

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
    cache = mem_alloc_pages(2);
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

#define OCRAM_START (0x1C000000>>PAGE_BITS)
#define OCRAM_END   (0x20000000>>PAGE_BITS)

void mem_set_cache_mode( int mode )
{
    uint32_t i;
    switch( mode ) {
        case MEM_OC_INDEX0: /* OIX=0 */
            for( i=OCRAM_START; i<OCRAM_END; i++ )
                page_map[i] = cache + ((i&0x02)<<(PAGE_BITS-1));
            break;
        case MEM_OC_INDEX1: /* OIX=1 */
            for( i=OCRAM_START; i<OCRAM_END; i++ )
                page_map[i] = cache + ((i&0x02000000)>>(25-PAGE_BITS));
            break;
        default: /* disabled */
            for( i=OCRAM_START; i<OCRAM_END; i++ )
                page_map[i] = NULL;
            break;
    }
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

#define TRACE_IO( str, p, r, ... ) if(io_rgn[(uint32_t)p]->trace_flag) \
TRACE( str " [%s.%s: %s]", __VA_ARGS__, \
    MMIO_NAME_BYNUM((uint32_t)p), MMIO_REGID_BYNUM((uint32_t)p, r), \
    MMIO_REGDESC_BYNUM((uint32_t)p, r) )


int32_t mem_read_p4( uint32_t addr )
{
    struct mmio_region *io = P4_io[(addr&0x1FFFFFFF)>>19];
    if( !io ) {
        ERROR( "Attempted read from unknown P4 region: %08X", addr );
        return 0;
    } else {
        return io->io_read( addr&0xFFF );
    }    
}

void mem_write_p4( uint32_t addr, int32_t val )
{
    struct mmio_region *io = P4_io[(addr&0x1FFFFFFF)>>19];
    if( !io ) {
        if( (addr & 0xFC000000) == 0xE0000000 ) {
            /* Store queue */
            SH4_WRITE_STORE_QUEUE( addr, val );
        } else {
            ERROR( "Attempted write to unknown P4 region: %08X", addr );
        }
    } else {
        io->io_write( addr&0xFFF, val );
    }
}

int mem_has_page( uint32_t addr )
{
    char *page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    return page != NULL;
}

int32_t mem_read_phys_word( uint32_t addr )
{
    char *page;
    if( addr > 0xE0000000 ) /* P4 Area, handled specially */
        return SIGNEXT16(mem_read_p4( addr ));
    
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

int32_t mem_read_long( uint32_t addr )
{
    char *page;
    
    CHECK_READ_WATCH(addr,4);

    if( addr > 0xE0000000 ) /* P4 Area, handled specially */
        return mem_read_p4( addr );
    
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

int32_t mem_read_word( uint32_t addr )
{
    char *page;

    CHECK_READ_WATCH(addr,2);

    if( addr > 0xE0000000 ) /* P4 Area, handled specially */
        return SIGNEXT16(mem_read_p4( addr ));
    
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

int32_t mem_read_byte( uint32_t addr )
{
    char *page;

    CHECK_READ_WATCH(addr,1);

    if( addr > 0xE0000000 ) /* P4 Area, handled specially */
        return SIGNEXT8(mem_read_p4( addr ));
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

void mem_write_long( uint32_t addr, uint32_t val )
{
    char *page;
    
    CHECK_WRITE_WATCH(addr,4,val);

    if( addr > 0xE0000000 ) {
        mem_write_p4( addr, val );
        return;
    }
    if( (addr&0x1F800000) == 0x04000000 ) {
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

void mem_write_word( uint32_t addr, uint32_t val )
{
    char *page;

    CHECK_WRITE_WATCH(addr,2,val);

    if( addr > 0xE0000000 ) {
        mem_write_p4( addr, (int16_t)val );
        return;
    }
    if( (addr&0x1F800000) == 0x04000000 ) {
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

void mem_write_byte( uint32_t addr, uint32_t val )
{
    char *page;
    
    CHECK_WRITE_WATCH(addr,1,val);

    if( addr > 0xE0000000 ) {
        mem_write_p4( addr, (int8_t)val );
        return;
    }
    if( (addr&0x1F800000) == 0x04000000 ) {
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

char *mem_get_region( uint32_t addr )
{
    char *page = page_map[ (addr & 0x1FFFFFFF) >> 12 ];
    if( ((uint32_t)page) < MAX_IO_REGIONS ) { /* IO Region */
        return NULL;
    } else {
        return page+(addr&0xFFF);
    }
}

/* FIXME: Handle all the many special cases when the range doesn't fall cleanly
 * into the same memory black
 */

void mem_copy_from_sh4( char *dest, uint32_t srcaddr, size_t count ) {
    char *src = mem_get_region(srcaddr);
    memcpy( dest, src, count );
}

void mem_copy_to_sh4( uint32_t destaddr, char *src, size_t count ) {
    char *dest = mem_get_region(destaddr);
    memcpy( dest, src, count );
}
