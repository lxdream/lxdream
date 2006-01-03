/**
 * $Id: video.h,v 1.3 2006-01-03 12:21:45 nkeynes Exp $
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

#ifdef __cplusplus
extern "C" {
#endif

void video_open( void );
void video_update_frame( void );
void video_update_size( int, int, int );

extern char *video_data;
extern uint32_t video_frame_count;

#ifdef __cplusplus
}
#endif
#endif
