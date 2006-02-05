/**
 * $Id: video_x11.c,v 1.1 2006-02-05 04:05:27 nkeynes Exp $
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

#include "drivers/video_x11.h"

Display *video_x11_display = NULL;
Screen *video_x11_screen = NULL;
Window video_x11_window = 0;

void video_x11_set_display( Display *display, Screen *screen, Window window )
{
    video_x11_display = display;
    video_x11_screen = screen;
    video_x11_window = window;
}

