/**
 * $Id$
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
#include <GL/gl.h>
#include "dream.h"
#include "pvr2/pvr2.h"
#include "drivers/video_glx.h"
#include "drivers/gl_common.h"

/**
 * General X11 parameters. The front-end driver is expected to set this up
 * by calling video_glx_init after initializing itself.
 */
Display *video_x11_display = NULL;
Window video_x11_window = 0;
static gboolean glsl_loaded = FALSE;

static int glx_version = 100;
static XVisualInfo *glx_visual;
static GLXFBConfig glx_fbconfig;
static GLXContext glx_context = NULL;
static gboolean glx_is_initialized = FALSE;
static gboolean glx_fbconfig_supported = FALSE;
static gboolean glx_pbuffer_supported = FALSE;
static int glx_pbuffer_texture = 0; 

/* Prototypes for pbuffer support methods */
static void glx_pbuffer_init( display_driver_t driver );
static render_buffer_t glx_pbuffer_create_render_buffer( uint32_t width, uint32_t height );
static void glx_pbuffer_destroy_render_buffer( render_buffer_t buffer );
static gboolean glx_pbuffer_set_render_target( render_buffer_t buffer );
static gboolean glx_pbuffer_display_render_buffer( render_buffer_t buffer );
static void glx_pbuffer_load_frame_buffer( frame_buffer_t frame, render_buffer_t buffer );
static gboolean glx_pbuffer_display_blank( uint32_t colour );
static gboolean glx_pbuffer_read_render_buffer( unsigned char *target, render_buffer_t buffer, int rowstride, int format );

/**
 * Test if a specific extension is supported. From opengl.org
 * @param extension extension name to check for
 * @return TRUE if supported, otherwise FALSE.
 */
gboolean isServerGLXExtensionSupported( Display *display, int screen, 
					const char *extension )
{
    const char *extensions = NULL;
    const char *start;
    char *where, *terminator;

    /* Extension names should not have spaces. */
    where = strchr(extension, ' ');
    if (where || *extension == '\0')
	return 0;
    extensions = glXQueryServerString(display, screen, GLX_EXTENSIONS);
    start = extensions;
    for (;;) {
	where = strstr((const char *) start, extension);
	if (!where)
	    break;
	terminator = where + strlen(extension);
	if (where == start || *(where - 1) == ' ')
	    if (*terminator == ' ' || *terminator == '\0')
		return TRUE;
	start = terminator;
    }
    return FALSE;
}

gboolean video_glx_init( Display *display, int screen )
{
    int major, minor;

    if( glx_is_initialized ) {
        return TRUE;
    }

    Bool result = glXQueryVersion( display, &major, &minor );
    if( result != False ) {
        glx_version = (major*100) + minor;
    }
    
    glx_fbconfig_supported = (glx_version >= 103 || 
			      isServerGLXExtensionSupported(display, screen,
						      "GLX_SGIX_fbconfig") );
    glx_pbuffer_supported = (glx_version >= 103 ||
			     isServerGLXExtensionSupported(display, screen,
						     "GLX_SGIX_pbuffer") );
    
    if( glx_fbconfig_supported ) {
	int nelem;
        int fb_attribs[] = { GLX_DRAWABLE_TYPE, 
			     GLX_PBUFFER_BIT|GLX_WINDOW_BIT, 
			     GLX_RENDER_TYPE, GLX_RGBA_BIT, 
			     GLX_DEPTH_SIZE, 24, 0 };
	GLXFBConfig *configs = glXChooseFBConfig( display, screen, 
						  fb_attribs, &nelem );

	if( configs == NULL || nelem == 0 ) {
	    /* Didn't work. Fallback to 1.2 methods */
	    glx_fbconfig_supported = FALSE;
	    glx_pbuffer_supported = FALSE;
	} else {
	    glx_fbconfig = configs[0];
	    glx_visual = glXGetVisualFromFBConfig(display, glx_fbconfig);
	    XFree(configs);
	}
    }

    if( !glx_fbconfig_supported ) {
        int attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, 0 };
	glx_visual = glXChooseVisual( display, screen, attribs );
    }
    glx_is_initialized = TRUE;
    return TRUE;
}

gboolean video_glx_init_context( Display *display, Window window )
{
    if( glx_fbconfig_supported ) {
        glx_context = glXCreateNewContext( display, glx_fbconfig, 
					   GLX_RGBA_TYPE, NULL, True );
	if( glx_context == NULL ) {
	    ERROR( "Unable to create a GLX Context.");
	    return FALSE;
	}

	if( glXMakeContextCurrent( display, window, window, 
				   glx_context ) == False ) {
	    ERROR( "Unable to prepare GLX context for drawing" );
	    glXDestroyContext( display, glx_context );
	    return FALSE;
	}
    } else {
        glx_context = glXCreateContext( display, glx_visual, None, True );
	if( glx_context == NULL ) {
	    ERROR( "Unable to create a GLX Context.");
	    return FALSE;
	}
	
	if( glXMakeCurrent( display, window, glx_context ) == False ) {
	    ERROR( "Unable to prepare GLX context for drawing" );
	    glXDestroyContext( display, glx_context );
	    return FALSE;
	}
    }

    if( !glXIsDirect(display, glx_context) ) {
    	WARN( "Not using direct rendering - this is likely to be slow" );
    }

    texcache_gl_init();
    video_x11_display = display;
    video_x11_window = window;

    return TRUE;
}

