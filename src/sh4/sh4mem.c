/**
 * $Id$
 * sh4mem.c is responsible for interfacing between the SH4's internal memory
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

/* System regions (probably should be defined elsewhere) */
extern struct mem_region_fn mem_region_unmapped;
extern struct mem_region_fn mem_region_sdram;
extern struct mem_region_fn mem_region_vram32;
extern struct mem_region_fn mem_region_vram64;
extern struct mem_region_fn mem_region_audioram;
extern struct mem_region_fn mem_region_flashram;
extern struct mem_region_fn mem_region_bootrom;

/* On-chip regions other than defined MMIO regions */
extern struct mem_region_fn mem_region_storequeue;
extern struct mem_region_fn mem_region_icache_addr;
extern struct mem_region_fn mem_region_icache_data;
extern struct mem_region_fn mem_region_ocache_addr;
extern struct mem_region_fn mem_region_ocache_data;
extern struct mem_region_fn mem_region_itlb_addr;
extern struct mem_region_fn mem_region_itlb_data;
extern struct mem_region_fn mem_region_utlb_addr;
extern struct mem_region_fn mem_region_utlb_data;

extern struct mmio_region *P4_io[];

/********************* The "unmapped" address space ********************/
/* Always reads as 0, writes have no effect */
static int32_t FASTCALL unmapped_read_long( sh4addr_t addr )
{
    return 0;
}
static void FASTCALL unmapped_write_long( sh4addr_t addr, uint32_t val )
{
}
static void FASTCALL unmapped_read_burst( unsigned char *dest, sh4addr_t addr )
{
    memset( dest, 0, 32 );
}
static void FASTCALL unmapped_write_burst( sh4addr_t addr, unsigned char *src )
{
}

struct mem_region_fn mem_region_unmapped = { 
        unmapped_read_long, unmapped_write_long, 
        unmapped_read_long, unmapped_write_long, 
        unmapped_read_long, unmapped_write_long, 
        unmapped_read_burst, unmapped_write_burst }; 

/********************* Store-queue (long-write only?) ******************/
static void FASTCALL p4_storequeue_write_long( sh4addr_t addr, uint32_t val )
{
    sh4r.store_queue[(addr>>2)&0xF] = val;
}
static int32_t FASTCALL p4_storequeue_read_long( sh4addr_t addr )
{
    return sh4r.store_queue[(addr>>2)&0xF];
}
struct mem_region_fn p4_region_storequeue = { 
        p4_storequeue_read_long, p4_storequeue_write_long,
        p4_storequeue_read_long, p4_storequeue_write_long,
        p4_storequeue_read_long, p4_storequeue_write_long,
        unmapped_read_burst, unmapped_write_burst }; // No burst access.

/********************* The main ram address space **********************/
static int32_t FASTCALL ext_sdram_read_long( sh4addr_t addr )
{
    return *((int32_t *)(sh4_main_ram + (addr&0x00FFFFFF)));
}
static int32_t FASTCALL ext_sdram_read_word( sh4addr_t addr )
{
    return SIGNEXT16(*((int16_t *)(sh4_main_ram + (addr&0x00FFFFFF))));
}
static int32_t FASTCALL ext_sdram_read_byte( sh4addr_t addr )
{
    return SIGNEXT8(*((int16_t *)(sh4_main_ram + (addr&0x00FFFFFF))));
}
static void FASTCALL ext_sdram_write_long( sh4addr_t addr, uint32_t val )
{
    *(uint32_t *)(sh4_main_ram + (addr&0x00FFFFFF)) = val;
    xlat_invalidate_long(addr);
}
static void FASTCALL ext_sdram_write_word( sh4addr_t addr, uint32_t val )
{
    *(uint16_t *)(sh4_main_ram + (addr&0x00FFFFFF)) = (uint16_t)val;
    xlat_invalidate_word(addr);
}
static void FASTCALL ext_sdram_write_byte( sh4addr_t addr, uint32_t val )
{
    *(uint8_t *)(sh4_main_ram + (addr&0x00FFFFFF)) = (uint8_t)val;
    xlat_invalidate_word(addr);
}
static void FASTCALL ext_sdram_read_burst( unsigned char *dest, sh4addr_t addr )
{
    memcpy( dest, sh4_main_ram+(addr&0x00FFFFFF), 32 );
}
static void FASTCALL ext_sdram_write_burst( sh4addr_t addr, unsigned char *src )
{
    memcpy( sh4_main_ram+(addr&0x00FFFFFF), src, 32 );
}

