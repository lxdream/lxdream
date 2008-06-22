/**
 * $Id$
 * 
 * The darwin core-audio audio driver
 *
 * Copyright (c) 2008 Nathan Keynes.
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
#include <CoreAudio/CoreAudio.h>
#include "aica/audio.h"
#include "lxdream.h"

#define BUFFER_SIZE (sizeof(float)*2*2205)

static AudioDeviceID output_device;
static volatile audio_buffer_t output_buffer = NULL;
static uint32_t buffer_size;

OSStatus audio_osx_callback( AudioDeviceID inDevice,
        const AudioTimeStamp *inNow,
        const AudioBufferList *inInputData,
        const AudioTimeStamp *inInputTime,
        AudioBufferList *outOutputData,
        const AudioTimeStamp *inOutputTime,
        void *inClientData)
{
    char *output = outOutputData->mBuffers[0].mData;
    int data_requested = buffer_size;

    while( output_buffer != NULL && data_requested > 0 ) {
        int copysize = output_buffer->length - output_buffer->posn;
        if( copysize > data_requested ) {
            copysize = data_requested;
        }
        memcpy( output, &output_buffer->data[output_buffer->posn], copysize );
        output += copysize;
        data_requested -= copysize;
        output_buffer->posn += copysize;
        if( output_buffer->posn >= output_buffer->length ) {
            output_buffer = audio_next_read_buffer();
        }
    }
    if( data_requested > 0 ) {
        memset( output, 0, data_requested );
    }
    return noErr;
}

static gboolean audio_osx_shutdown()
{
    AudioDeviceStop( output_device, audio_osx_callback );
    AudioDeviceRemoveIOProc( output_device, audio_osx_callback );
    return TRUE;
}

static gboolean audio_osx_init()
{
    UInt32 size = sizeof(output_device);
    AudioStreamBasicDescription outputDesc;
    UInt32 outputDescSize = sizeof(outputDesc);
    
    if( AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                 &size, &output_device) != noErr ||
        output_device == kAudioDeviceUnknown ) {
        return FALSE;
    }
     
    if( AudioDeviceGetProperty( output_device, 1, 0, kAudioDevicePropertyStreamFormat,
            &outputDescSize, &outputDesc ) != noErr ) {
        return FALSE;
    }
    
    buffer_size = BUFFER_SIZE;
    
    if( AudioDeviceSetProperty( output_device, 0, 0, 0, kAudioDevicePropertyBufferSize,
                                sizeof(buffer_size), &buffer_size ) != noErr ) {
        return FALSE;
    }
    
    AudioDeviceAddIOProc( output_device, audio_osx_callback, NULL );    
    return TRUE;
}
static gboolean audio_osx_process_buffer( audio_buffer_t buffer )
{
    if( output_buffer == NULL ) {
        output_buffer = buffer;
        output_buffer->posn = 0;
        AudioDeviceStart(output_device, audio_osx_callback);
        return FALSE;
    }
}

void audio_osx_start()
{
    if( output_buffer != NULL ) {
        AudioDeviceStart(output_device, audio_osx_callback);
    }
}

void audio_osx_stop()
{
    AudioDeviceStop( output_device, audio_osx_callback );
}


struct audio_driver audio_osx_driver = { 
        "osx",
        N_("OS X CoreAudio system driver"), 
        DEFAULT_SAMPLE_RATE,
        AUDIO_FMT_FLOATST,
        audio_osx_init,
        audio_osx_start, 
        audio_osx_process_buffer,
        audio_osx_stop,
        audio_osx_shutdown};

