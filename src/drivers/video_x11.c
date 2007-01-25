/**
 * $Id: video_x11.c,v 1.10 2007-01-25 11:46:35 nkeynes Exp $
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

/**
 * General X11 parameters. The front-end driver is expected to set this up
 * by calling video_x11_set_display after initializing itself.
 */
Display *video_x11_display = NULL;
Screen *video_x11_screen = NULL;
Window video_x11_window = 0;

/**
 * GLX parameters.
 */
GLXContext glx_context;
Window glx_window;
gboolean glx_open = FALSE;

void video_x11_set_display( Display *display, Screen *screen, Window window )
{
    video_x11_display = display;
    video_x11_screen = screen;
    video_x11_window = window;
}


gboolean video_glx_create_window( int x, int y, int width, int height )
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
				x, y, width, height, 0, visual->depth, 
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

    hasRequiredGLExtensions();
    glx_open = TRUE;
    return TRUE;
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
}



gboolean video_glx_set_render_format( int x, int y, int width, int height )
{
    if( glx_open )
	return TRUE;
    return video_glx_create_window( x, y, width, height );
}

gboolean video_glx_display_frame( video_buffer_t frame )
{
    GLenum type = colour_formats[frame->colour_format].type;
    GLenum format = colour_formats[frame->colour_format].format;

    glDrawBuffer( GL_FRONT );
    glViewport( 0, 0, frame->hres, frame->vres );
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho( 0, frame->hres, frame->vres, 0, 0, -65535 );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRasterPos2i( 0, 0 );
    glPixelZoom( 1.0f, -1.0f );
    glDrawPixels( frame->hres, frame->vres, format, type,
		  frame->data );
    glFlush();
    return TRUE;
}

gboolean video_glx_blank( int width, int height, uint32_t colour )
{
    glDrawBuffer( GL_FRONT );
    glViewport( 0, 0, width, height );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( 0, width, height, 0, 0, -65535 );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glColor3b( (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF );
    glRecti(0,0, width, height );
    glFlush();
    return TRUE;
}

void video_glx_swap_buffers( void )
{
    glXSwapBuffers( video_x11_display, glx_window );
}

void video_glx_create_pixmap( int width, int height )
{

}