struct mem_region_fn mem_region_sdram = { ext_sdram_read_long, ext_sdram_write_long, 
        ext_sdram_read_word, ext_sdram_write_word, 
        ext_sdram_read_byte, ext_sdram_write_byte, 
        ext_sdram_read_burst, ext_sdram_write_burst }; 


/********************* The Boot ROM address space **********************/
extern sh4ptr_t dc_boot_rom;
extern sh4ptr_t dc_flash_ram;
extern sh4ptr_t dc_audio_ram;
static int32_t FASTCALL ext_bootrom_read_long( sh4addr_t addr )
{
    return *((int32_t *)(dc_boot_rom + (addr&0x001FFFFF)));
}
static int32_t FASTCALL ext_bootrom_read_word( sh4addr_t addr )
{
    return SIGNEXT16(*((int16_t *)(dc_boot_rom + (addr&0x001FFFFF))));
}
static int32_t FASTCALL ext_bootrom_read_byte( sh4addr_t addr )
{
    return SIGNEXT8(*((int16_t *)(dc_boot_rom + (addr&0x001FFFFF))));
}
static void FASTCALL ext_bootrom_read_burst( unsigned char *dest, sh4addr_t addr )
{
    memcpy( dest, sh4_main_ram+(addr&0x001FFFFF), 32 );
}

struct mem_region_fn mem_region_bootrom = { 
        ext_bootrom_read_long, unmapped_write_long, 
        ext_bootrom_read_word, unmapped_write_long, 
        ext_bootrom_read_byte, unmapped_write_long, 
        ext_bootrom_read_burst, unmapped_write_burst }; 

/********************* The Audio RAM address space **********************/
static int32_t FASTCALL ext_audioram_read_long( sh4addr_t addr )
{
    return *((int32_t *)(dc_audio_ram + (addr&0x001FFFFF)));
}
static int32_t FASTCALL ext_audioram_read_word( sh4addr_t addr )
{
    return SIGNEXT16(*((int16_t *)(dc_audio_ram + (addr&0x001FFFFF))));
}
static int32_t FASTCALL ext_audioram_read_byte( sh4addr_t addr )
{
    return SIGNEXT8(*((int16_t *)(dc_audio_ram + (addr&0x001FFFFF))));
}
static void FASTCALL ext_audioram_write_long( sh4addr_t addr, uint32_t val )
{
    *(uint32_t *)(dc_audio_ram + (addr&0x001FFFFF)) = val;
    asic_g2_write_word();
}
static void FASTCALL ext_audioram_write_word( sh4addr_t addr, uint32_t val )
{
    *(uint16_t *)(dc_audio_ram + (addr&0x001FFFFF)) = (uint16_t)val;
    asic_g2_write_word();
}
static void FASTCALL ext_audioram_write_byte( sh4addr_t addr, uint32_t val )
{
    *(uint8_t *)(dc_audio_ram + (addr&0x001FFFFF)) = (uint8_t)val;
    asic_g2_write_word();
}
static void FASTCALL ext_audioram_read_burst( unsigned char *dest, sh4addr_t addr )
{
    memcpy( dest, dc_audio_ram+(addr&0x001FFFFF), 32 );
}
static void FASTCALL ext_audioram_write_burst( sh4addr_t addr, unsigned char *src )
{
    memcpy( dc_audio_ram+(addr&0x001FFFFF), src, 32 );
}

