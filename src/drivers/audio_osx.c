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
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnitProperties.h>
#include "aica/audio.h"
#include "lxdream.h"

static AudioUnit output_au;
static volatile audio_buffer_t output_buffer = NULL;
static int sample_size;

OSStatus audio_osx_callback( void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
                             const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                             UInt32 inNumberFrames, AudioBufferList *ioData )
{
    char *output = ioData->mBuffers[0].mData;
    int data_requested = inNumberFrames * sample_size;

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
    AudioUnitUninitialize( output_au );
    CloseComponent( output_au );
    return TRUE;
}

static gboolean audio_osx_init()
{
    AURenderCallbackStruct callbackData;
    AudioStreamBasicDescription outputDesc;
    UInt32 outputDescSize = sizeof(outputDesc);
    ComponentDescription cd;
    Component c;

    cd.componentType = kAudioUnitType_Output;
    cd.componentSubType = kAudioUnitSubType_DefaultOutput;
    cd.componentManufacturer = kAudioUnitManufacturer_Apple;
    cd.componentFlags = 0;
    cd.componentFlagsMask = 0;

    c = FindNextComponent( NULL, &cd );
    if( c == NULL ) {
        return FALSE;
    }
    
    if( OpenAComponent( c, &output_au ) != noErr ) {
        return FALSE;
    }
 
    if( AudioUnitGetProperty( output_au, kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Global, 0, &outputDesc, &outputDescSize ) != noErr ) {
        CloseComponent( output_au );
        return FALSE;
    }
    
    outputDesc.mSampleRate = DEFAULT_SAMPLE_RATE;
    sample_size = outputDesc.mBytesPerFrame;
    
    if( AudioUnitSetProperty( output_au, kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Global, 0, &outputDesc, sizeof(outputDesc) ) != noErr ) {
        CloseComponent( output_au );
        return FALSE;
    }
    
    if( AudioUnitInitialize( output_au ) != noErr ) {
        CloseComponent( output_au );
        return FALSE;
    }

    callbackData.inputProc = audio_osx_callback;
    callbackData.inputProcRefCon = NULL;
    if( AudioUnitSetProperty( output_au, kAudioUnitProperty_SetRenderCallback,
            kAudioUnitScope_Global, 0, &callbackData, sizeof(callbackData)) != noErr ) {
        audio_osx_shutdown();
        return FALSE;
    }
    
    return TRUE;
}
static gboolean audio_osx_process_buffer( audio_buffer_t buffer )
{
    if( output_buffer == NULL ) {
        output_buffer = buffer;
        output_buffer->posn = 0;
        AudioOutputUnitStart(output_au);
        return FALSE;
    }
}

void audio_osx_start()
{
}

void audio_osx_stop()
{
}


struct audio_driver audio_osx_driver = { "osx", 
        DEFAULT_SAMPLE_RATE,
        AUDIO_FMT_FLOATST,
        audio_osx_init,
        audio_osx_start, 
        audio_osx_process_buffer,
        audio_osx_stop,
        audio_osx_shutdown};

