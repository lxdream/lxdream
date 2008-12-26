/**
 * $Id$
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
#include <glib/gmem.h>
#include "dream.h"
#include <assert.h>
#include <string.h>


extern struct audio_driver audio_null_driver;
extern struct audio_driver audio_osx_driver;
extern struct audio_driver audio_pulse_driver;
extern struct audio_driver audio_esd_driver;
extern struct audio_driver audio_alsa_driver;

audio_driver_t audio_driver_list[] = {
#ifdef HAVE_CORE_AUDIO
        &audio_osx_driver,
#endif
#ifdef HAVE_PULSE
        &audio_pulse_driver,
#endif
#ifdef HAVE_ESOUND
        &audio_esd_driver,
#endif
#ifdef HAVE_ALSA
        &audio_alsa_driver,
#endif
        &audio_null_driver,
        NULL };

#define NUM_BUFFERS 3
#define MS_PER_BUFFER 100

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
    struct audio_channel channels[AUDIO_CHANNEL_COUNT];
} audio;

audio_driver_t audio_driver = NULL;

#define NEXT_BUFFER() ((audio.write_buffer == NUM_BUFFERS-1) ? 0 : audio.write_buffer+1)

/**
 * Preserve audio channel state only - don't bother saving the buffers
 */
void audio_save_state( FILE *f )
{
    fwrite( &audio.channels[0], sizeof(struct audio_channel), AUDIO_CHANNEL_COUNT, f );
}

int audio_load_state( FILE *f )
{
    int read = fread( &audio.channels[0], sizeof(struct audio_channel), AUDIO_CHANNEL_COUNT, f );
    return (read == AUDIO_CHANNEL_COUNT ? 0 : -1 );
}

audio_driver_t get_audio_driver_by_name( const char *name )
{
    int i;
    if( name == NULL ) {
        return audio_driver_list[0];
    }
    for( i=0; audio_driver_list[i] != NULL; i++ ) {
        if( strcasecmp( audio_driver_list[i]->name, name ) == 0 ) {
            return audio_driver_list[i];
        }
    }

    return NULL;
}

void print_audio_drivers( FILE * out )
{
    int i;
    fprintf( out, "Available audio drivers:\n" );
    for( i=0; audio_driver_list[i] != NULL; i++ ) {
        fprintf( out, "  %-8s %s\n", audio_driver_list[i]->name,
                gettext(audio_driver_list[i]->description) );
    }
}

audio_driver_t audio_init_driver( const char *preferred_driver )
{
    audio_driver_t audio_driver = get_audio_driver_by_name(preferred_driver);
    if( audio_driver == NULL ) {
        ERROR( "Audio driver '%s' not found, aborting.", preferred_driver );
        exit(2);
    } else if( audio_set_driver( audio_driver ) == FALSE ) {
        int i;
        for( i=0; audio_driver_list[i] != NULL; i++ ) {
            if( audio_driver_list[i] != audio_driver &&
                audio_set_driver( audio_driver_list[i] ) ) {
                ERROR( "Failed to initialize audio driver %s, falling back to %s", 
                       audio_driver->name, audio_driver_list[i]->name );
                return audio_driver_list[i];
            }
        }
        ERROR( "Unable to intialize any audio driver, aborting." );
        exit(2);
    }
    return audio_driver;
}

/**
 * Set the output driver, sample rate and format. Also initializes the 
 * output buffers, flushing any current data and reallocating as 
 * necessary.
 */