struct mem_region_fn mem_region_audioram = { ext_audioram_read_long, ext_audioram_write_long, 
        ext_audioram_read_word, ext_audioram_write_word, 
        ext_audioram_read_byte, ext_audioram_write_byte, 
        ext_audioram_read_burst, ext_audioram_write_burst }; 

/********************* The Flash RAM address space **********************/
static int32_t FASTCALL ext_flashram_read_long( sh4addr_t addr )
{
    return *((int32_t *)(dc_flash_ram + (addr&0x0001FFFF)));
}
static int32_t FASTCALL ext_flashram_read_word( sh4addr_t addr )
{
    return SIGNEXT16(*((int16_t *)(dc_flash_ram + (addr&0x0001FFFF))));
}
static int32_t FASTCALL ext_flashram_read_byte( sh4addr_t addr )
{
    return SIGNEXT8(*((int16_t *)(dc_flash_ram + (addr&0x0001FFFF))));
}
static void FASTCALL ext_flashram_write_long( sh4addr_t addr, uint32_t val )
{
    *(uint32_t *)(dc_flash_ram + (addr&0x0001FFFF)) = val;
    asic_g2_write_word();
}
static void FASTCALL ext_flashram_write_word( sh4addr_t addr, uint32_t val )
{
    *(uint16_t *)(dc_flash_ram + (addr&0x0001FFFF)) = (uint16_t)val;
    asic_g2_write_word();
}
static void FASTCALL ext_flashram_write_byte( sh4addr_t addr, uint32_t val )
{
    *(uint8_t *)(dc_flash_ram + (addr&0x0001FFFF)) = (uint8_t)val;
    asic_g2_write_word();
}
static void FASTCALL ext_flashram_read_burst( unsigned char *dest, sh4addr_t addr )
{
    memcpy( dest, dc_flash_ram+(addr&0x0001FFFF), 32 );
}
static void FASTCALL ext_flashram_write_burst( sh4addr_t addr, unsigned char *src )
{
    memcpy( dc_flash_ram+(addr&0x0001FFFF), src, 32 );
}

struct mem_region_fn mem_region_flashram = { ext_flashram_read_long, ext_flashram_write_long, 
        ext_flashram_read_word, ext_flashram_write_word, 
        ext_flashram_read_byte, ext_flashram_write_byte, 
        ext_flashram_read_burst, ext_flashram_write_burst }; 

/**************************************************************************/

struct mem_region_fn p4_region_icache_addr = {
        mmu_icache_addr_read, mmu_icache_addr_write,
        mmu_icache_addr_read, mmu_icache_addr_write,
        mmu_icache_addr_read, mmu_icache_addr_write,
        unmapped_read_burst, unmapped_write_burst };
struct mem_region_fn p4_region_icache_data = {
        mmu_icache_data_read, mmu_icache_data_write,
        mmu_icache_data_read, mmu_icache_data_write,
        mmu_icache_data_read, mmu_icache_data_write,
        unmapped_read_burst, unmapped_write_burst };
struct mem_region_fn p4_region_ocache_addr = {
        mmu_ocache_addr_read, mmu_ocache_addr_write,
        mmu_ocache_addr_read, mmu_ocache_addr_write,
        mmu_ocache_addr_read, mmu_ocache_addr_write,
        unmapped_read_burst, unmapped_write_burst };
struct mem_region_fn p4_region_ocache_data = {
        mmu_ocache_data_read, mmu_ocache_data_write,
        mmu_ocache_data_read, mmu_ocache_data_write,
        mmu_ocache_data_read, mmu_ocache_data_write,
        unmapped_read_burst, unmapped_write_burst };
struct mem_region_fn p4_region_itlb_addr = {
        mmu_itlb_addr_read, mmu_itlb_addr_write,
        mmu_itlb_addr_read, mmu_itlb_addr_write,
        mmu_itlb_addr_read, mmu_itlb_addr_write,
        unmapped_read_burst, unmapped_write_burst };
