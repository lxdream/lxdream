/**
 * $Id$
 *
 * Cocoa (NSOpenGL) video driver
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

#ifndef video_nsgl_H
#define video_nsgl_H

#include <AppKit/NSView.h>
#include "display.h"

/**
 * Initialize the display driver by setting the appropriate methods
 * for NSGL support
 */
gboolean video_nsgl_init_driver( NSView *view, display_driver_t driver );

/**
 * Shutdown GLX support and release all resources.
 */
void video_nsgl_shutdown();

/**
 * Standard front/back buffer swap
 */
void video_nsgl_swap_buffers();

#endif /* !video_nsgl_H */