gboolean audio_set_driver( audio_driver_t driver )
{
    uint32_t bytes_per_sample = 1;
    uint32_t samples_per_buffer;
    int i;

    if( audio_driver == NULL || driver != NULL ) {
        if( driver == NULL  )
            driver = &audio_null_driver;
        if( driver != audio_driver ) {	
            if( !driver->init() )
                return FALSE;
            audio_driver = driver;
        }
    }

    switch( driver->sample_format & AUDIO_FMT_SAMPLE_MASK ) {
    case AUDIO_FMT_8BIT:
        bytes_per_sample = 1;
        break;
    case AUDIO_FMT_16BIT:
        bytes_per_sample = 2;
        break;
    case AUDIO_FMT_FLOAT:
        bytes_per_sample = 4;
        break;
    }

    if( driver->sample_format & AUDIO_FMT_STEREO )
        bytes_per_sample <<= 1;
    if( driver->sample_rate == audio.output_rate &&
            bytes_per_sample == audio.output_sample_size )
        return TRUE;
    samples_per_buffer = (driver->sample_rate * MS_PER_BUFFER / 1000);
    for( i=0; i<NUM_BUFFERS; i++ ) {
        if( audio.output_buffers[i] != NULL )
            free(audio.output_buffers[i]);
        audio.output_buffers[i] = g_malloc0( sizeof(struct audio_buffer) + samples_per_buffer * bytes_per_sample );
        audio.output_buffers[i]->length = samples_per_buffer * bytes_per_sample;
        audio.output_buffers[i]->posn = 0;
        audio.output_buffers[i]->status = BUFFER_EMPTY;
    }
    audio.output_format = driver->sample_format;
    audio.output_rate = driver->sample_rate;
    audio.output_sample_size = bytes_per_sample;
    audio.write_buffer = 0;
    audio.read_buffer = 0;

    return TRUE;
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
    int next_buffer = NEXT_BUFFER();
    result = audio.output_buffers[next_buffer];
    if( result->status == BUFFER_FULL )
        return NULL;
    else {
        audio.write_buffer = next_buffer;
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
    if( current->status == BUFFER_FULL ) {
        // Current read buffer has data, which we've just emptied
        current->status = BUFFER_EMPTY;
        current->posn = 0;
        audio.read_buffer++;
        if( audio.read_buffer == NUM_BUFFERS )
            audio.read_buffer = 0;

        current = audio.output_buffers[audio.read_buffer];
        if( current->status == BUFFER_FULL ) {
            current->posn = 0;
            return current;
        }
        else return NULL;
    } else {
        return NULL;
    }

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

    for( i=0; i < AUDIO_CHANNEL_COUNT; i++ ) {
        audio_channel_t channel = &audio.channels[i];
        if( channel->active ) {
            int32_t sample;
            int vol_left = (channel->vol * (32 - channel->pan)) >> 5;
            int vol_right = (channel->vol * (channel->pan + 1)) >> 5;
            switch( channel->sample_format ) {
            case AUDIO_FMT_16BIT:
                for( j=0; j<num_samples; j++ ) {
                    sample = ((int16_t *)(aica_main_ram + channel->start))[channel->posn];
                    result_buf[j][0] += sample * vol_left;
                    result_buf[j][1] += sample * vol_right;

                    channel->posn_left += channel->sample_rate;
                    while( channel->posn_left > audio.output_rate ) {
                        channel->posn_left -= audio.output_rate;
                        channel->posn++;

                        if( channel->posn == channel->end ) {
                            if( channel->loop ) {
                                channel->posn = channel->loop_start;
                                channel->loop = LOOP_LOOPED;
                            } else {
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
                    sample = ((int8_t *)(aica_main_ram + channel->start))[channel->posn] << 8;
                    result_buf[j][0] += sample * vol_left;
                    result_buf[j][1] += sample * vol_right;

                    channel->posn_left += channel->sample_rate;
                    while( channel->posn_left > audio.output_rate ) {
                        channel->posn_left -= audio.output_rate;
                        channel->posn++;

                        if( channel->posn == channel->end ) {
                            if( channel->loop ) {
                                channel->posn = channel->loop_start;
                                channel->loop = LOOP_LOOPED;
                            } else {
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
                        channel->posn++;
                        if( channel->posn == channel->end ) {
                            if( channel->loop ) {
                                channel->posn = channel->loop_start;
                                channel->loop = LOOP_LOOPED;
                                channel->adpcm_predict = 0;
                                channel->adpcm_step = 0;
                            } else {
                                audio_stop_channel(i);
                                j = num_samples;
                                break;
                            }
                        }
                        uint8_t data = ((uint8_t *)(aica_main_ram + channel->start))[channel->posn>>1];
                        if( channel->posn&1 ) {
                            adpcm_yamaha_decode_nibble( channel, (data >> 4) & 0x0F );
                        } else {
                            adpcm_yamaha_decode_nibble( channel, data & 0x0F );
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
    audio_buffer_t buf = audio.output_buffers[audio.write_buffer];
    if( buf->status == BUFFER_FULL ) {
        buf = audio_next_write_buffer();
        if( buf == NULL ) { // no available space
            return;
        }
    }

    switch( audio.output_format & AUDIO_FMT_SAMPLE_MASK ) {
    case AUDIO_FMT_FLOAT: {
        float scale = 1.0/SHRT_MAX;
        float *data = (float *)&buf->data[buf->posn];
        for( j=0; j<num_samples; j++ ) {
            *data++ = scale * (result_buf[j][0] >> 6);
            *data++ = scale * (result_buf[j][1] >> 6);
            buf->posn += 8;
            if( buf->posn == buf->length ) {
                buf = audio_next_write_buffer();
                if( buf == NULL ) {
                    break;
                }
                data = (float *)&buf->data[0];
            }
        }
        break;
    }
    case AUDIO_FMT_16BIT: {
        int16_t *data = (int16_t *)&buf->data[buf->posn];
        for( j=0; j < num_samples; j++ ) {
            *data++ = (int16_t)(result_buf[j][0] >> 6);
            *data++ = (int16_t)(result_buf[j][1] >> 6);	
            buf->posn += 4;
            if( buf->posn == buf->length ) {
                buf = audio_next_write_buffer();
                if( buf == NULL ) {
                    // All buffers are full
                    break;
                }
                data = (int16_t *)&buf->data[0];
            }
        }
        break;
    }
    case AUDIO_FMT_8BIT: {
        int8_t *data = (int8_t *)&buf->data[buf->posn];
        for( j=0; j < num_samples; j++ ) {
            *data++ = (int8_t)(result_buf[j][0] >> 16);
            *data++ = (int8_t)(result_buf[j][1] >> 16);	
            buf->posn += 2;
            if( buf->posn == buf->length ) {
                buf = audio_next_write_buffer();
                if( buf == NULL ) {
                    // All buffers are full
                    break;
                }
                buf = audio.output_buffers[audio.write_buffer];
                data = (int8_t *)&buf->data[0];
            }
        }
        break;
    }
    }
}

/********************** Internal AICA calls ***************************/

audio_channel_t audio_get_channel( int channel ) 
{
    return &audio.channels[channel];
}

void audio_start_stop_channel( int channel, gboolean start )
{
    if( audio.channels[channel].active ) {
        if( !start ) {
            audio_stop_channel(channel);
        }
    } else if( start ) {
        audio_start_channel(channel);
    }
}

void audio_stop_channel( int channel ) 
{
    audio.channels[channel].active = FALSE;
}


void audio_start_channel( int channel )
{
    audio.channels[channel].posn = 0;
    audio.channels[channel].posn_left = 0;
    audio.channels[channel].active = TRUE;
    if( audio.channels[channel].sample_format == AUDIO_FMT_ADPCM ) {
        audio.channels[channel].adpcm_step = 0;
        audio.channels[channel].adpcm_predict = 0;
        uint8_t data = ((uint8_t *)(aica_main_ram + audio.channels[channel].start))[0];
        adpcm_yamaha_decode_nibble( &audio.channels[channel], data & 0x0F );
    }
}