struct mem_region_fn p4_region_itlb_data = {
        mmu_itlb_data_read, mmu_itlb_data_write,
        mmu_itlb_data_read, mmu_itlb_data_write,
        mmu_itlb_data_read, mmu_itlb_data_write,
        unmapped_read_burst, unmapped_write_burst };
struct mem_region_fn p4_region_utlb_addr = {
        mmu_utlb_addr_read, mmu_utlb_addr_write,
        mmu_utlb_addr_read, mmu_utlb_addr_write,
        mmu_utlb_addr_read, mmu_utlb_addr_write,
        unmapped_read_burst, unmapped_write_burst };
struct mem_region_fn p4_region_utlb_data = {
        mmu_utlb_data_read, mmu_utlb_data_write,
        mmu_utlb_data_read, mmu_utlb_data_write,
        mmu_utlb_data_read, mmu_utlb_data_write,
        unmapped_read_burst, unmapped_write_burst };

#ifdef HAVE_FRAME_ADDRESS
#define RETURN_VIA(exc) do{ *(((void **)__builtin_frame_address(0))+1) = exc; return; } while(0)
#else
#define RETURN_VIA(exc) return NULL
#endif

int decode_sdram = 0;
mem_region_fn_t FASTCALL mem_decode_address( sh4addr_t addr )
{
    sh4ptr_t page;
    switch( addr >> 26 ) { /* Area */
    case 0: /* Holly multiplexed */
        page = page_map[(addr&0x1FFFFFFF)>>12];
        if( ((uintptr_t)page) < MAX_IO_REGIONS ) {
            return &io_rgn[(uintptr_t)page]->fn;
        } else if( addr < 0x00200000 ) {
            return &mem_region_bootrom;
        } else if( addr < 0x00400000 ) {
            return &mem_region_flashram;
        } else if( addr >= 0x00800000 && addr < 0x00A00000 ) {
            return &mem_region_audioram;
        }
        break;
    case 1: /* VRAM */
        switch( addr >> 24 ) {
        case 4: return &mem_region_vram64; /* 0x04xxxxxx */
        case 5: return &mem_region_vram32; /* 0x05xxxxxx */ 
        }    
        break;
    case 2: /* Unmapped */
        break;
    case 3: /* Main sdram */
        decode_sdram++;
        return &mem_region_sdram;
    case 4: /* Holly burst-access only? */
        break;
    }
    return &mem_region_unmapped;
}

int decode_count = 0;
int p4_count = 0;
int sq_count = 0;

mem_region_fn_t FASTCALL sh7750_decode_address( sh4addr_t addr )
{
    decode_count++;
    sh4addr_t region = addr >> 29;
    switch( region ) {
    case 7: /* P4 E0000000-FFFFFFFF - On-chip I/O */
        if( addr < 0xE4000000 ) {
            sq_count++;
            return &p4_region_storequeue;
        } else if( addr < 0xFC000000 ) { /* Control register region */
            p4_count++;
            switch( addr & 0x1F000000 ) {
            case 0x10000000: return &p4_region_icache_addr;
            case 0x11000000: return &p4_region_icache_data;
            case 0x12000000: return &p4_region_itlb_addr;
            case 0x13000000: return &p4_region_itlb_data;
            case 0x14000000: return &p4_region_ocache_addr;
            case 0x15000000: return &p4_region_ocache_data;
            case 0x16000000: return &p4_region_utlb_addr;
            case 0x17000000: return &p4_region_utlb_data;
            default: return &mem_region_unmapped;
            }
        } else {
            p4_count++;
            struct mmio_region *io = P4_io[(addr&0x1FFFFFFF)>>19];
            if( io != NULL ) {
                return &io->fn;
            }
            return &mem_region_unmapped;
        }
        break;
    case 6: /* P3 C0000000-DFFFFFFF - TLB on, Cache on */
    case 5: /* P2 A0000000-BFFFFFFF - TLB off, Cache off*/
    case 4: /* P1 80000000-9FFFFFFF - TLB off, Cache on */
    default: /* P0/U0 00000000-7FFFFFFF - TLB on, Cache on */
        return mem_decode_address( addr & 0x1FFFFFFF ); 
        /* TODO: Integrate TLB, Operand Cache lookups */
    }
}

