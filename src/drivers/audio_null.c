/**
 * $Id: audio_null.c,v 1.1 2006-01-10 13:56:54 nkeynes Exp $
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

gboolean null_audio_set_format( uint32_t rate, uint32_t format )
{
    return TRUE;
}

gboolean null_audio_process_buffer( audio_buffer_t buffer )
{
    return TRUE;
}

struct audio_driver null_audio_driver = { null_audio_set_format, null_audio_process_buffer };
