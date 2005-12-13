/**
 * $Id: aica.c,v 1.3 2005-12-13 12:17:26 nkeynes Exp $
 * 
 * This is the core sound system (ie the bit which does the actual work)
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

#include "dream.h"
#include "modules.h"
#include "mem.h"
#include "aica.h"
#define MMIO_IMPL
#include "aica.h"

MMIO_REGION_READ_DEFFN( AICA0 )
MMIO_REGION_READ_DEFFN( AICA1 )
MMIO_REGION_READ_DEFFN( AICA2 )

struct dreamcast_module aica_module = { "AICA", aica_init, aica_reset, NULL, NULL,
					NULL, NULL };

/**
 * Initialize the AICA subsystem. Note requires that 
 */
void aica_init( void )
{
    register_io_regions( mmio_list_spu );
    MMIO_NOTRACE(AICA0);
    MMIO_NOTRACE(AICA1);
    arm_mem_init();
}

void aica_reset( void )
{

}

/** Channel register structure:
 * 00
 * 04
 * 08  4  Loop start address
 * 0C  4  Loop end address
 * 10  4  Volume envelope
 * 14
 * 18  4  Frequency (floating point 
 * 1C
 * 20
 * 24  1  Pan
 * 25  1  ??
 * 26  
 * 27  
 * 28  1  ??
 * 29  1  Volume
 * 2C
 * 30
 * 

/* Write to channels 0-31 */
void mmio_region_AICA0_write( uint32_t reg, uint32_t val )
{
    //    aica_write_channel( reg >> 7, reg % 128, val );

}

/* Write to channels 32-64 */
void mmio_region_AICA1_write( uint32_t reg, uint32_t val )
{
    //    aica_write_channel( (reg >> 7) + 32, reg % 128, val );

}

/* General registers */
void mmio_region_AICA2_write( uint32_t reg, uint32_t val )
{

}
