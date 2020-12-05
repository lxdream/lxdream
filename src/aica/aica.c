/**
 * $Id$
 * 
 * This module implements the AICA's IO interfaces, as well
 * as providing the core AICA module to the system.
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

#include <time.h>
#include "dream.h"
#include "dreamcast.h"
#include "mem.h"
#include "aica/aica.h"
#include "armcore.h"
#include "aica/audio.h"
#define MMIO_IMPL
#include "aica.h"

MMIO_REGION_READ_DEFFN( AICA0 )
MMIO_REGION_READ_DEFFN( AICA1 )
MMIO_REGION_READ_DEFSUBFNS(AICA0)
MMIO_REGION_READ_DEFSUBFNS(AICA1)
MMIO_REGION_READ_DEFSUBFNS(AICA2)
MMIO_REGION_READ_DEFSUBFNS(AICARTC)

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

struct aica_state_struct {
    uint32_t time_of_day;
    /**
     * Keep track of what we've done so far this second, to try to keep the
     * precision of samples/second.
     */
    uint32_t samples_done;
    uint32_t nanosecs_done;
    /**
     * Event (IRQ) state
     */
    int event_pending;
    int clear_count;
};

static struct aica_state_struct aica_state;


/**
 * Initialize the AICA subsystem. Note requires that 
 */
void aica_init( void )
{
    register_io_regions( mmio_list_spu );
    MMIO_NOTRACE(AICA0);
    MMIO_NOTRACE(AICA1);
    aica_reset();
}

void aica_reset( void )
{
    arm_reset();
    aica_state.time_of_day = 0x5bfc8900;
    aica_state.samples_done = 0;
    aica_state.nanosecs_done = 0;
    aica_state.event_pending = 0;
    aica_state.clear_count = 0;
    //    aica_event(2); /* Pre-deliver a timer interrupt */
}

void aica_start( void )
{
    audio_start_driver();
}

uint32_t aica_run_slice( uint32_t nanosecs )
{
    /* Run arm instructions */
    int reset = MMIO_READ( AICA2, AICA_RESET );
    if( (reset & 1) == 0 ) { /* Running */
        int num_samples = (int)((uint64_t)AICA_SAMPLE_RATE * (aica_state.nanosecs_done + nanosecs) / 1000000000) - aica_state.samples_done;
        num_samples = arm_run_slice( num_samples );
        audio_mix_samples( num_samples );

        aica_state.samples_done += num_samples;
        aica_state.nanosecs_done += nanosecs;
    }
    if( aica_state.nanosecs_done > 1000000000 ) {
        aica_state.samples_done -= AICA_SAMPLE_RATE;
        aica_state.nanosecs_done -= 1000000000;
        aica_state.time_of_day++;
    }
    return nanosecs;
}

void aica_stop( void )
{
    audio_stop_driver();
}

void aica_save_state( FILE *f )
{
    fwrite( &aica_state, sizeof(struct aica_state_struct), 1, f );
    arm_save_state( f );
    audio_save_state(f);
}

int aica_load_state( FILE *f )
{
    fread( &aica_state, sizeof(struct aica_state_struct), 1, f );
    arm_load_state( f );
    return audio_load_state(f);
}

/* Note: This is probably not necessarily technically correct but it should
 * work in the meantime.
 */

void aica_event( int event )
{
    if( aica_state.event_pending == 0 )
        armr.int_pending |= CPSR_F;
    aica_state.event_pending |= (1<<event);

    int pending = MMIO_READ( AICA2, AICA_IRQ );
    if( pending == 0 || event < pending )
        MMIO_WRITE( AICA2, AICA_IRQ, event );
}

void aica_clear_event( )
{
    aica_state.clear_count++;
    if( aica_state.clear_count == 4 ) {
        int i;
        aica_state.clear_count = 0;

        for( i=0; i<8; i++ ) {
            if( aica_state.event_pending & (1<<i) ) {
                aica_state.event_pending &= ~(1<<i);
                break;
            }
        }
        for( ;i<8; i++ ) {
            if( aica_state.event_pending & (1<<i) ) {
                MMIO_WRITE( AICA2, AICA_IRQ, i );
                break;
            }
        }
        if( aica_state.event_pending == 0 )
            armr.int_pending &= ~CPSR_F;
    }
}

