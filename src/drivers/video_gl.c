/**
 * $Id$
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

#include "display.h"
#include "pvr2/pvr2.h"
#include "drivers/video_gl.h"

extern uint32_t video_width, video_height;

/**
 * Reset the gl state to simple orthographic projection with 
 * texturing, alpha/depth/scissor/cull tests disabled.
 */
void gl_reset_state()
{
    glViewport( 0, 0, video_width, video_height );
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho( 0, video_width, video_height, 0, 0, 65535 );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable( GL_BLEND );
    glDisable( GL_TEXTURE_2D );
    glDisable( GL_TEXTURE_RECTANGLE_ARB );
    glDisable( GL_ALPHA_TEST );
    glDisable( GL_DEPTH_TEST );
    glDisable( GL_SCISSOR_TEST );
    glDisable( GL_CULL_FACE );
}

void gl_display_render_buffer( render_buffer_t buffer )
{
    gl_texture_window( buffer->width, buffer->height, buffer->buf_id, buffer->inverted );
}

/**
 * Convert window coordinates to dreamcast device coords (640x480) using the 
 * same viewable area as gl_texture_window.
 * If the coordinates are outside the viewable area, the result is -1,-1.
 */ 
void gl_window_to_system_coords( int *x, int *y )
{
    int x1=0,y1=0,x2=video_width,y2=video_height;

    int ah = video_width * 0.75;

    if( ah > video_height ) {
        int w = (video_height/0.75);
        x1 = (video_width - w)/2;
        x2 -= x1;
    } else if( ah < video_height ) {
        y1 = (video_height - ah)/2;
        y2 -= y1;
    }
    if( *x < x1 || *x >= x2 || *y < y1 || *y >= y2 ) {
        *x = -1;
        *y = -1;
    } else {
        *x = (*x - x1) * DISPLAY_WIDTH / (x2-x1);
        *y = (*y - y1) * DISPLAY_HEIGHT / (y2-y1);
    }
}

void gl_texture_window( int width, int height, int tex_id, gboolean inverted )
{
    float top, bottom;
    if( inverted ) {
        top = ((float)height);
        bottom = 0;
    } else {
        top = 0;
        bottom = ((float)height);
    }

    /* Reset display parameters */
    gl_reset_state();
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
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, tex_id );
    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glEnable( GL_BLEND );
    glBlendFunc( GL_ONE, GL_ZERO );
    glBegin( GL_QUADS );
    glTexCoord2f( 0, top );
    glVertex2f( x1, y1 );
    glTexCoord2f( ((float)width), top );
    glVertex2f( x2, y1 );
    glTexCoord2f( ((float)width), bottom );
    glVertex2f( x2, y2 );
    glTexCoord2f( 0, bottom );
    glVertex2f( x1, y2 );
    glEnd();
    glDisable( GL_TEXTURE_RECTANGLE_ARB );
    glFlush();
}

gboolean gl_load_frame_buffer( frame_buffer_t frame, int tex_id )
{
    GLenum type = colour_formats[frame->colour_format].type;
    GLenum format = colour_formats[frame->colour_format].format;
    int bpp = colour_formats[frame->colour_format].bpp;
    int rowstride = (frame->rowstride / bpp) - frame->width;

    glPixelStorei( GL_UNPACK_ROW_LENGTH, rowstride );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, tex_id );
    glTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 0,0,
                     frame->width, frame->height, format, type, frame->data );
    return TRUE;
}

void gl_display_blank( uint32_t colour )
{
    gl_reset_state();
    glColor3ub( (colour >> 16) & 0xFF, (colour >> 8) & 0xFF, colour & 0xFF );
    glRecti(0,0, video_width, video_height );
    glFlush();
}

/**
 * Generic GL read_render_buffer. This function assumes that the caller
 * has already set the appropriate glReadBuffer(); in other words, unless
 * there's only one buffer this needs to be wrapped.
 */
gboolean gl_read_render_buffer( unsigned char *target, render_buffer_t buffer, 
                                int rowstride, int colour_format ) 
{
    glFinish();
    GLenum type = colour_formats[colour_format].type;
    GLenum format = colour_formats[colour_format].format;
    // int line_size = buffer->width * colour_formats[colour_format].bpp;
    // int size = line_size * buffer->height;
    int glrowstride = (rowstride / colour_formats[colour_format].bpp) - buffer->width;
    glPixelStorei( GL_PACK_ROW_LENGTH, glrowstride );

    glReadPixels( 0, 0, buffer->width, buffer->height, format, type, target );
    return TRUE;
}
