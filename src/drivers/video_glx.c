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

#include <stdlib.h>
#include <string.h>
#include "display.h"
#include <X11/Xlib.h>
#include <GL/glx.h>
#include "pvr2/pvr2.h"
#include "pvr2/glutil.h"
#include "drivers/video_glx.h"
#include "drivers/video_gl.h"

/**
 * General X11 parameters. The front-end driver is expected to set this up
 * by calling video_glx_init after initializing itself.
 */
Display *video_x11_display = NULL;
Window video_x11_window = 0;

static int glx_version = 100;
static XVisualInfo *glx_visual = NULL;
static GLXFBConfig glx_fbconfig;
static GLXContext glx_context = NULL;
static gboolean glx_is_initialized = FALSE;
static gboolean glx_fbconfig_supported = FALSE;
static gboolean glx_pbuffer_supported = FALSE;
static GLuint glx_pbuffer_texture = 0; 
static int glx_depth_bits = 0;

static void video_glx_swap_buffers( void );
static void video_glx_print_info( FILE *out );

/* Prototypes for pbuffer support methods */
static void glx_pbuffer_init( display_driver_t driver );
static render_buffer_t glx_pbuffer_create_render_buffer( uint32_t width, uint32_t height, GLuint tex_id );
static void glx_pbuffer_destroy_render_buffer( render_buffer_t buffer );
static gboolean glx_pbuffer_set_render_target( render_buffer_t buffer );
static void glx_pbuffer_finish_render( render_buffer_t buffer );
static void glx_pbuffer_display_render_buffer( render_buffer_t buffer );
static void glx_pbuffer_load_frame_buffer( frame_buffer_t frame, render_buffer_t buffer );
static void glx_pbuffer_display_blank( uint32_t colour );
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
    int glx_major, glx_minor, glx_error;

    if( glx_is_initialized ) {
        return TRUE;
    }

    Bool result = XQueryExtension( display, "GLX", &glx_major, &glx_minor, &glx_error ) &&
                  glXQueryVersion( display, &major, &minor );
    if( result == False ) {
        ERROR( "GLX not supported on display" );
        return FALSE;
    }
    
    glx_version = (major*100) + minor;

#ifdef APPLE_BUILD
    /* fbconfig is broken on at least the 10.5 GLX implementation */
    glx_fbconfig_supported = FALSE;
#else
    glx_fbconfig_supported = (glx_version >= 103 || 
            isServerGLXExtensionSupported(display, screen,
                    "GLX_SGIX_fbconfig") );
#endif
    glx_pbuffer_supported = (glx_version >= 103 ||
            isServerGLXExtensionSupported(display, screen,
                    "GLX_SGIX_pbuffer") );
//    glx_fbconfig_supported = FALSE;
    if( glx_fbconfig_supported ) {
        int nelem;
        glx_depth_bits = 24;
        int fb_attribs[] = { GLX_DRAWABLE_TYPE, 
                GLX_PBUFFER_BIT|GLX_WINDOW_BIT, 
                GLX_RENDER_TYPE, GLX_RGBA_BIT, 
                GLX_DEPTH_SIZE, 24, 
                GLX_STENCIL_SIZE, 8, 0 };
        GLXFBConfig *configs = glXChooseFBConfig( display, screen, 
                fb_attribs, &nelem );

        if( configs == NULL || nelem == 0 ) {
            /* Try a 16-bit depth buffer and see if it helps */
            fb_attribs[5] = 16;
            glx_depth_bits = 16;
            configs = glXChooseFBConfig( display, screen, fb_attribs, &nelem );
            if( nelem > 0 ) {
                WARN( "Using a 16-bit depth buffer - expect video glitches" );
            }

        }
        if( configs == NULL || nelem == 0 ) {
            /* Still didn't work. Fallback to 1.2 methods */
            glx_fbconfig_supported = FALSE;
            glx_pbuffer_supported = FALSE;
        } else {
            glx_fbconfig = configs[0];
            glx_visual = glXGetVisualFromFBConfig(display, glx_fbconfig);
            XFree(configs);
        }
    }

    if( !glx_fbconfig_supported ) {
        glx_depth_bits = 24;
        int attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_STENCIL_SIZE, 8, 0 };
        glx_visual = glXChooseVisual( display, screen, attribs );
        if( glx_visual == NULL ) {
            /* Try the 16-bit fallback here too */
            glx_depth_bits = 16;
            attribs[2] = 16;
            glx_visual = glXChooseVisual( display, screen, attribs );
            if( glx_visual != NULL ) {
                WARN( "Using a 16-bit depth buffer - expect video glitches" );
            }
        }
    }

    if( glx_visual == NULL ) {
        return FALSE;
    }

    glx_is_initialized = TRUE;
    return TRUE;
}

