/**
 * $Id: aica.c,v 1.8 2005-12-26 11:47:15 nkeynes Exp $
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

#define MODULE aica_module

#include "dream.h"
#include "mem.h"
#include "aica.h"
#define MMIO_IMPL
#include "aica.h"

MMIO_REGION_READ_DEFFN( AICA0 )
MMIO_REGION_READ_DEFFN( AICA1 )
MMIO_REGION_READ_DEFFN( AICA2 )

void aica_init( void );
void aica_reset( void );
void aica_start( void );
void aica_stop( void );
void aica_save_state( FILE *f );
int aica_load_state( FILE *f );
uint32_t aica_run_slice( uint32_t );


struct dreamcast_module aica_module = { "AICA", aica_init, aica_reset, 
					aica_start, aica_run_slice, aica_stop,
					aica_save_state, aica_load_state };

/**
 * Initialize the AICA subsystem. Note requires that 
 */
void aica_init( void )
{
    register_io_regions( mmio_list_spu );
    MMIO_NOTRACE(AICA0);
    MMIO_NOTRACE(AICA1);
    arm_mem_init();
    arm_reset();
}

void aica_reset( void )
{
    arm_reset();
}

void aica_start( void )
{

}

uint32_t aica_run_slice( uint32_t nanosecs )
{
    /* Run arm instructions */
    int reset = MMIO_READ( AICA2, AICA_RESET );
    if( reset & 1 == 0 ) { 
	/* Running */
        nanosecs = arm_run_slice( nanosecs );
    }
    /* Generate audio buffer */
    return nanosecs;
}

void aica_stop( void )
{

}

void aica_save_state( FILE *f )
{
    arm_save_state( f );
}

int aica_load_state( FILE *f )
{
    return arm_load_state( f );
}

/** Channel register structure:
 * 00  4  Channel config
 * 04  4  Waveform address lo (16 bits)
 * 08  4  Loop start address
 * 0C  4  Loop end address
 * 10  4  Volume envelope
 * 14  4  Init to 0x1F
 * 18  4  Frequency (floating point)
 * 1C  4  ?? 
 * 20  4  ??
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
    MMIO_WRITE( AICA0, reg, val );
    //    DEBUG( "AICA0 Write %08X => %08X", val, reg );
}

/* Write to channels 32-64 */
void mmio_region_AICA1_write( uint32_t reg, uint32_t val )
{
    //    aica_write_channel( (reg >> 7) + 32, reg % 128, val );
    MMIO_WRITE( AICA1, reg, val );
    // DEBUG( "AICA1 Write %08X => %08X", val, reg );
}

/* General registers */
void mmio_region_AICA2_write( uint32_t reg, uint32_t val )
{
    uint32_t tmp;
    switch( reg ) {
    case AICA_RESET:
	tmp = MMIO_READ( AICA2, AICA_RESET );
	if( (tmp & 1) == 1 && (val & 1) == 0 ) {
	    /* ARM enabled - execute a core reset */
	    DEBUG( "ARM enabled" );
	    arm_reset();
	} else if( (tmp&1) == 0 && (val&1) == 1 ) {
	    DEBUG( "ARM disabled" );
	}
	MMIO_WRITE( AICA2, AICA_RESET, val );
	break;
    default:
	MMIO_WRITE( AICA2, reg, val );
	break;
    }
}
