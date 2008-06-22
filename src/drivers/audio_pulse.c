/**
 * $Id$
 * 
 * The pulseaudio sound driver
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
#include <stdio.h>
#include <unistd.h>
#include <pulse/simple.h>
#include "aica/audio.h"
#include "lxdream.h"

static pa_simple *pulse_server = NULL;

gboolean audio_init( )
{
    int rate = DEFAULT_SAMPLE_RATE;
    int format = DEFAULT_SAMPLE_FORMAT;
    pa_sample_spec ss;

    if( pulse_server != NULL ) {
        pa_simple_free(pulse_server);
    }
    ss.rate = rate;

    if( format & AUDIO_FMT_16BIT ) {
        ss.format = PA_SAMPLE_S16NE;
    } else {
        ss.format = PA_SAMPLE_U8;
    }
    
    if( format & AUDIO_FMT_STEREO ) {
	ss.channels = 2;
    } else {
        ss.channels = 1;
    }

    pulse_server = pa_simple_new(NULL, APP_NAME, PA_STREAM_PLAYBACK,
                                 NULL, "Audio", &ss, NULL, NULL, NULL);
    if( pulse_server == NULL ) {
	ERROR( "Unable to open audio output (pulseaudio)" );
	return FALSE;
    }
    return TRUE;
}

gboolean audio_pulse_process_buffer( audio_buffer_t buffer )
{
    if( pulse_server != NULL ) {
        int error;
        pa_simple_write( pulse_server, buffer->data, buffer->length, &error );
	return TRUE;
    } else {
	ERROR( "Pulseaudio not initialized" );
	return FALSE;
    }
}

gboolean audio_pulse_shutdown()
{
  pa_simple_free(pulse_server);
  pulse_server = NULL;
  return TRUE;
}

struct audio_driver audio_pulse_driver = { "pulse",
        DEFAULT_SAMPLE_RATE,
        DEFAULT_SAMPLE_FORMAT,
        audio_pulse_init,
        NULL,
        audio_pulse_process_buffer,
        NULL,
        audio_pulse_shutdown};

