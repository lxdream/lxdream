/**
 * $Id$
 *
 * The SDL sound driver
 *
 * Copyright (c) 2009 wahrhaft
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
#include <SDL.h>
#include <SDL_audio.h>
#include "aica/audio.h"
#include "lxdream.h"

#define SDL_SAMPLES 512        //tweaking this value may help with audio dropouts
#define BYTES_PER_SAMPLE 4     //should be changed if samples are not S16 stereo

#define BUFFER_MIN_SIZE SDL_SAMPLES * BYTES_PER_SAMPLE * 4
#define BUFFER_MAX_SIZE SDL_SAMPLES * BYTES_PER_SAMPLE * 16

static char *audio_buffer;
static int buffer_pos;

static void mix_audio(void *userdata, Uint8 *stream, int len);

static gboolean audio_sdl_init( )
{
    int rate = DEFAULT_SAMPLE_RATE;
    int format = DEFAULT_SAMPLE_FORMAT;

    SDL_AudioSpec fmt;
    fmt.freq = rate;
    if (format & AUDIO_FMT_16BIT)
        fmt.format = AUDIO_S16;
    else
        fmt.format = AUDIO_U8;
    if (format & AUDIO_FMT_STEREO)
        fmt.channels = 2;
    else
        fmt.channels = 1;

    fmt.samples = SDL_SAMPLES;
    fmt.callback = mix_audio;
    fmt.userdata = NULL;

    if (SDL_OpenAudio(&fmt, NULL) < 0)
    {
        ERROR("Unable to open audio output (SDL)");
        return FALSE;
    }
    buffer_pos = 0;
    audio_buffer = (char*)malloc(BUFFER_MAX_SIZE * sizeof(char));
    if (audio_buffer == NULL)
    {
        ERROR("Could not allocate audio buffer (SDL)");
        return FALSE;
    }

    return TRUE;
}

gboolean audio_sdl_process_buffer( audio_buffer_t buffer )
{
    SDL_LockAudio();
    if (buffer_pos + buffer->length >= BUFFER_MAX_SIZE)
    {
        DEBUG("Audio buffer full, dropping a chunk\n");
    }
    else
    {
        memcpy(audio_buffer, buffer->data, buffer->length);
        buffer_pos += buffer->length;
    }
    SDL_UnlockAudio();

    return TRUE;
}

static void mix_audio(void *userdata, Uint8 *stream, int len)
{
    if (len < buffer_pos)
    {
        memcpy(stream, audio_buffer, len);
    }
    if (buffer_pos > BUFFER_MIN_SIZE)
    {
        memcpy(audio_buffer, &audio_buffer[len], buffer_pos - len);
        buffer_pos -= len;
    }
    else
    {
        DEBUG("Audio buffer low, repeating a chunk\n");
    }
}

static gboolean audio_sdl_shutdown()
{
    SDL_CloseAudio();
    free(audio_buffer);
    return TRUE;
}

static void audio_sdl_start()
{
    SDL_PauseAudio(0);
}

static void audio_sdl_stop()
{
    SDL_PauseAudio(1);
}

static struct audio_driver audio_sdl_driver = {
    "sdl",
    N_("SDL sound driver"),
    20,
    DEFAULT_SAMPLE_RATE,
    DEFAULT_SAMPLE_FORMAT,
    audio_sdl_init,
    audio_sdl_start,
    audio_sdl_process_buffer,
    audio_sdl_stop,
    audio_sdl_shutdown
};

AUDIO_DRIVER( "sdl", audio_sdl_driver );
