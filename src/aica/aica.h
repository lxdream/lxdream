/**
 * $Id: aica.h,v 1.10 2007-01-17 09:37:22 nkeynes Exp $
 * 
 * MMIO definitions for the AICA sound chip. Note that the regions defined
 * here are relative to the SH4 memory map (0x00700000 based), rather than
 * the ARM addresses (0x00800000 based).
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

#include "mmio.h"

MMIO_REGION_BEGIN( 0x00700000, AICA0, "AICA Sound System 0-31" )
LONG_PORT( 0x000, AICACH0, PORT_MRW, UNDEFINED, "Channel 0" )
MMIO_REGION_END

MMIO_REGION_BEGIN( 0x00701000, AICA1, "AICA Sound System 32-63" )
LONG_PORT( 0x000, AICACH32, PORT_MRW, UNDEFINED, "Channel 32" )
MMIO_REGION_END

MMIO_REGION_BEGIN( 0x00702000, AICA2, "AICA Sound System Control" )
LONG_PORT( 0x040, CDDA_VOL_L, PORT_MRW, 0, "CDDA Volume left" )
LONG_PORT( 0x044, CDDA_VOL_R, PORT_MRW, 0, "CDDA Volume right" )
LONG_PORT( 0x800, VOL_MASTER, PORT_MRW, UNDEFINED, "Master volume" )
LONG_PORT( 0x808, AICA_UNK7, PORT_MRW, 0, "AICA ??? 7" )
LONG_PORT( 0x880, AICA_UNK6, PORT_MRW, 0, "AICA ??? 6" )
LONG_PORT( 0x890, AICA_TIMER, PORT_MRW, 0, "AICA Timer" )
LONG_PORT( 0x89C, AICA_UNK1, PORT_MRW, 0, "AICA ??? 1" )
LONG_PORT( 0x8A4, AICA_TCR, PORT_MRW, 0, "AICA Timer Control?" )
BYTE_PORT( 0x8A8, AICA_UNK3, PORT_MRW, 0, "AICA ??? 3" )
BYTE_PORT( 0x8AC, AICA_UNK4, PORT_MRW, 0, "AICA ??? 4" )
BYTE_PORT( 0x8B0, AICA_UNK5, PORT_MRW, 0, "AICA ??? 5" )
LONG_PORT( 0xC00, AICA_RESET,PORT_MRW, 1, "AICA reset" )
LONG_PORT( 0xD00, AICA_IRQ, PORT_MR, 0, "AICA IRQ Pending" )
LONG_PORT( 0xD04, AICA_IRQCLEAR, PORT_MRW, 0, "AICA IRQ Clear" )
MMIO_REGION_END

MMIO_REGION_BEGIN( 0x00710000, AICARTC, "AICA Sound System RTC" )
     LONG_PORT( 0x000, AICA_RTCHI, PORT_R, 0, "RTC High 16-bits" )
     LONG_PORT( 0x004, AICA_RTCLO, PORT_R, 0, "RTC Low 16-bits" )
     LONG_PORT( 0x008, AICA_RTCEN, PORT_W, 0, "RTC write enable" )
MMIO_REGION_END

MMIO_REGION_LIST_BEGIN( spu )
    MMIO_REGION( AICA0 )
    MMIO_REGION( AICA1 )
    MMIO_REGION( AICA2 )
    MMIO_REGION( AICARTC )
MMIO_REGION_LIST_END

void aica_init( void );
void aica_reset( void );
void aica_enable( void );

#define AICA_EVENT_TIMER 2
#define AICA_EVENT_OTHER 5

void aica_event( int event );
void aica_write_channel( int channel, uint32_t addr, uint32_t val );

/**
 * The AICA core runs at 44100 samples/second, regardless of what we're
 * actually outputing.
 */
#define AICA_SAMPLE_RATE 44100

/**
 * This is only used to determine number of instructions to execute
 * per sample, which isn't cycle accurate at the moment.
 */
#define AICA_SAMPLE_PERIOD (1000000000 / 44100)
