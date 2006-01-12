/**
 * $Id: aica.c,v 1.12 2006-01-12 11:30:19 nkeynes Exp $
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
#include "dreamcast.h"
#include "mem.h"
#include "aica.h"
#include "armcore.h"
#include "audio.h"
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
    aica_reset();
    audio_set_output( &esd_audio_driver, 44100, AUDIO_FMT_16BIT|AUDIO_FMT_STEREO );
}

void aica_reset( void )
{
    arm_reset();
    aica_event(2); /* Pre-deliver a timer interrupt */
}

void aica_start( void )
{

}

/**
 * Keep track of what we've done so far this second, to try to keep the
 * precision of samples/second.
 */
int samples_done = 0;
uint32_t nanosecs_done = 0;

uint32_t aica_run_slice( uint32_t nanosecs )
{
    /* Run arm instructions */
    int reset = MMIO_READ( AICA2, AICA_RESET );
    if( (reset & 1) == 0 ) { /* Running */
	int num_samples = (nanosecs_done + nanosecs) / AICA_SAMPLE_RATE - samples_done;
	num_samples = arm_run_slice( num_samples );
	audio_mix_samples( num_samples );

	samples_done += num_samples;
	nanosecs_done += nanosecs;
    }
    if( nanosecs_done > 1000000000 ) {
	samples_done -= AICA_SAMPLE_RATE;
	nanosecs_done -= 1000000000;
    }
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

int aica_event_pending = 0;
int aica_clear_count = 0;

/* Note: This is probably not necessarily technically correct but it should
 * work in the meantime.
 */

void aica_event( int event )
{
    if( aica_event_pending == 0 )
	armr.int_pending |= CPSR_F;
    aica_event_pending |= (1<<event);
    
    int pending = MMIO_READ( AICA2, AICA_IRQ );
    if( pending == 0 || event < pending )
	MMIO_WRITE( AICA2, AICA_IRQ, event );
}

void aica_clear_event( )
{
    aica_clear_count++;
    if( aica_clear_count == 4 ) {
	int i;
	aica_clear_count = 0;

	for( i=0; i<8; i++ ) {
	    if( aica_event_pending & (1<<i) ) {
		aica_event_pending &= ~(1<<i);
		break;
	    }
	}
	for( ;i<8; i++ ) {
	    if( aica_event_pending & (1<<i) ) {
		MMIO_WRITE( AICA2, AICA_IRQ, i );
		break;
	    }
	}
	if( aica_event_pending == 0 )
	    armr.int_pending &= ~CPSR_F;
    }
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
    MMIO_WRITE( AICA0, reg, val );
    aica_write_channel( reg >> 7, reg % 128, val );
    //    DEBUG( "AICA0 Write %08X => %08X", val, reg );
}

/* Write to channels 32-64 */
void mmio_region_AICA1_write( uint32_t reg, uint32_t val )
{
    MMIO_WRITE( AICA1, reg, val );
    aica_write_channel( (reg >> 7) + 32, reg % 128, val );
    // DEBUG( "AICA1 Write %08X => %08X", val, reg );
}

/**
 * AICA control registers 
 */
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
	    samples_done = 0;
	    nanosecs_done = 0;
	} else if( (tmp&1) == 0 && (val&1) == 1 ) {
	    DEBUG( "ARM disabled" );
	}
	MMIO_WRITE( AICA2, AICA_RESET, val );
	break;
    case AICA_IRQCLEAR:
	aica_clear_event();
	break;
    default:
	MMIO_WRITE( AICA2, reg, val );
	break;
    }
}

/**
 * Translate the channel frequency to a sample rate. The frequency is a
 * 14-bit floating point number, where bits 0..9 is the mantissa,
 * 11..14 is the signed exponent (-8 to +7). Bit 10 appears to
 * be unused.
 *
 * @return sample rate in samples per second.
 */
uint32_t aica_frequency_to_sample_rate( uint32_t freq )
{
    uint32_t exponent = (freq & 0x3800) >> 11;
    uint32_t mantissa = freq & 0x03FF;
    if( freq & 0x4000 ) {
	/* neg exponent - rate < 44100 */
	exponent = 8 - exponent;
	return (44100 >> exponent) +
	    ((44100 * mantissa) >> (10+exponent));
    } else {
	/* pos exponent - rate > 44100 */
	return (44100 << exponent) +
	    ((44100 * mantissa) >> (10-exponent));
    }
}

void aica_write_channel( int channelNo, uint32_t reg, uint32_t val ) 
{
    val &= 0x0000FFFF;
    audio_channel_t channel = audio_get_channel(channelNo);
    switch( reg ) {
    case 0x00: /* Config + high address bits*/
	channel->start = (channel->start & 0xFFFF) | ((val&0x1F) << 16);
	if( val & 0x200 ) 
	    channel->loop = TRUE;
	else 
	    channel->loop = FALSE;
	switch( (val >> 7) & 0x03 ) {
	case 0:
	    channel->sample_format = AUDIO_FMT_16BIT;
	    break;
	case 1:
	    channel->sample_format = AUDIO_FMT_8BIT;
	    break;
	case 2:
	case 3:
	    channel->sample_format = AUDIO_FMT_ADPCM;
	    break;
	}
	switch( (val >> 14) & 0x03 ) {
	case 2: 
	    audio_stop_channel( channelNo ); 
	    break;
	case 3: 
	    audio_start_channel( channelNo ); 
	    break;
	default:
	    break;
	    /* Hrmm... */
	}
	break;
    case 0x04: /* Low 16 address bits */
	channel->start = (channel->start & 0x001F0000) | val;
	break;
    case 0x08: /* Loop start */
	channel->loop_start = val;
	break;
    case 0x0C: /* End */
	channel->end = val;
	break;
    case 0x10: /* Envelope register 1 */
	break;
    case 0x14: /* Envelope register 2 */
	break;
    case 0x18: /* Frequency */
	channel->sample_rate = aica_frequency_to_sample_rate ( val );
	break;
    case 0x1C: /* ??? */
    case 0x20: /* ??? */
    case 0x24: /* Volume? /pan */
	break;
    case 0x28: /* Volume */
	channel->vol_left = channel->vol_right = val & 0xFF;
	break;
    default: /* ??? */
	break;
    }

}