gboolean video_glx_init_driver( display_driver_t driver )
{
    if( gl_fbo_is_supported() ) { // First preference
	gl_fbo_init(driver);
    } else if( glx_pbuffer_supported ) {
	glx_pbuffer_init(driver);
    } else {
        ERROR( "Unable to create render buffers (requires either EXT_framebuffer_object or GLX 1.3+)" );
        video_glx_shutdown();
	return FALSE;
    }
    return TRUE;
}


void video_glx_shutdown()
{
  //    texcache_gl_shutdown();
    glx_is_initialized = FALSE;
    if( glx_context != NULL ) {
        glXDestroyContext( video_x11_display, glx_context );
	glx_context = NULL;
    }
    if( glx_visual != NULL ) {
        XFree(glx_visual);
	glx_visual = NULL;
    }
}


XVisualInfo *video_glx_get_visual()
{
    return glx_visual;
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
    glXSwapBuffers( video_x11_display, video_x11_window );
}

void video_glx_make_window_current( void )
{
    glXMakeCurrent( video_x11_display, video_x11_window, glx_context );
}


// Pbuffer support

/**
 * Construct the initial frame buffers and allocate ids for everything.
 * The render handling driver methods are set to the fbo versions.
 */
static void glx_pbuffer_init( display_driver_t driver ) 
{
    glGenTextures( 1, &glx_pbuffer_texture );
    driver->create_render_buffer = glx_pbuffer_create_render_buffer;
    driver->destroy_render_buffer = glx_pbuffer_destroy_render_buffer;
    driver->set_render_target = glx_pbuffer_set_render_target;
    driver->display_render_buffer = glx_pbuffer_display_render_buffer;
    driver->load_frame_buffer = glx_pbuffer_load_frame_buffer;
    driver->display_blank = glx_pbuffer_display_blank;
    driver->read_render_buffer = glx_pbuffer_read_render_buffer;
}

void glx_pbuffer_shutdown()
{
    glDeleteTextures( 1, &glx_pbuffer_texture );
}

static render_buffer_t glx_pbuffer_create_render_buffer( uint32_t width, uint32_t height )
{
    int attribs[] = { GLX_PBUFFER_WIDTH, width, GLX_PBUFFER_HEIGHT, height,
                      GLX_PRESERVED_CONTENTS, True, 0 };
    GLXPbuffer pb = glXCreatePbuffer( video_x11_display, glx_fbconfig, attribs );
    if( pb == (GLXPbuffer)NULL ) {
	ERROR( "Unable to create pbuffer" );
	return NULL;
    }
    render_buffer_t buffer = calloc( sizeof(struct render_buffer), 1 );
    buffer->width = width;
    buffer->height = height;
    buffer->buf_id = pb;
    return buffer;
}

static void glx_pbuffer_destroy_render_buffer( render_buffer_t buffer )
{
    glXDestroyPbuffer( video_x11_display, (GLXPbuffer)buffer->buf_id );
    buffer->buf_id = 0;
    free( buffer );
}

static gboolean glx_pbuffer_set_render_target( render_buffer_t buffer )
{
    glFinish();
    if( glXMakeContextCurrent( video_x11_display, (GLXPbuffer)buffer->buf_id, (GLXPbuffer)buffer->buf_id, glx_context ) == False ) {
	ERROR( "Make context current (pbuffer) failed!" );
    }
    /* setup the gl context */
    glViewport( 0, 0, buffer->width, buffer->height );
    glDrawBuffer(GL_FRONT);
    
    return TRUE;
}

/**
 * Render the texture holding the given buffer to the front window
 * buffer.
 */
static gboolean glx_pbuffer_display_render_buffer( render_buffer_t buffer )
{
    glFinish();
    glReadBuffer( GL_FRONT );
    glDrawBuffer( GL_FRONT );
    glXMakeContextCurrent( video_x11_display, (GLXPbuffer)buffer->buf_id, (GLXPbuffer)buffer->buf_id, glx_context );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, glx_pbuffer_texture );
    glCopyTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA, 0, 0, buffer->width, buffer->height, 0 );
    video_glx_make_window_current();
    gl_texture_window( buffer->width, buffer->height, glx_pbuffer_texture, buffer->inverted );
    return TRUE;
}

static void glx_pbuffer_load_frame_buffer( frame_buffer_t frame, render_buffer_t buffer )
{
    glFinish();
    glXMakeContextCurrent( video_x11_display, (GLXPbuffer)buffer->buf_id, (GLXPbuffer)buffer->buf_id, glx_context );
    GLenum type = colour_formats[frame->colour_format].type;
    GLenum format = colour_formats[frame->colour_format].format;
    int bpp = colour_formats[frame->colour_format].bpp;
    int rowstride = (frame->rowstride / bpp) - frame->width;
    
    gl_reset_state();
    glPixelStorei( GL_UNPACK_ROW_LENGTH, rowstride );
    glRasterPos2f(0.375, frame->height-0.375);
    glPixelZoom( 1.0, 1.0 );
    glDrawPixels( frame->width, frame->height, format, type, frame->data );
    glFlush();
}

static gboolean glx_pbuffer_display_blank( uint32_t colour )
{
    glFinish();
    video_glx_make_window_current();
    return gl_display_blank( colour );
}

static gboolean glx_pbuffer_read_render_buffer( unsigned char *target, render_buffer_t buffer, 
					       int rowstride, int format )
{
    glXMakeCurrent( video_x11_display, (GLXDrawable)buffer->buf_id, glx_context );
    glReadBuffer( GL_FRONT );
    return gl_read_render_buffer( target, buffer, rowstride, format );
}


