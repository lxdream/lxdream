/**
 * $Id$
 *
 * Dreamcast main SDRAM - access methods and timing controls. This is fairly
 * directly coupled to the SH4
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

#include "lxdream.h"
#include "mem.h"
#include "dreamcast.h"
#include "sh4/xltcache.h"
#include <string.h>


static int32_t FASTCALL ext_sdram_read_long( sh4addr_t addr )
{
    return *((int32_t *)(dc_main_ram + (addr&0x00FFFFFF)));
}
static int32_t FASTCALL ext_sdram_read_word( sh4addr_t addr )
{
    return SIGNEXT16(*((int16_t *)(dc_main_ram + (addr&0x00FFFFFF))));
}
static int32_t FASTCALL ext_sdram_read_byte( sh4addr_t addr )
{
    return SIGNEXT8(*((int16_t *)(dc_main_ram + (addr&0x00FFFFFF))));
}
static void FASTCALL ext_sdram_write_long( sh4addr_t addr, uint32_t val )
{
    *(uint32_t *)(dc_main_ram + (addr&0x00FFFFFF)) = val;
    xlat_invalidate_long(addr);
}
static void FASTCALL ext_sdram_write_word( sh4addr_t addr, uint32_t val )
{
    *(uint16_t *)(dc_main_ram + (addr&0x00FFFFFF)) = (uint16_t)val;
    xlat_invalidate_word(addr);
}
static void FASTCALL ext_sdram_write_byte( sh4addr_t addr, uint32_t val )
{
    *(uint8_t *)(dc_main_ram + (addr&0x00FFFFFF)) = (uint8_t)val;
    xlat_invalidate_word(addr);
}
static void FASTCALL ext_sdram_read_burst( unsigned char *dest, sh4addr_t addr )
{
    memcpy( dest, dc_main_ram+(addr&0x00FFFFFF), 32 );
}
static void FASTCALL ext_sdram_write_burst( sh4addr_t addr, unsigned char *src )
{
    memcpy( dc_main_ram+(addr&0x00FFFFFF), src, 32 );
}

struct mem_region_fn mem_region_sdram = { ext_sdram_read_long, ext_sdram_write_long, 
        ext_sdram_read_word, ext_sdram_write_word, 
        ext_sdram_read_byte, ext_sdram_write_byte, 
        ext_sdram_read_burst, ext_sdram_write_burst }; 
