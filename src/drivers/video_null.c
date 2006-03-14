/**
 * $Id: video_null.c,v 1.1 2006-03-14 12:45:53 nkeynes Exp $
 *
 * Null video output driver (ie no video output whatsoever)
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

#include "video.h"

gboolean video_null_set_output_format( uint32_t hres, uint32_t vres,
				       int colour_format )
{
    return TRUE;
}

gboolean video_null_set_render_format( uint32_t hres, uint32_t vres,
				       int colour_format, gboolean tex )
{
    return TRUE;
}

gboolean video_null_display_frame( video_buffer_t buffer )
{
    return TRUE;
}

gboolean video_null_blank( uint32_t colour )
{
    return TRUE;
}

void video_null_display_back_buffer( void )
{
}


struct video_driver video_null_driver = { "null", 
					 NULL,
					 NULL,
					 video_null_set_output_format,
					 video_null_set_render_format,
					 video_null_display_frame,
					 video_null_blank,
					 video_null_display_back_buffer };