void FASTCALL sh7750_decode_address_copy( sh4addr_t addr, mem_region_fn_t result )
{
    mem_region_fn_t region = sh7750_decode_address( addr );
    memcpy( result, region, sizeof(struct mem_region_fn) -8 );
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

static uint32_t last_page = -1;
static mem_region_fn_t last_region = NULL; 
static uint32_t hit_count = 0;
static uint32_t miss_count = 0;
static uint32_t rl_count = 0, rw_count = 0, rb_count = 0;
static uint32_t wl_count = 0, ww_count = 0, wb_count = 0;

/************** Compatibility methods ***************/

int32_t FASTCALL sh4_read_long( sh4addr_t addr )
{
    rl_count++;
    uint32_t page = (addr & 0xFFFFF000);
    if( page == last_page ) {
        hit_count++;
        return last_region->read_long(addr);
    } else {
        miss_count++;
        last_page = page;
        last_region = sh7750_decode_address(addr);
        return last_region->read_long(addr);
    }
}

int32_t FASTCALL sh4_read_word( sh4addr_t addr )
{
    rw_count++;
    uint32_t page = (addr & 0xFFFFF000);
    if( page == last_page ) {
        hit_count++;
        return last_region->read_word(addr);
    } else {
        miss_count++;
        last_page = page;
        last_region = sh7750_decode_address(addr);
        return last_region->read_word(addr);
    }
}

int32_t FASTCALL sh4_read_byte( sh4addr_t addr )
{
    rb_count++;
    uint32_t page = (addr & 0xFFFFF000);
    if( page == last_page ) {
        hit_count++;
        return last_region->read_byte(addr);
    } else {
        miss_count++;
        last_page = page;
        last_region = sh7750_decode_address(addr);
        return last_region->read_byte(addr);
    }    
}

void FASTCALL sh4_write_long( sh4addr_t addr, uint32_t val )
{
    wl_count++;
    uint32_t page = (addr & 0xFFFFF000);
    if( page == last_page ) {
        hit_count++;
        last_region->write_long(addr, val);
    } else {
        miss_count++;
        last_page = page;
        last_region = sh7750_decode_address(addr);
        last_region->write_long(addr,val);
    }
}

void FASTCALL sh4_write_word( sh4addr_t addr, uint32_t val )
{
    ww_count++;
    uint32_t page = (addr & 0xFFFFF000);
    if( page == last_page ) {
        hit_count++;
        last_region->write_word(addr, val);
    } else {
        miss_count++;
        last_page = page;
        last_region = sh7750_decode_address(addr);
        last_region->write_word(addr,val);
    }
}

void FASTCALL sh4_write_byte( sh4addr_t addr, uint32_t val )
{
    wb_count++;
    uint32_t page = (addr & 0xFFFFF000);
    if( page == last_page ) {
        hit_count++;
        last_region->write_byte(addr, val);
    } else {
        miss_count++;
        last_page = page;
        last_region = sh7750_decode_address(addr);
        last_region->write_byte(addr,val);
    }
}

void print_sh4mem_stats() {
    printf( "Decodes to p4: %d sq: %d\n", p4_count+sq_count, sq_count );
    printf( "Decodes to sdram: %d\n", decode_sdram );
    printf( "Decodes: %d Hits: %d Miss: %d\n", decode_count, hit_count, miss_count );
    printf( "Read long:  %d word: %d byte: %d\n", rl_count, rw_count, rb_count );
    printf( "Write long: %d word: %d byte: %d\n", wl_count, ww_count, wb_count );
}