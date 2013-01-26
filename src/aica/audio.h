/**
 * $Id$
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
#ifndef lxdream_audio_H
#define lxdream_audio_H 1

#include <stdint.h>
#include <stdio.h>
#include <glib.h>
#include "gettext.h"
#include "plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_CHANNEL_COUNT 64

#define AUDIO_FMT_8BIT 0
#define AUDIO_FMT_16BIT 1
#define AUDIO_FMT_ADPCM 2
#define AUDIO_FMT_FLOAT 3  // 32-bit -1.0 to 1.0
#define AUDIO_FMT_MONO 0
#define AUDIO_FMT_STEREO 4
#define AUDIO_FMT_SIGNED 0
#define AUDIO_FMT_UNSIGNED 8
    
#define AUDIO_FMT_SAMPLE_MASK 3

#define AUDIO_FMT_16ST (AUDIO_FMT_16BIT|AUDIO_FMT_STEREO)
#define AUDIO_FMT_FLOATST (AUDIO_FMT_FLOAT|AUDIO_FMT_STEREO)
#define AUDIO_MEM_MASK 0x1FFFFF

#define DEFAULT_SAMPLE_RATE 44100
#define DEFAULT_SAMPLE_FORMAT AUDIO_FMT_16ST
    
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
    const char *name;
    const char *description;
    int priority; /* Lower == higher priority */
    uint32_t sample_rate;
    uint32_t sample_format;
    gboolean (*init)( );
    void (*start)( );
    gboolean (*process_buffer)( audio_buffer_t buffer );
    void (*stop)( );
    gboolean (*shutdown)(  );
} *audio_driver_t;


/**
 * Print the configured audio drivers to the output stream, one to a line.
 */
void print_audio_drivers( FILE *out );

audio_driver_t get_audio_driver_by_name( const char *name );

/**
 * Set the output driver, sample rate and format. Also initializes the 
 * output buffers, flushing any current data and reallocating as 
 * necessary. Must be called before attempting to generate any audio.
 */
gboolean audio_set_driver( audio_driver_t driver );

/**
 * Initialize the audio driver, using the specified driver if available.
 */
audio_driver_t audio_init_driver( const char *preferred_driver );

/**
 * Add a new audio driver to the available drivers list
 */
gboolean audio_register_driver( audio_driver_t driver );

/**
 * Signal the audio driver that playback is beginning
 */
void audio_start_driver();

/**
 * Signal the audio driver that playback is stopping
 */
void audio_stop_driver();

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

void audio_save_state( FILE *f );
int audio_load_state( FILE *f );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_audio_H */
