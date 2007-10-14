/**
 * $Id: gl_common.c,v 1.5 2007-10-14 09:30:16 nkeynes Exp $
 *
 * Common GL code that doesn't depend on a specific implementation
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

#include <sys/time.h>

#include <GL/gl.h>
#include "dream.h"
#include "drivers/gl_common.h"

extern uint32_t video_width, video_height;
static GLuint frame_last_texid = 0, fbuf_id = 0;
static uint32_t frame_width = 0;
static uint32_t frame_height = 0;
static uint32_t frame_colour = 0;
static gboolean frame_inverted = FALSE;



char *required_extensions[] = { "GL_EXT_framebuffer_object", NULL };

/**
 * Test if a specific extension is supported. From opengl.org
 * @param extension extension name to check for
 * @return TRUE if supported, otherwise FALSE.
 */
gboolean isGLExtensionSupported( const char *extension )
{
    const GLubyte *extensions = NULL;
    const GLubyte *start;
    GLubyte *where, *terminator;

    /* Extension names should not have spaces. */
    where = (GLubyte *) strchr(extension, ' ');
    if (where || *extension == '\0')
	return 0;
    extensions = glGetString(GL_EXTENSIONS);
    /* It takes a bit of care to be fool-proof about parsing the
       OpenGL extensions string. Don't be fooled by sub-strings,
       etc. */
    start = extensions;
    for (;;) {
	where = (GLubyte *) strstr((const char *) start, extension);
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

gboolean hasRequiredGLExtensions( ) 
{
    int i;
    gboolean isOK = TRUE;

    for( i=0; required_extensions[i] != NULL; i++ ) {
	if( !isGLExtensionSupported(required_extensions[i]) ) {
	    ERROR( "Required OpenGL extension not supported: %s", required_extensions[i] );
	    isOK = FALSE;
	}
    }
    return isOK;
}

void gl_frame_buffer_to_tex_rectangle( frame_buffer_t frame, GLuint texid )
{
    GLenum type = colour_formats[frame->colour_format].type;
    GLenum format = colour_formats[frame->colour_format].format;
    int bpp = colour_formats[frame->colour_format].bpp;
    int rowstride = (frame->rowstride / bpp) - frame->width;
    
    glPixelStorei( GL_UNPACK_ROW_LENGTH, rowstride );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texid );
    glTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGB,
		  frame->width, frame->height, 0, format, type, frame->data );
}

void gl_display_tex_rectangle( GLuint texid, uint32_t tex_width, uint32_t tex_height, gboolean invert )
{
    float top, bottom;
    if( invert ) {
	top = ((float)tex_height) - 0.5;
	bottom = 0.5;
    } else {
	top = 0.5;
	bottom = ((float)tex_height) - 0.5;
    }

    /* Reset display parameters */
    glViewport( 0, 0, video_width, video_height );
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho( 0, video_width, video_height, 0, 0, -65535 );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable( GL_TEXTURE_2D );
    glDisable( GL_ALPHA_TEST );
    glDisable( GL_DEPTH_TEST );
    glDisable( GL_SCISSOR_TEST );
    glDisable( GL_CULL_FACE );
    glColor3f( 0,0,0 );
    

    int x1=0,y1=0,x2=video_width,y2=video_height;

    int ah = video_width * 0.75;

    if( ah > video_height ) {
	int w = (video_height/0.75);
	x1 = (video_width - w)/2;
	x2 -= x1;

	glBegin( GL_QUADS );
	glVertex2f( 0, 0 );
	glVertex2f( x1, 0 );
	glVertex2f( x1, video_height );
	glVertex2f( 0, video_height);
	glVertex2f( x2, 0 );
	glVertex2f( video_width, 0 );
	glVertex2f( video_width, video_height );
	glVertex2f( x2, video_height);
	glEnd();
    } else if( ah < video_height ) {
	y1 = (video_height - ah)/2;
	y2 -= y1;
	glBegin( GL_QUADS );
	glVertex2f( 0, 0 );
	glVertex2f( video_width, 0 );
	glVertex2f( video_width, y1 );
	glVertex2f( 0, y1 );
	glVertex2f( 0, y2 );
	glVertex2f( video_width, y2 );
	glVertex2f( video_width, video_height );
	glVertex2f( 0, video_height );
	glEnd();
    }

    /* Render the textured rectangle */
    glEnable( GL_TEXTURE_RECTANGLE_ARB );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texid );
    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glEnable( GL_BLEND );
    glBlendFunc( GL_ONE, GL_ZERO );
    glBegin( GL_QUADS );
    glTexCoord2f( 0.5, top );
    glVertex2f( x1, y1 );
    glTexCoord2f( ((float)tex_width)-0.5, top );
    glVertex2f( x2, y1 );
    glTexCoord2f( ((float)tex_width)-0.5, bottom );
    glVertex2f( x2, y2 );
    glTexCoord2f( 0.5, bottom );
    glVertex2f( x1, y2 );
    glEnd();
    glDisable( GL_TEXTURE_RECTANGLE_ARB );
    glFlush();
    frame_last_texid = texid;
    frame_width = tex_width;
    frame_height = tex_height;
    frame_inverted = invert;
}

gboolean gl_display_frame_buffer( frame_buffer_t frame )
{
    if( fbuf_id == 0 ) {
	glGenTextures( 1, &fbuf_id );
    }
    gl_frame_buffer_to_tex_rectangle( frame, fbuf_id );
    gl_display_tex_rectangle( fbuf_id, frame->width, frame->height, FALSE );
    return TRUE;
}

gboolean gl_display_blank( uint32_t colour )
{
    glViewport( 0, 0, video_width, video_height );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( 0, video_width, video_height, 0, 0, -65535 );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glColor3b( (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF );
    glRecti(0,0, video_width, video_height );
    glFlush();
    frame_colour = colour;
    frame_last_texid = 0;
    return TRUE;
}

void gl_redisplay_last()
{
    if( frame_last_texid == 0 ) {
	gl_display_blank( frame_colour );
    } else {
	gl_display_tex_rectangle( frame_last_texid, frame_width, frame_height, frame_inverted );
    }
}

/**
 * Generic GL read_render_buffer. This function assumes that the caller
 * has already set the appropriate glReadBuffer(); in other words, unless
 * there's only one buffer this needs to be wrapped.
 */
gboolean gl_read_render_buffer( render_buffer_t buffer, unsigned char *target ) 
{
    if( buffer->address == -1 )
	return FALSE;
    glFinish();
    GLenum type = colour_formats[buffer->colour_format].type;
    GLenum format = colour_formats[buffer->colour_format].format;
    // int line_size = buffer->width * colour_formats[buffer->colour_format].bpp;
    // int size = line_size * buffer->height;
    // int rowstride = (buffer->rowstride / colour_formats[buffer->colour_format].bpp) - buffer->width;
    // glPixelStorei( GL_PACK_ROW_LENGTH, rowstride );
    
    glReadPixels( 0, 0, buffer->width, buffer->height, format, type, target );
    return TRUE;
}
