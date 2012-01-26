/**
 * $Id$
 * 
 * The "null" audio driver, which just discards all input without even
 * looking at it.
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
#include "aica/audio.h"

static gboolean audio_null_init()
{
    return TRUE;
}

static gboolean audio_null_process_buffer( audio_buffer_t buffer )
{
    return TRUE;
}

static gboolean audio_null_shutdown()
{
    return TRUE;
}

struct audio_driver audio_null_driver = { 
        "null",
        N_("Null (no audio) driver"),
        65536, // Always last
        DEFAULT_SAMPLE_RATE,
        DEFAULT_SAMPLE_FORMAT,
        audio_null_init,
        NULL,
        audio_null_process_buffer,
        NULL,
        audio_null_shutdown};

AUDIO_DRIVER( "null", audio_null_driver );
