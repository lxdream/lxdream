/**
 * $Id$
 * 
 * Miscellaneous and not-really-implemented SH4 peripheral modules. Also
 * responsible for including the IMPL side of the SH4 MMIO pages.
 * Most of these will eventually be split off into their own files.
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

#include "dream.h"
#include "dreamcast.h"
#include "mem.h"
#include "clock.h"
#include "sh4/sh4core.h"
#include "sh4/sh4mmio.h"
#define MMIO_IMPL
#include "sh4/sh4mmio.h"

/********************************* BSC *************************************/

uint32_t bsc_input = 0x0300;

uint16_t bsc_read_pdtra()
{
    int i;
    uint32_t pctra = MMIO_READ( BSC, PCTRA );
    uint16_t output = MMIO_READ( BSC, PDTRA );
    uint16_t input_mask = 0, output_mask = 0;
    for( i=0; i<16; i++ ) {
        int bits = (pctra >> (i<<1)) & 0x03;
        if( bits == 2 ) input_mask |= (1<<i);
        else if( bits != 0 ) output_mask |= (1<<i);
    }

    /* ??? */
    if( ((output | (~output_mask)) & 0x03) == 3 ) {
        output |= 0x03;
    } else {
        output &= ~0x03;
    }

    return (bsc_input & input_mask) | output;
}

uint32_t bsc_read_pdtrb()
{
    int i;
    uint32_t pctrb = MMIO_READ( BSC, PCTRB );
    uint16_t output = MMIO_READ( BSC, PDTRB );
    uint16_t input_mask = 0, output_mask = 0;
    for( i=0; i<4; i++ ) {
        int bits = (pctrb >> (i<<1)) & 0x03;
        if( bits == 2 ) input_mask |= (1<<i);
        else if( bits != 0 ) output_mask |= (1<<i);
    }

    return ((bsc_input>>16) & input_mask) | output;

}

MMIO_REGION_WRITE_DEFFN(BSC)

MMIO_REGION_READ_FN( BSC, reg )
{
    int32_t val;
    reg &= 0xFFF;
    switch( reg ) {
    case PDTRA:
        val = bsc_read_pdtra();
        break;
    case PDTRB:
        val = bsc_read_pdtrb();
        break;
    default:
        val = MMIO_READ( BSC, reg );
    }
    return val;
}

MMIO_REGION_READ_DEFSUBFNS(BSC)

/********************************* UBC *************************************/

MMIO_REGION_READ_FN( UBC, reg )
{
    return MMIO_READ( UBC, reg & 0xFFF );
}

MMIO_REGION_WRITE_FN( UBC, reg, val )
{
    reg &= 0xFFF;
    switch( reg ) {
    case BAMRA:
    case BAMRB:
        val &= 0x0F;
        break;
    case BBRA:
    case BBRB:
        val &= 0x07F;
        if( val != 0 ) { 
            WARN( "UBC not implemented" );
        }
        break;
    case BRCR:
        val &= 0xC4C9;
        break;
    }
    MMIO_WRITE( UBC, reg, val );
}

MMIO_REGION_READ_DEFSUBFNS(UBC)

/********************************** SCI *************************************/

MMIO_REGION_STUBFNS( SCI )

MMIO_REGION_READ_DEFSUBFNS(SCI)
