/**
 * $Id$
 * 
 * The esd (esound) audio driver
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
#include <esd.h>
#include "aica/audio.h"
#include "dream.h"

static int esd_handle = -1;
static int esd_sample_size = 1;


static gboolean audio_esd_init()
{
    int format = DEFAULT_SAMPLE_FORMAT;
    int rate = DEFAULT_SAMPLE_RATE;

    if( esd_handle != -1 ) {
        esd_close(esd_handle);
    }
    esd_format_t esd_format = 0;
    esd_sample_size = 1;
    if( format & AUDIO_FMT_16BIT ) {
        esd_format |= ESD_BITS16;
    } else esd_format |= ESD_BITS8;
    if( format & AUDIO_FMT_STEREO ) {
        esd_format |= ESD_STEREO;
    }
    else esd_format |= ESD_MONO;

    esd_handle = esd_play_stream( esd_format, rate, "localhost", "lxdream" );
    if( esd_handle == -1 ) {
        ERROR( "Unable to open audio output (ESD)" );
        return FALSE;
    }
    return TRUE;
}

static gboolean audio_esd_process_buffer( audio_buffer_t buffer )
{
    if( esd_handle != -1 ) {
        write( esd_handle, buffer->data, buffer->length );
        return TRUE;
    } else {
        ERROR( "ESD not initialized" );
        return FALSE;
    }
}

static gboolean audio_esd_shutdown()
{
    close(esd_handle);
    esd_handle = -1;
    return TRUE;
}

static struct audio_driver audio_esd_driver = { 
        "esd",
        N_("Enlightened Sound Daemon driver"),
        30,
        DEFAULT_SAMPLE_RATE,
        DEFAULT_SAMPLE_FORMAT,
        audio_esd_init,
        NULL, 
        audio_esd_process_buffer,
        NULL,
        audio_esd_shutdown};

AUDIO_DRIVER( "esd", audio_esd_driver );
