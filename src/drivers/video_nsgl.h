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

#ifndef lxdream_video_nsgl_H
#define lxdream_video_nsgl_H 1

#include <AppKit/NSView.h>
#include "display.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* !video_nsgl_H */