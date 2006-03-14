/**
 * $Id: audio.c,v 1.5 2006-03-14 12:45:53 nkeynes Exp $
 * 
 * Audio mixer core. Combines all the active streams into a single sound
 * buffer for output. 
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

#include "aica/aica.h"
#include "aica/audio.h"
#include "glib/gmem.h"
#include "dream.h"
#include <assert.h>
#include <string.h>

#define NUM_BUFFERS 3
#define MS_PER_BUFFER 1000

#define BUFFER_EMPTY   0
#define BUFFER_WRITING 1
#define BUFFER_FULL    2

struct audio_state {
    audio_buffer_t output_buffers[NUM_BUFFERS];
    int write_buffer;
    int read_buffer;
    uint32_t output_format;
    uint32_t output_rate;
    uint32_t output_sample_size;
    struct audio_channel channels[64];
} audio;

audio_driver_t audio_driver = NULL;

#define NEXT_BUFFER() ((audio.write_buffer == NUM_BUFFERS-1) ? 0 : audio.write_buffer+1)

extern char *arm_mem;

/**
 * Set the output driver, sample rate and format. Also initializes the 
 * output buffers, flushing any current data and reallocating as 
 * necessary.
 */
void audio_set_driver( audio_driver_t driver, 
		       uint32_t samplerate, int format )
{
    uint32_t bytes_per_sample = 1;
    uint32_t samples_per_buffer;
    int i;

    if( format & AUDIO_FMT_16BIT )
	bytes_per_sample = 2;
    if( format & AUDIO_FMT_STEREO )
	bytes_per_sample <<= 1;
    if( samplerate == audio.output_rate &&
	bytes_per_sample == audio.output_sample_size )
	return;
    samples_per_buffer = (samplerate * MS_PER_BUFFER / 1000);
    for( i=0; i<NUM_BUFFERS; i++ ) {
	if( audio.output_buffers[i] != NULL )
	    free(audio.output_buffers[i]);
	audio.output_buffers[i] = g_malloc0( sizeof(struct audio_buffer) + samples_per_buffer * bytes_per_sample );
	audio.output_buffers[i]->length = samples_per_buffer * bytes_per_sample;
	audio.output_buffers[i]->posn = 0;
	audio.output_buffers[i]->status = BUFFER_EMPTY;
    }
    audio.output_format = format;
    audio.output_rate = samplerate;
    audio.output_sample_size = bytes_per_sample;
    audio.write_buffer = 0;
    audio.read_buffer = 0;

    if( driver == NULL )
	driver = &audio_null_driver;
    audio_driver = driver;
    audio_driver->set_output_format( samplerate, format );
}

/**
 * Mark the current write buffer as full and prepare the next buffer for
 * writing. Returns the next buffer to write to.
 * If all buffers are full, returns NULL.
 */
audio_buffer_t audio_next_write_buffer( )
{
    audio_buffer_t result = NULL;
    audio_buffer_t current = audio.output_buffers[audio.write_buffer];
    current->status = BUFFER_FULL;
    if( audio.read_buffer == audio.write_buffer &&
	audio_driver->process_buffer( current ) ) {
	audio_next_read_buffer();
    }
    audio.write_buffer = NEXT_BUFFER();
    result = audio.output_buffers[audio.write_buffer];
    if( result->status == BUFFER_FULL )
	return NULL;
    else {
	result->status = BUFFER_WRITING;
	return result;
    }
}

/**
 * Mark the current read buffer as empty and return the next buffer for
 * reading. If there is no next buffer yet, returns NULL.
 */
audio_buffer_t audio_next_read_buffer( )
{
    audio_buffer_t current = audio.output_buffers[audio.read_buffer];
    assert( current->status == BUFFER_FULL );
    current->status = BUFFER_EMPTY;
    current->posn = 0;
    audio.read_buffer++;
    if( audio.read_buffer == NUM_BUFFERS )
	audio.read_buffer = 0;
    
    current = audio.output_buffers[audio.read_buffer];
    if( current->status == BUFFER_FULL )
	return current;
    else return NULL;
}

/*************************** ADPCM ***********************************/

/**
 * The following section borrows heavily from ffmpeg, which is
 * copyright (c) 2001-2003 by the fine folks at the ffmpeg project,
 * distributed under the GPL version 2 or later.
 */

#define CLAMP_TO_SHORT(value) \
if (value > 32767) \
    value = 32767; \
else if (value < -32768) \
    value = -32768; \

static const int yamaha_indexscale[] = {
    230, 230, 230, 230, 307, 409, 512, 614,
    230, 230, 230, 230, 307, 409, 512, 614
};

static const int yamaha_difflookup[] = {
    1, 3, 5, 7, 9, 11, 13, 15,
    -1, -3, -5, -7, -9, -11, -13, -15
};

static inline short adpcm_yamaha_decode_nibble( audio_channel_t c, 
						unsigned char nibble )
{
    if( c->adpcm_step == 0 ) {
        c->adpcm_predict = 0;
        c->adpcm_step = 127;
    }

    c->adpcm_predict += (c->adpcm_step * yamaha_difflookup[nibble]) >> 3;
    CLAMP_TO_SHORT(c->adpcm_predict);
    c->adpcm_step = (c->adpcm_step * yamaha_indexscale[nibble]) >> 8;
    c->adpcm_step = CLAMP(c->adpcm_step, 127, 24567);
    return c->adpcm_predict;
}

