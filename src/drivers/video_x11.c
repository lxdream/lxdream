/**
 * $Id: video_x11.c,v 1.15 2007-10-07 05:42:25 nkeynes Exp $
 *
 * Shared functions for all X11-based display drivers.
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

#include <X11/Xlib.h>
#include <GL/glx.h>
#include "dream.h"
#include "drivers/video_x11.h"
#include "drivers/gl_common.h"

extern uint32_t video_width, video_height;

/**
 * General X11 parameters. The front-end driver is expected to set this up
 * by calling video_glx_init after initializing itself.
 */
static Display *video_x11_display = NULL;
static Screen *video_x11_screen = NULL;
static Window video_x11_window = 0;
static gboolean glsl_loaded = FALSE;

/**
 * GLX parameters.
 */
static GLXContext glx_context;
static Window glx_window;
static XSetWindowAttributes win_attrs;

gboolean video_glx_create_window( int width, int height );

gboolean video_glx_init( Display *display, Screen *screen, Window window,
			 int width, int height, display_driver_t driver )
{
    video_x11_display = display;
    video_x11_screen = screen;
    video_x11_window = window;

    if( !video_glx_create_window(width,height) ) {
	return FALSE;
    }

    if( gl_fbo_is_supported() ) {
	gl_fbo_init(driver);

#ifdef USE_GLSL
	if( glsl_is_supported() ) {
	    glsl_loaded = glsl_load_shaders( glsl_vertex_shader_src, glsl_fragment_shader_src );
	    if( !glsl_loaded ) {
	        WARN( "Shaders failed to load" );
	    }
	} else {
	    WARN( "Shaders not supported" );
	}
#endif
	return TRUE;
    } else {
	/* Pbuffers? */
	ERROR( "Framebuffer objects not supported (required in this version)" );
	video_glx_shutdown();
	return FALSE;
    }
}

gboolean video_glx_create_window( int width, int height )
{
    int major, minor;
    int visual_attrs[] = { GLX_RGBA, GLX_RED_SIZE, 4, 
			   GLX_GREEN_SIZE, 4, 
			   GLX_BLUE_SIZE, 4,
			   GLX_ALPHA_SIZE, 4,
			   GLX_DEPTH_SIZE, 24,
			   GLX_DOUBLEBUFFER, 
			   None };
    int screen = XScreenNumberOfScreen(video_x11_screen);
    XVisualInfo *visual;

    if( glXQueryVersion( video_x11_display, &major, &minor ) == False ) {
	ERROR( "X Display lacks the GLX nature" );
	return FALSE;
    }
    if( major < 1 || minor < 2 ) {
	ERROR( "X display supports GLX %d.%d, but we need at least 1.2", major, minor );
	return FALSE;
    }

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
	/* Ok you have _GOT_ to be kidding me */
	ERROR( "Unable to prepare GLX window for drawing" );
	XDestroyWindow( video_x11_display, glx_window );
	XFreeColormap( video_x11_display, win_attrs.colormap );
	glXDestroyContext( video_x11_display, glx_context );
	return FALSE;
    }
    return TRUE;
}

void video_glx_shutdown()
{
    if( glsl_loaded ) {
	glsl_unload_shaders();
    }
    if( glx_window != None ) {
	XDestroyWindow( video_x11_display, glx_window );
	XFreeColormap( video_x11_display, win_attrs.colormap );
	glx_window = None;
    }
    if( glx_context != NULL ) {
	glXDestroyContext( video_x11_display, glx_context );
	glx_context = NULL;
    }
}


int video_glx_load_font( const gchar *font_name )
{
    int lists;
    XFontStruct *font = XLoadQueryFont(video_x11_display, font_name );
    if (font == NULL)
	return -1;
    
    lists = glGenLists(96);
    glXUseXFont(font->fid, 32, 96, lists);
    XFreeFont(video_x11_display, font);
    return lists;
}


void video_glx_swap_buffers( void )
{
    glXSwapBuffers( video_x11_display, glx_window );
}

