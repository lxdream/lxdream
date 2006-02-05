/**
 * $Id: video.h,v 1.4 2006-02-05 04:05:27 nkeynes Exp $
 *
 * The PC side of the video support (responsible for actually displaying / 
 * rendering frames)
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

#ifndef dream_video_H
#define dream_video_H

#include <stdint.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COLFMT_RGB15 0x00000000
#define COLFMT_RGB16 0x00000004
#define COLFMT_RGB24 0x00000008
#define COLFMT_RGB32 0x0000000C

typedef struct video_buffer {
    uint32_t hres;
    uint32_t vres;
    uint32_t rowstride;
    int colour_format;
    char *data;
} *video_buffer_t;

typedef struct video_driver {
    char *name;
    gboolean (*set_output_format)( uint32_t hres, uint32_t vres, 
				   int colour_fmt );
    gboolean (*display_frame)( video_buffer_t buffer );
    gboolean (*display_blank_frame)( uint32_t rgb );
} *video_driver_t;


void video_open( void );
void video_update_frame( void );
void video_update_size( int, int, int );

extern uint32_t pvr2_frame_counter;

extern struct video_driver video_gtk_driver;

#ifdef __cplusplus
}
#endif
#endif
