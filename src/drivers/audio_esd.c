/**
 * $Id: audio_esd.c,v 1.5 2006-01-22 22:40:05 nkeynes Exp $
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
#include <esd.h>
#include "aica/audio.h"
#include "dream.h"

int esd_handle = -1;
int esd_sample_size = 1;

gboolean esd_audio_set_format( uint32_t rate, uint32_t format )
{
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
    
    esd_handle = esd_play_stream( esd_format, rate, "localhost", "dreamon" );
    if( esd_handle == -1 ) {
	ERROR( "Unable to open audio output (ESD)" );
    }
    return TRUE;
}

gboolean esd_audio_process_buffer( audio_buffer_t buffer )
{
    if( esd_handle != -1 ) {
	write( esd_handle, buffer->data, buffer->length );
	return TRUE;
    } else {
	ERROR( "ESD not initialized" );
	return FALSE;
    }
}

struct audio_driver esd_audio_driver = { "esd", esd_audio_set_format, esd_audio_process_buffer };