/*************************** Sample mixer *****************************/

/**
 * Mix a single output sample.
 */
void audio_mix_samples( int num_samples )
{
    int i, j;
    int32_t result_buf[num_samples][2];

    memset( &result_buf, 0, sizeof(result_buf) );

    for( i=0; i < 64; i++ ) {
	audio_channel_t channel = &audio.channels[i];
	if( channel->active ) {
	    int32_t sample;
	    int vol_left = (channel->vol * (32 - channel->pan)) >> 5;
	    int vol_right = (channel->vol * (channel->pan + 1)) >> 5;
	    switch( channel->sample_format ) {
	    case AUDIO_FMT_16BIT:
		for( j=0; j<num_samples; j++ ) {
		    sample = *(int16_t *)(arm_mem + channel->posn + channel->start);
		    result_buf[j][0] += sample * vol_left;
		    result_buf[j][1] += sample * vol_right;
		    
		    channel->posn_left += channel->sample_rate;
		    while( channel->posn_left > audio.output_rate ) {
			channel->posn_left -= audio.output_rate;
			channel->posn++;
			
			if( channel->posn == channel->end ) {
			    if( channel->loop )
				channel->posn = channel->loop_start;
			    else {
				audio_stop_channel(i);
				j = num_samples;
				break;
			    }
			}
		    }
		}
		break;
	    case AUDIO_FMT_8BIT:
		for( j=0; j<num_samples; j++ ) {
		    sample = (*(int8_t *)(arm_mem + channel->posn + channel->start)) << 8;
		    result_buf[j][0] += sample * vol_left;
		    result_buf[j][1] += sample * vol_right;
		    
		    channel->posn_left += channel->sample_rate;
		    while( channel->posn_left > audio.output_rate ) {
			channel->posn_left -= audio.output_rate;
			channel->posn++;
			
			if( channel->posn == channel->end ) {
			    if( channel->loop )
				channel->posn = channel->loop_start;
			    else {
				audio_stop_channel(i);
				j = num_samples;
				break;
			    }
			}
		    }
		}
		break;
	    case AUDIO_FMT_ADPCM:
		for( j=0; j<num_samples; j++ ) {
		    sample = (int16_t)channel->adpcm_predict;
		    result_buf[j][0] += sample * vol_left;
		    result_buf[j][1] += sample * vol_right;
		    channel->posn_left += channel->sample_rate;
		    while( channel->posn_left > audio.output_rate ) {
			channel->posn_left -= audio.output_rate;
			if( channel->adpcm_nibble == 0 ) {
			    uint8_t data = *(uint8_t *)(arm_mem + channel->posn + channel->start);
			    adpcm_yamaha_decode_nibble( channel, (data >> 4) & 0x0F );
			    channel->adpcm_nibble = 1;
			} else {
			    channel->posn++;
			    if( channel->posn == channel->end ) {
				if( channel->loop )
				    channel->posn = channel->loop_start;
				else
				    audio_stop_channel(i);
				break;
			    }
			    uint8_t data = *(uint8_t *)(arm_mem + channel->posn + channel->start);
			    adpcm_yamaha_decode_nibble( channel, data & 0x0F );
			    channel->adpcm_nibble = 0;
			}
		    }
		}
		break;
	    default:
		break;
	    }
	}
    }
	    
    /* Down-render to the final output format */
    
    if( audio.output_format & AUDIO_FMT_16BIT ) {
	audio_buffer_t buf = audio.output_buffers[audio.write_buffer];
	uint16_t *data = (uint16_t *)&buf->data[buf->posn];
	for( j=0; j < num_samples; j++ ) {
	    *data++ = (int16_t)(result_buf[j][0] >> 6);
	    *data++ = (int16_t)(result_buf[j][1] >> 6);	
	    buf->posn += 4;
	    if( buf->posn == buf->length ) {
		audio_next_write_buffer();
		buf = audio.output_buffers[audio.write_buffer];
		data = (uint16_t *)&buf->data[0];
	    }
	}
    } else {
	audio_buffer_t buf = audio.output_buffers[audio.write_buffer];
	uint8_t *data = (uint8_t *)&buf->data[buf->posn];
	for( j=0; j < num_samples; j++ ) {
	    *data++ = (uint8_t)(result_buf[j][0] >> 16);
	    *data++ = (uint8_t)(result_buf[j][1] >> 16);	
	    buf->posn += 2;
	    if( buf->posn == buf->length ) {
		audio_next_write_buffer();
		buf = audio.output_buffers[audio.write_buffer];
		data = (uint8_t *)&buf->data[0];
	    }
	}
    }
}

/********************** Internal AICA calls ***************************/

audio_channel_t audio_get_channel( int channel ) 
{
    return &audio.channels[channel];
}

void audio_stop_channel( int channel ) 
{
    audio.channels[channel].active = FALSE;
}


void audio_start_channel( int channel )
{
    audio.channels[channel].posn = 0;
    audio.channels[channel].posn_left = 0;
    audio.channels[channel].adpcm_nibble = 0;
    audio.channels[channel].adpcm_step = 0;
    audio.channels[channel].adpcm_predict = 0;
    audio.channels[channel].active = TRUE;
}
