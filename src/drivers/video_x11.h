/**
 * $Id: video_x11.h,v 1.2 2006-03-15 13:16:46 nkeynes Exp $
 *
 * Parent for all X11 display drivers.
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

#ifndef video_x11_driver_H
#define video_x11_driver_H

#include "X11/Xlib.h"
#include "video.h"

void video_x11_set_display( Display *display, Screen *screen, Window window );

extern Display *video_x11_display;
extern Screen *video_x11_screen;
extern Window video_x11_window;


gboolean video_glx_set_render_format( int x, int y, int width, int height );
void video_glx_swap_buffers();

#endif
