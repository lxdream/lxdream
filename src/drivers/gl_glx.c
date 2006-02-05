/**
 * $Id: gl_glx.c,v 1.1 2006-02-05 04:05:27 nkeynes Exp $
 *
 * GLX framebuffer support. Note depends on an X11 video driver 
 * (ie video_gtk) to maintain the X11 side of things.
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

#include "dream.h"
#include <X11/Xlib.h>
#include <GL/glx.h>
#include "video.h"
#include "drivers/video_x11.h"

gboolean gl_glx_init( )
{
    int major, minor;
    const char *glxExts, *glxServer;
    int screen = XScreenNumberOfScreen(video_x11_screen);

    if( glXQueryVersion( video_x11_display, &major, &minor ) == False ) {
	ERROR( "X Display lacks the GLX nature" );
	return FALSE;
    }
    if( major < 1 || minor < 1 ) {
	ERROR( "GLX version %d.%d is not supported", major, minor );
	return FALSE;
    }

    glxExts = glXQueryExtensionsString( video_x11_display, screen );
    glxServer = glXQueryServerString( video_x11_display, screen, GLX_VENDOR );
    INFO( "GLX version %d.%d, %s. Supported exts: %s", major, minor,
	  glxServer, glxExts );
}

gboolean gl_glx_start_frame( uint32_t width, uint32_t height,
			     int colour_format )
{
    return FALSE;
}

gboolean gl_glx_swap_frame( )
{
    return FALSE;
}