static void video_glx_print_info( FILE *out )
{
    XWindowAttributes attr;

    if( !XGetWindowAttributes(video_x11_display, video_x11_window, &attr) )
        return; /* Failed */
    int screen = XScreenNumberOfScreen(attr.screen);

    fprintf( out, "GLX Server: %s %s\n", glXQueryServerString(video_x11_display, screen, GLX_VENDOR),
            glXQueryServerString(video_x11_display, screen, GLX_VERSION) );
    fprintf( out, "GLX Client: %s %s\n", glXGetClientString(video_x11_display, GLX_VENDOR),
            glXGetClientString(video_x11_display, GLX_VERSION) );
    fprintf( out, "GLX Server Extensions:\n" );
    fprint_extensions( out, glXQueryServerString(video_x11_display, screen, GLX_EXTENSIONS) );
    fprintf( out, "GLX Client Extensions:\n" );
    fprint_extensions( out, glXGetClientString(video_x11_display, GLX_EXTENSIONS) );
    fprintf( out, "GLX Extensions:\n" );
    fprint_extensions( out, glXQueryExtensionsString(video_x11_display, screen) );

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

        if( glXMakeCurrent( display, window,
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

    video_x11_display = display;
    video_x11_window = window;

    return TRUE;
}

gboolean video_glx_init_driver( display_driver_t driver )
{
    driver->swap_buffers = video_glx_swap_buffers;
    driver->print_info = video_glx_print_info;
    driver->capabilities.has_gl = TRUE;
    driver->capabilities.depth_bits = glx_depth_bits;
    if( !gl_init_driver(driver, !glx_pbuffer_supported) ) {
        video_glx_shutdown();
        return FALSE;
    }
    if( driver->create_render_buffer == NULL ) {
        /* If we get here, pbuffers are supported and FBO didn't work */
        glx_pbuffer_init(driver);
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


static void video_glx_swap_buffers( void )
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
    GLint stencil_bits = 0;
    
    /* Retrieve the number of stencil bits */
    glGetIntegerv( GL_STENCIL_BITS, &stencil_bits );
    driver->capabilities.stencil_bits = stencil_bits;
    
    glGenTextures( 1, &glx_pbuffer_texture );
    driver->create_render_buffer = glx_pbuffer_create_render_buffer;
    driver->destroy_render_buffer = glx_pbuffer_destroy_render_buffer;
    driver->set_render_target = glx_pbuffer_set_render_target;
    driver->finish_render = glx_pbuffer_finish_render;
    driver->display_render_buffer = glx_pbuffer_display_render_buffer;
    driver->load_frame_buffer = glx_pbuffer_load_frame_buffer;
    driver->display_blank = glx_pbuffer_display_blank;
    driver->read_render_buffer = glx_pbuffer_read_render_buffer;
}

void glx_pbuffer_shutdown()
{
    glDeleteTextures( 1, &glx_pbuffer_texture );
}

static render_buffer_t glx_pbuffer_create_render_buffer( uint32_t width, uint32_t height, GLuint tex_id )
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
    buffer->tex_id = tex_id;
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

static void glx_pbuffer_finish_render( render_buffer_t buffer )
{
    glFinish();
    if( buffer->tex_id != 0 ) {
        // The pbuffer should already be the current context, but just in case...
        glXMakeContextCurrent( video_x11_display, (GLXPbuffer)buffer->buf_id, (GLXPbuffer)buffer->buf_id, glx_context );
        glBindTexture( GL_TEXTURE_2D, buffer->tex_id );
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, buffer->width, buffer->height, 0 );
    }
}
    

/**
 * Render the texture holding the given buffer to the front window
 * buffer.
 */
static void glx_pbuffer_display_render_buffer( render_buffer_t buffer )
{
    glFinish();
    glReadBuffer( GL_FRONT );
    glXMakeContextCurrent( video_x11_display, (GLXPbuffer)buffer->buf_id, (GLXPbuffer)buffer->buf_id, glx_context );
    glBindTexture( GL_TEXTURE_2D, glx_pbuffer_texture );
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, buffer->width, buffer->height, 0 );
    video_glx_make_window_current();
    gl_texture_window( buffer->width, buffer->height, glx_pbuffer_texture, buffer->inverted );
}

static void glx_pbuffer_load_frame_buffer( frame_buffer_t frame, render_buffer_t buffer )
{
    glFinish();
    glXMakeContextCurrent( video_x11_display, (GLXPbuffer)buffer->buf_id, (GLXPbuffer)buffer->buf_id, glx_context );
    GLenum type = colour_formats[frame->colour_format].type;
    GLenum format = colour_formats[frame->colour_format].format;
    int bpp = colour_formats[frame->colour_format].bpp;
    int rowstride = (frame->rowstride / bpp) - frame->width;

    gl_framebuffer_setup();
    glPixelStorei( GL_UNPACK_ROW_LENGTH, rowstride );
    glRasterPos2f(0.375, frame->height-0.375);
    glPixelZoom( 1.0, 1.0 );
    glDrawPixels( frame->width, frame->height, format, type, frame->data );
    glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
    glFlush();
    gl_framebuffer_cleanup();
}

static void glx_pbuffer_display_blank( uint32_t colour )
{
    glFinish();
    video_glx_make_window_current();
    gl_display_blank( colour );
}

static gboolean glx_pbuffer_read_render_buffer( unsigned char *target, render_buffer_t buffer, 
                                                int rowstride, int format )
{
    glXMakeCurrent( video_x11_display, (GLXDrawable)buffer->buf_id, glx_context );
    glReadBuffer( GL_FRONT );
    return gl_read_render_buffer( target, buffer, rowstride, format );
}


