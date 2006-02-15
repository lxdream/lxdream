/**
 * $Id: gl_glx.c,v 1.2 2006-02-15 12:39:13 nkeynes Exp $
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

GLXContext glx_context;
Window glx_window;
gboolean glx_open = FALSE;

gboolean gl_glx_create_window( uint32_t width, uint32_t height )
{
    int major, minor;
    const char *glxExts, *glxServer;
    int visual_attrs[] = { GLX_RGBA, GLX_RED_SIZE, 4, 
			   GLX_GREEN_SIZE, 4, 
			   GLX_BLUE_SIZE, 4,
			   GLX_ALPHA_SIZE, 4,
			   GLX_DEPTH_SIZE, 16,
			   GLX_DOUBLEBUFFER, 
			   None };
    int screen = XScreenNumberOfScreen(video_x11_screen);
    XSetWindowAttributes win_attrs;
    XVisualInfo *visual;

    if( glXQueryVersion( video_x11_display, &major, &minor ) == False ) {
	ERROR( "X Display lacks the GLX nature" );
	return FALSE;
    }
    if( major < 1 || minor < 2 ) {
	ERROR( "X display supports GLX %d.%d, but we need at least 1.2", major, minor );
	return FALSE;
    }

    glxExts = glXQueryExtensionsString( video_x11_display, screen );
    glxServer = glXQueryServerString( video_x11_display, screen, GLX_VENDOR );
    INFO( "GLX version %d.%d, %s. Supported exts: %s", major, minor,
	  glxServer, glxExts );

    /* Find ourselves a nice visual */
    visual = glXChooseVisual( video_x11_display, 
			      screen,
			      visual_attrs );
    if( visual == NULL ) {
	ERROR( "Unable to obtain a compatible visual" );
	return FALSE;
    }

    /* And a matching gl context */
    glx_context = glXCreateContext( video_x11_display, visual, None, True );
    if( glx_context == NULL ) {
	ERROR( "Unable to obtain a GLX Context. Possibly your system is broken in some small, undefineable way" );
	return FALSE;
    }
    

    /* Ok, all good so far. Unfortunately the visual we need to use will 
     * almost certainly be different from the one our frame is using. Which 
     * means we have to jump through the following hoops to create a 
     * child window with the appropriate settings.
     */
    win_attrs.event_mask = 0;
    win_attrs.colormap = XCreateColormap( video_x11_display, 
					  RootWindowOfScreen(video_x11_screen),
					  visual->visual, AllocNone );
    glx_window = XCreateWindow( video_x11_display, video_x11_window, 
				0, 0, width, height, 0, visual->depth, 
				InputOutput, visual->visual, 
				CWColormap | CWEventMask, 
				&win_attrs );
    if( glx_window == None ) {
	/* Hrm. Aww, no window? */
	ERROR( "Unable to create GLX window" );
	glXDestroyContext( video_x11_display, glx_context );
	if( win_attrs.colormap ) 
	    XFreeColormap( video_x11_display, win_attrs.colormap );
	return FALSE;
    }
    XMapRaised( video_x11_display, glx_window );

    /* And finally set the window to be the active drawing area */
    if( glXMakeCurrent( video_x11_display, glx_window, glx_context ) == False ) {
	/* Oh you have _GOT_ to be kidding me */
	ERROR( "Unable to prepare GLX window for drawing" );
	XDestroyWindow( video_x11_display, glx_window );
	XFreeColormap( video_x11_display, win_attrs.colormap );
	glXDestroyContext( video_x11_display, glx_context );
	return FALSE;
    }
    
    glx_open = TRUE;
    return TRUE;
}



gboolean gl_glx_set_output_format( uint32_t width, uint32_t height,
				   int colour_format )
{
    GLXFBConfig config;
    int screen = XScreenNumberOfScreen(video_x11_screen);
    int buffer_attrs[] = { GLX_PBUFFER_WIDTH, width, 
			   GLX_PBUFFER_HEIGHT, height,
			   GLX_PRESERVED_CONTENTS, True,
			   None };
    if( !glx_open ) {
	if( !gl_glx_create_window( width, height ) )
	    return FALSE;
    }
    return TRUE;
}

gboolean gl_glx_swap_frame( )
{
    return FALSE;
}