void aica_enable( void )
{
    mmio_region_AICA2_write( AICA_RESET, MMIO_READ(AICA2,AICA_RESET) & ~1 );
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
 */

/* Write to channels 0-31 */
MMIO_REGION_WRITE_FN( AICA0, reg, val )
{
    reg &= 0xFFF;
    MMIO_WRITE( AICA0, reg, val );
    aica_write_channel( reg >> 7, reg % 128, val );
    //    DEBUG( "AICA0 Write %08X => %08X", val, reg );
}

/* Write to channels 32-64 */
MMIO_REGION_WRITE_FN( AICA1, reg, val )
{
    reg &= 0xFFF;
    MMIO_WRITE( AICA1, reg, val );
    aica_write_channel( (reg >> 7) + 32, reg % 128, val );
    // DEBUG( "AICA1 Write %08X => %08X", val, reg );
}

/**
 * AICA control registers 
 */
MMIO_REGION_WRITE_FN( AICA2, reg, val )
{
    uint32_t tmp;
    reg &= 0xFFF;
    
    switch( reg ) {
    case AICA_RESET:
        tmp = MMIO_READ( AICA2, AICA_RESET );
        if( (tmp & 1) == 1 && (val & 1) == 0 ) {
            /* ARM enabled - execute a core reset */
            DEBUG( "ARM enabled" );
            arm_reset();
            aica_state.samples_done = 0;
            aica_state.nanosecs_done = 0;
        } else if( (tmp&1) == 0 && (val&1) == 1 ) {
            DEBUG( "ARM disabled" );
        }
        MMIO_WRITE( AICA2, AICA_RESET, val );
        break;
    case AICA_IRQCLEAR:
        aica_clear_event();
        break;
    case AICA_FIFOIN: /* Read-only */
        break;
    default:
        MMIO_WRITE( AICA2, reg, val );
        break;
    }
}

MMIO_REGION_READ_FN( AICA2, reg )
{
    audio_channel_t channel;
    uint32_t channo;
    int32_t val;
    reg &= 0xFFF;
    switch( reg ) {
    case AICA_CHANSTATE:
        channo = (MMIO_READ( AICA2, AICA_CHANSEL ) >> 8) & 0x3F;
        channel = audio_get_channel(channo);
        if( channel->loop == LOOP_LOOPED ) {
            val = 0x8000;
            channel->loop = LOOP_ON;
        } else {
            val = 0;
        }
        return val;
    case AICA_CHANPOSN:
        channo = (MMIO_READ( AICA2, AICA_CHANSEL ) >> 8) & 0x3F;
        channel = audio_get_channel(channo);
        return channel->posn;
    default:
        return MMIO_READ( AICA2, reg );
    }
}

MMIO_REGION_READ_FN( AICARTC, reg )
{
    int32_t rv = 0;
    reg &= 0xFFF;
    switch( reg ) {
    case AICA_RTCHI:
        rv = (aica_state.time_of_day >> 16) & 0xFFFF;
        break;
    case AICA_RTCLO:
        rv = aica_state.time_of_day & 0xFFFF;
        break;
    }
    // DEBUG( "Read AICA RTC %d => %08X", reg, rv );
    return rv;
}

MMIO_REGION_WRITE_FN( AICARTC, reg, val )
{
    reg &= 0xFFF;
    switch( reg ) {
    case AICA_RTCEN:
        MMIO_WRITE( AICARTC, reg, val&0x01 );
        break;
    case AICA_RTCLO:
        if( MMIO_READ( AICARTC, AICA_RTCEN ) & 0x01 ) {
            aica_state.time_of_day = (aica_state.time_of_day & 0xFFFF0000) | (val & 0xFFFF);
        }
        break;
    case AICA_RTCHI:
        if( MMIO_READ( AICARTC, AICA_RTCEN ) & 0x01 ) {
            aica_state.time_of_day = (aica_state.time_of_day & 0xFFFF) | (val<<16);
            MMIO_WRITE( AICARTC, AICA_RTCEN, 0 );
        }
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
    uint32_t rate;
    if( freq & 0x4000 ) {
        /* neg exponent - rate < 44100 */
        exponent = 8 - exponent;
        rate = (44100 >> exponent) +
        ((44100 * mantissa) >> (10+exponent));
    } else {
        /* pos exponent - rate > 44100 */
        rate = (44100 << exponent) +
        ((44100 * mantissa) >> (10-exponent));
    }
    return rate;
}

void aica_start_stop_channels()
{
    int i;
    for( i=0; i<32; i++ ) {
        uint32_t val = MMIO_READ( AICA0, i<<7 );
        audio_start_stop_channel(i, val&0x4000);
    }
    for( ; i<64; i++ ) {
        uint32_t val = MMIO_READ( AICA1, (i-32)<<7 );
        audio_start_stop_channel(i, val&0x4000);
    }
}

/**
 * Derived directly from Dan Potter's log table
 */
uint8_t aica_volume_table[256] = {
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
        1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   3,   3,   3,   3,   4,
        4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   7,   7,   7,   8,   8,   9,
        9,   9,  10,  10,  11,  11,  11,  12,  12,  13,  13,  14,  14,  15,  15,  16,
        16,  17,  17,  18,  18,  19,  19,  20,  20,  21,  22,  22,  23,  23,  24,  25,
        25,  26,  27,  27,  28,  29,  29,  30,  31,  31,  32,  33,  34,  34,  35,  36,
        37,  37,  38,  39,  40,  40,  41,  42,  43,  44,  45,  45,  46,  47,  48,  49,
        50,  51,  52,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
        65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  76,  77,  78,  79,  80,  81,
        82,  83,  85,  86,  87,  88,  89,  90,  92,  93,  94,  95,  97,  98,  99, 100,
        102, 103, 104, 105, 107, 108, 109, 111, 112, 113, 115, 116, 117, 119, 120, 121,
        123, 124, 126, 127, 128, 130, 131, 133, 134, 136, 137, 139, 140, 142, 143, 145,
        146, 148, 149, 151, 152, 154, 155, 157, 159, 160, 162, 163, 165, 167, 168, 170,
        171, 173, 175, 176, 178, 180, 181, 183, 185, 187, 188, 190, 192, 194, 195, 197,
        199, 201, 202, 204, 206, 208, 210, 211, 213, 215, 217, 219, 221, 223, 224, 226,
        228, 230, 232, 234, 236, 238, 240, 242, 244, 246, 248, 250, 252, 253, 254, 255 };


void aica_write_channel( int channelNo, uint32_t reg, uint32_t val ) 
{
    val &= 0x0000FFFF;
    audio_channel_t channel = audio_get_channel(channelNo);
    switch( reg ) {
    case 0x00: /* Config + high address bits*/
        channel->start = (channel->start & 0xFFFF) | ((val&0x1F) << 16);
        if( val & 0x200 ) 
            channel->loop = LOOP_ON;
        else 
            channel->loop = LOOP_OFF;
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
        if( val & 0x8000 ) {
            aica_start_stop_channels();
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
            break;
        case 0x20: /* ??? */
            break;
        case 0x24: { /* Volume / pan */
            int pan = (val & 0x1F);
            if( pan <= 0x0F )
                pan = 0x0F - pan; /* Convert to smooth pan over 0..31 */
            channel->pan = 31 - pan;
        }   break;
        case 0x28: /* Volume */
            // This isn't remotely correct, but it will have to suffice until I have
            // time to figure out what's actually going on here... 
            channel->vol = aica_volume_table[max((val & 0xFF),((val>>8)&0xFF))];
            break;
        default: /* ??? */
            break;
    }

}
