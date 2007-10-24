/**
 * $Id: audio.h,v 1.9 2007-10-24 21:24:09 nkeynes Exp $
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

#define AUDIO_FMT_16ST (AUDIO_FMT_16BIT|AUDIO_FMT_STEREO)

typedef enum { LOOP_OFF = 0, LOOP_ON = 1, LOOP_LOOPED = 2 } loop_t;

typedef struct audio_channel {
    gboolean active;
    uint32_t posn; /* current sample #, 0 = first sample */
    uint32_t posn_left;
    uint32_t start;
    uint32_t end;
    loop_t loop;
    uint32_t loop_start;
    int vol; /* 0..255 */
    int pan; /* 0 (left) .. 31 (right) */
    uint32_t sample_rate;
    int sample_format; 
    /* Envelope etc stuff */
    /* ADPCM */
    int adpcm_step;
    int adpcm_predict;
} *audio_channel_t;


typedef struct audio_buffer {
    uint32_t length; /* Bytes */
    uint32_t posn; /* Bytes */
    int status;
    char data[0];
} *audio_buffer_t;

typedef struct audio_driver {
    char *name;
    gboolean (*set_output_format)( uint32_t sample_rate, uint32_t format );
    gboolean (*process_buffer)( audio_buffer_t buffer );
} *audio_driver_t;

extern struct audio_driver audio_null_driver;
extern struct audio_driver audio_esd_driver;

/**
 * Set the output driver, sample rate and format. Also initializes the 
 * output buffers, flushing any current data and reallocating as 
 * necessary. Must be called before attempting to generate any audio.
 */
gboolean audio_set_driver( audio_driver_t driver, uint32_t samplerate,
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
void audio_mix_samples( int num_samples );

/**
 * Retrieve the channel information for the channel, numbered 0..63. 
 */
audio_channel_t audio_get_channel( int channel );

void audio_start_stop_channel( int channel, gboolean start );
void audio_start_channel( int channel );
void audio_stop_channel( int channel );


#ifdef __cplusplus
}
#endif
#endif
