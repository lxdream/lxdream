/**
 * $Id: audio_esd.c 602 2008-01-15 20:50:23Z nkeynes $
 * 
 * The asla  audio driver
 *
 * Copyright (c) 2008 Jonathan Muller
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

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>
#include "config.h"
#include "aica/audio.h"
#include "dream.h"


static snd_pcm_t *_soundDevice = NULL;
static int frame_bytes;


struct lxdream_config_entry alsa_config[] = {
        {"device", N_("Audio output device"), CONFIG_TYPE_FILE, "default"},
        {NULL, CONFIG_TYPE_NONE}
};


gboolean audio_alsa_init(  )
{
    int err;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
    snd_pcm_uframes_t frames;
    snd_pcm_uframes_t bufferSize;
    int rate = DEFAULT_SAMPLE_RATE;
    int format = DEFAULT_SAMPLE_FORMAT;
    int dir;


    // Open the device we were told to open.
    err = snd_pcm_open( &_soundDevice, alsa_config[0].value,
            SND_PCM_STREAM_PLAYBACK, 0 );

    // Check for error on open.
    if ( err < 0 ) {
        ERROR( "Init: cannot open audio device %s (%s)\n",
                alsa_config[0].value, snd_strerror( err ) );
        return FALSE;
    } else {
        DEBUG( "Audio device opened successfully." );
    }

    frame_bytes = ( 2 * ( snd_pcm_format_width( SND_PCM_FORMAT_S16_LE ) / 8 ) );


    //snd_pcm_hw_params_alloca (&hw_params);
    // Allocate the hardware parameter structure.
    if ( ( err = snd_pcm_hw_params_malloc( &hw_params ) ) < 0 ) {
        ERROR( "Init: cannot allocate hardware parameter structure (%s)\n",
                snd_strerror( err ) );
        return FALSE;
    }

    if ( ( err = snd_pcm_hw_params_any( _soundDevice, hw_params ) ) < 0 ) {
        ERROR( "Init: cannot allocate hardware parameter structure (%s)\n",
                snd_strerror( err ) );
        return FALSE;
    }
    // Set access to RW interleaved.
    if ( ( err = snd_pcm_hw_params_set_access( _soundDevice, hw_params,
            SND_PCM_ACCESS_RW_INTERLEAVED ) )
            < 0 ) {
        ERROR( " Init: cannot set access type (%s)\n", snd_strerror( err ) );
        return FALSE;
    }

    if ( ( err = snd_pcm_hw_params_set_format( _soundDevice, hw_params,
            SND_PCM_FORMAT_S16_LE ) ) <
            0 ) {
        ERROR( "Init: cannot set sample format (%s)\n", snd_strerror( err ) );
        return FALSE;
    }

    err = snd_pcm_hw_params_set_rate_near( _soundDevice, hw_params, &rate, 0 );
    if ( err < 0 ) {
        ERROR( "Init: Resampling setup failed for playback: %s\n",
                snd_strerror( err ) );
        return err;
    }
    // Set channels to stereo (2).
    err = snd_pcm_hw_params_set_channels( _soundDevice, hw_params, 2 );
    if ( err < 0 ) {
        ERROR( "Init: cannot set channel count (%s)\n", snd_strerror( err ) );
        return FALSE;
    }

    // frames = 4410;
    // snd_pcm_hw_params_set_period_size_near( _soundDevice, hw_params, &frames,
    //                                     &dir );

    // Apply the hardware parameters that we've set.
    err = snd_pcm_hw_params( _soundDevice, hw_params );
    if ( err < 0 ) {
        DEBUG( "Init: cannot set parameters (%s)\n", snd_strerror( err ) );
        return FALSE;
    } else {
        DEBUG( "Audio device parameters have been set successfully." );
    }

    snd_pcm_hw_params_get_period_size( hw_params, &frames, &dir );
    DEBUG( "period size = %d\n", frames );

    // Get the buffer size.
    snd_pcm_hw_params_get_buffer_size( hw_params, &bufferSize );
    DEBUG("Buffer Size = %d\n", bufferSize);

    // If we were going to do more with our sound device we would want to store
    // the buffer size so we know how much data we will need to fill it with.

    //cout << "Init: Buffer size = " << bufferSize << " frames." << endl;

    // Display the bit size of samples.
    //cout << "Init: Significant bits for linear samples = " << snd_pcm_hw_params_get_sbits(hw_params) << endl;

    // Free the hardware parameters now that we're done with them.
    snd_pcm_hw_params_free( hw_params );

    // Set the start threshold to reduce inter-buffer gaps
    snd_pcm_sw_params_alloca( &sw_params );
    snd_pcm_sw_params_current( _soundDevice, sw_params );
    snd_pcm_sw_params_set_start_threshold( _soundDevice, sw_params, bufferSize/2 );
    err = snd_pcm_sw_params( _soundDevice, sw_params );
    if( err < 0 ) {
        ERROR("Unable to set sw params for alsa driver: %s\n", snd_strerror(err));
        return FALSE;
    } 

    err = snd_pcm_prepare( _soundDevice );
    if ( err < 0 ) {
        ERROR( "Init: cannot prepare audio interface for use (%s)\n",
                snd_strerror( err ) );
        return FALSE;
    }
    return TRUE;
}

gboolean audio_alsa_process_buffer( audio_buffer_t buffer )
{
    int err;
    int length;


    length = buffer->length / frame_bytes;

    err = snd_pcm_writei( _soundDevice, buffer->data, length );
    if( err == -EPIPE ) {
        snd_pcm_prepare( _soundDevice );
    } else if( err == -ESTRPIPE ) {
        snd_pcm_resume( _soundDevice );
    }

    return TRUE;
}


gboolean audio_alsa_shutdown(  )
{
    return TRUE;
}



struct audio_driver audio_alsa_driver = { 
        "alsa",
        N_("Linux ALSA system driver"),
        DEFAULT_SAMPLE_RATE,
        DEFAULT_SAMPLE_FORMAT,
        audio_alsa_init,
        NULL,
        audio_alsa_process_buffer,
        NULL,
        audio_alsa_shutdown
};
