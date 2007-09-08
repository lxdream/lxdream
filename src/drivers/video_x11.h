/**
 * $Id: video_x11.h,v 1.5 2007-09-08 04:05:35 nkeynes Exp $
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
#include "display.h"

gboolean video_glx_init( Display *display, Screen *screen, Window window,
			 int width, int height, display_driver_t driver );
void video_glx_shutdown();
#endif
