/**
 * $Id$
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

#include "display.h"

static render_buffer_t video_null_create_render_buffer( uint32_t hres, uint32_t vres, GLuint tex_id )
{
    return NULL;
}

static void video_null_destroy_render_buffer( render_buffer_t buffer )
{
}

static gboolean video_null_set_render_target( render_buffer_t buffer )
{
    return TRUE;
}

static void video_null_finish_render( render_buffer_t buffer )
{
}

static void video_null_display_render_buffer( render_buffer_t buffer )
{
}

static gboolean video_null_read_render_buffer( unsigned char *target, 
                                               render_buffer_t buffer, 
                                               int rowstride, int format )
{
    return TRUE;
}

static void video_null_load_frame_buffer( frame_buffer_t frame, 
                                          render_buffer_t buffer )
{
}

static void video_null_display_blank( uint32_t colour )
{
}

static void video_null_swap_buffers(void)
{
}

struct display_driver display_null_driver = { 
        "null",
        N_("Null (no video) driver"),
        NULL,
        NULL,
        NULL,
        NULL, 
        NULL,
        video_null_create_render_buffer,
        video_null_destroy_render_buffer,
        video_null_set_render_target,
        video_null_finish_render,
        video_null_load_frame_buffer,
        video_null_display_render_buffer,
        video_null_display_blank,
        video_null_swap_buffers,
        video_null_read_render_buffer,
        NULL };
