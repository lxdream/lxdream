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

/***************************** P4 Regions ************************************/

/* Store-queue (long-write only?) */
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

/* Cache access */
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

/* TLB access */
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

/********************** Initialization *************************/

mem_region_fn_t *sh4_address_space;

static void sh4_register_mem_region( uint32_t start, uint32_t end, mem_region_fn_t fn )
{
    int count = (end - start) >> 12;
    mem_region_fn_t *ptr = &sh4_address_space[start>>12];
    while( count-- > 0 ) {
        *ptr++ = fn;
    }
}

static gboolean sh4_ext_page_remapped( sh4addr_t page, mem_region_fn_t fn, void *user_data )
{
    int i;
    for( i=0; i<= 0xC0000000; i+= 0x20000000 ) {
        sh4_address_space[(page|i)>>12] = fn;
    }
}


void sh4_mem_init()
{
    int i;
    mem_region_fn_t *ptr;
    sh4_address_space = mem_alloc_pages( sizeof(mem_region_fn_t) * 256 );
    for( i=0, ptr = sh4_address_space; i<7; i++, ptr += LXDREAM_PAGE_TABLE_ENTRIES ) {
        memcpy( ptr, ext_address_space, sizeof(mem_region_fn_t) * LXDREAM_PAGE_TABLE_ENTRIES );
    }
    
    /* Setup main P4 regions */
    sh4_register_mem_region( 0xE0000000, 0xE4000000, &p4_region_storequeue );
    sh4_register_mem_region( 0xE4000000, 0xF0000000, &mem_region_unmapped );
    sh4_register_mem_region( 0xF0000000, 0xF1000000, &p4_region_icache_addr );
    sh4_register_mem_region( 0xF1000000, 0xF2000000, &p4_region_icache_data );
    sh4_register_mem_region( 0xF2000000, 0xF3000000, &p4_region_itlb_addr );
    sh4_register_mem_region( 0xF3000000, 0xF4000000, &p4_region_itlb_data );
    sh4_register_mem_region( 0xF4000000, 0xF5000000, &p4_region_ocache_addr );
    sh4_register_mem_region( 0xF5000000, 0xF6000000, &p4_region_ocache_data );
    sh4_register_mem_region( 0xF6000000, 0xF7000000, &p4_region_utlb_addr );
    sh4_register_mem_region( 0xF7000000, 0xF8000000, &p4_region_utlb_data );
    sh4_register_mem_region( 0xF8000000, 0x00000000, &mem_region_unmapped );
    
    /* Setup P4 control region */
    sh4_register_mem_region( 0xFF000000, 0xFF001000, &mmio_region_MMU.fn );
    sh4_register_mem_region( 0xFF100000, 0xFF101000, &mmio_region_PMM.fn );
    sh4_register_mem_region( 0xFF200000, 0xFF201000, &mmio_region_UBC.fn );
    sh4_register_mem_region( 0xFF800000, 0xFF801000, &mmio_region_BSC.fn );
    sh4_register_mem_region( 0xFF900000, 0xFFA00000, &mem_region_unmapped ); // SDMR2 + SDMR3
    sh4_register_mem_region( 0xFFA00000, 0xFFA01000, &mmio_region_DMAC.fn );
    sh4_register_mem_region( 0xFFC00000, 0xFFC01000, &mmio_region_CPG.fn );
    sh4_register_mem_region( 0xFFC80000, 0xFFC81000, &mmio_region_RTC.fn );
    sh4_register_mem_region( 0xFFD00000, 0xFFD01000, &mmio_region_INTC.fn );
    sh4_register_mem_region( 0xFFD80000, 0xFFD81000, &mmio_region_TMU.fn );
    sh4_register_mem_region( 0xFFE00000, 0xFFE01000, &mmio_region_SCI.fn );
    sh4_register_mem_region( 0xFFE80000, 0xFFE81000, &mmio_region_SCIF.fn );
    sh4_register_mem_region( 0xFFF00000, 0xFFF01000, &mem_region_unmapped ); // H-UDI
    
    register_mem_page_remapped_hook( sh4_ext_page_remapped, NULL );
}
                           
/************** Access methods ***************/
#ifdef HAVE_FRAME_ADDRESS
#define RETURN_VIA(exc) do{ *(((void **)__builtin_frame_address(0))+1) = exc; return; } while(0)
#else
#define RETURN_VIA(exc) return NULL
#endif


int32_t FASTCALL sh4_read_long( sh4addr_t addr )
{
    return sh4_address_space[addr>>12]->read_long(addr);
}

int32_t FASTCALL sh4_read_word( sh4addr_t addr )
{
    return sh4_address_space[addr>>12]->read_word(addr);
}

int32_t FASTCALL sh4_read_byte( sh4addr_t addr )
{
    return sh4_address_space[addr>>12]->read_byte(addr);
}

void FASTCALL sh4_write_long( sh4addr_t addr, uint32_t val )
{
    sh4_address_space[addr>>12]->write_long(addr, val);
}

void FASTCALL sh4_write_word( sh4addr_t addr, uint32_t val )
{
    sh4_address_space[addr>>12]->write_word(addr,val);
}

void FASTCALL sh4_write_byte( sh4addr_t addr, uint32_t val )
{
    sh4_address_space[addr>>12]->write_byte(addr, val);
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
