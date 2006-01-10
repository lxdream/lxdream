/**
 * $Id: audio.h,v 1.1 2006-01-10 13:56:54 nkeynes Exp $
 * 
 * Audio engine, ie the part that does the actual work.
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
#ifndef dream_audio_H
#define dream_audio_H 1

#include <stdint.h>
#include <glib/gtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_FMT_8BIT 0
#define AUDIO_FMT_16BIT 1
#define AUDIO_FMT_ADPCM 2
#define AUDIO_FMT_MONO 0
#define AUDIO_FMT_STEREO 4
#define AUDIO_FMT_SIGNED 0
#define AUDIO_FMT_UNSIGNED 8


typedef struct audio_channel {
    gboolean active;
    uint32_t posn;
    uint32_t posn_left;
    uint32_t start;
    uint32_t end;
    uint32_t loop_start;
    uint32_t loop_end;
    int loop_count; /* 0 = no loop, -1 = loop forever */
    int vol_left; /* 0..255 */
    int vol_right; /* 0..255 */
    uint32_t sample_rate;
    int sample_format; 
    /* Envelope etc stuff */
    /* ADPCM */
    int adpcm_nibble; /* 0 = low nibble, 1 = high nibble */
    int adpcm_step;
    int adpcm_predict;
} *audio_channel_t;


typedef struct audio_buffer {
    uint32_t length; /* Samples */
    uint32_t posn; /* Samples */
    int status;
    char data[0];
} *audio_buffer_t;

struct audio_driver {
    gboolean (*set_output_format)( uint32_t sample_rate, uint32_t format );
    gboolean (*process_buffer)( audio_buffer_t buffer );
};

typedef struct audio_driver *audio_driver_t;

extern struct audio_driver null_audio_driver;
extern struct audio_driver esd_audio_driver;

/**
 * Set the output driver, sample rate and format. Also initializes the 
 * output buffers, flushing any current data and reallocating as 
 * necessary. Must be called before attempting to generate any audio.
 */
void audio_set_output( audio_driver_t driver, uint32_t samplerate,
		       int format );

/**
 * Mark the current write buffer as full and prepare the next buffer for
 * writing. Returns the next buffer to write to.
 * If all buffers are full, returns NULL.
 */
audio_buffer_t audio_next_write_buffer();

/**
 * Mark the current read buffer as empty and return the next buffer for
 * reading. If there is no next buffer yet, returns NULL.
 */
audio_buffer_t audio_next_read_buffer();

/**
 * Mix a single output sample and append it to the output buffers
 */
void audio_mix_sample( void );

/**
 * Retrieve the channel information for the channel, numbered 0..63. 
 */
audio_channel_t audio_get_channel( int channel );

void audio_start_channel( int channel );
void audio_stop_channel( int channel );


#ifdef __cplusplus
}
#endif
#endif
