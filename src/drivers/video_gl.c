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
#include "pvr2/glutil.h"
#include "pvr2/shaders.h"
#include "drivers/video_gl.h"

/* FIXME: Need to actually handle this case */
#ifndef GL_PACK_ROW_LENGTH
#define glPixelStorei(key,val)
#endif

uint32_t video_width, video_height;
struct video_vertex {
    float x,y;
    float u,v;
    float r,g,b,a;
};

static struct video_box_t {
    float viewMatrix[16];
    struct video_vertex gap1[4];
    struct video_vertex gap2[4];
    struct video_vertex video_view[4];
    struct video_vertex invert_view[4];
} video_box;

void gl_set_video_size( uint32_t width, uint32_t height )
{
    video_width = width;
    video_height = height;

    int x1=0,y1=0,x2=video_width,y2=video_height;

    int ah = video_width * 0.75;

    if( ah > video_height ) {
        int w = (video_height/0.75);
        x1 = (video_width - w)/2;
        x2 -= x1;
        video_box.gap1[0].x = 0; video_box.gap1[0].y = 0;
        video_box.gap1[1].x = x1; video_box.gap1[1].y = 0;
        video_box.gap1[2].x = 0; video_box.gap1[2].y = video_height;
        video_box.gap1[3].x = x2; video_box.gap1[3].y = video_height;
        video_box.gap2[0].x = x2; video_box.gap2[0].y = 0;
        video_box.gap2[1].x = video_width; video_box.gap2[1].y = 0;
        video_box.gap2[2].x = x2; video_box.gap2[2].y = video_height;
        video_box.gap2[3].x = video_width; video_box.gap2[3].y = video_height;
    } else if( ah < video_height ) {
        y1 = (video_height - ah)/2;
        y2 -= y1;

        video_box.gap1[0].x = 0; video_box.gap1[0].y = 0;
        video_box.gap1[1].x = video_width; video_box.gap1[1].y = 0;
        video_box.gap1[2].x = 0; video_box.gap1[2].y = y1;
        video_box.gap1[3].x = video_width; video_box.gap1[3].y = y1;
        video_box.gap2[0].x = 0; video_box.gap2[0].y = y2;
        video_box.gap2[1].x = video_width; video_box.gap2[1].y = y2;
        video_box.gap2[2].x = 0; video_box.gap2[2].y = video_height;
        video_box.gap2[3].x = video_width; video_box.gap2[3].y = video_height;
    }

    video_box.video_view[0].x = x1; video_box.video_view[0].y = y1;
    video_box.video_view[0].u = 0; video_box.video_view[0].v = 0;
    video_box.video_view[1].x = x2; video_box.video_view[1].y = y1;
    video_box.video_view[1].u = 1; video_box.video_view[1].v = 0;
    video_box.video_view[2].x = x1; video_box.video_view[2].y = y2;
    video_box.video_view[2].u = 0; video_box.video_view[2].v = 1;
    video_box.video_view[3].x = x2; video_box.video_view[3].y = y2;
    video_box.video_view[3].u = 1; video_box.video_view[3].v = 1;

    memcpy( &video_box.invert_view, &video_box.video_view, sizeof(video_box.video_view) );
    video_box.invert_view[0].v = 1; video_box.invert_view[1].v = 1;
    video_box.invert_view[2].v = 0; video_box.invert_view[3].v = 0;

    defineOrthoMatrix(video_box.viewMatrix, video_width, video_height, 0, 65535);
}

#ifdef HAVE_OPENGL_FIXEDFUNC
/**
 * Setup the gl context for writes to the display output.
 */
void gl_framebuffer_setup()
{
    glViewport( 0, 0, video_width, video_height );
    glLoadMatrixf(video_box.viewMatrix);
    glBlendFunc( GL_ONE, GL_ZERO );
    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
    glVertexPointer(2, GL_FLOAT, sizeof(struct video_vertex), &video_box.gap1[0].x);
    glColorPointer(3, GL_FLOAT, sizeof(struct video_vertex), &video_box.gap1[0].r);
    glTexCoordPointer(2, GL_FLOAT, sizeof(struct video_vertex), &video_box.gap1[0].u);
    glEnableClientState( GL_VERTEX_ARRAY );
    glEnableClientState( GL_COLOR_ARRAY );
    glEnableClientState( GL_TEXTURE_COORD_ARRAY );
}

void gl_framebuffer_cleanup()
{
    glDisableClientState( GL_VERTEX_ARRAY );
    glDisableClientState( GL_COLOR_ARRAY );
    glDisableClientState( GL_TEXTURE_COORD_ARRAY );
}
#else
void gl_framebuffer_setup()
{
    glViewport( 0, 0, video_width, video_height );
    glBlendFunc( GL_ONE, GL_ZERO );
    glsl_use_basic_shader();
    glsl_set_basic_shader_view_matrix(video_box.viewMatrix);
    glsl_set_basic_shader_in_vertex_pointer(&video_box.gap1[0].x, sizeof(struct video_vertex));
    glsl_set_basic_shader_in_colour_pointer(&video_box.gap1[0].r, sizeof(struct video_vertex));
    glsl_set_basic_shader_in_texcoord_pointer(&video_box.gap1[0].u, sizeof(struct video_vertex));
    glsl_set_basic_shader_primary_texture(0);
}

void gl_framebuffer_cleanup()
{
    glsl_clear_shader();
}
#endif

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
    /* Set video box tex alpha to 1 */
    video_box.video_view[0].a = video_box.video_view[1].a = video_box.video_view[2].a = video_box.video_view[3].a = 1;

    /* Reset display parameters */
    gl_framebuffer_setup();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glDrawArrays(GL_TRIANGLE_STRIP, inverted ? 12 : 8, 4);
    glDisable(GL_TEXTURE_2D);
    glFlush();
    gl_framebuffer_cleanup();
}

gboolean gl_load_frame_buffer( frame_buffer_t frame, int tex_id )
{
    GLenum type = colour_formats[frame->colour_format].type;
    GLenum format = colour_formats[frame->colour_format].format;
    int bpp = colour_formats[frame->colour_format].bpp;
    int rowstride = (frame->rowstride / bpp) - frame->width;

    glPixelStorei( GL_UNPACK_ROW_LENGTH, rowstride );
    glBindTexture( GL_TEXTURE_2D, tex_id );
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0,0,
                     frame->width, frame->height, format, type, frame->data );
    glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
    return TRUE;
}

void gl_display_blank( uint32_t colour )
{
    /* Set the video_box background colour */
    video_box.video_view[0].r = ((float)(((colour >> 16) & 0xFF) + 1)) / 256.0;
    video_box.video_view[0].g = ((float)(((colour >> 8) & 0xFF) + 1)) / 256.0;
    video_box.video_view[0].b = ((float)((colour & 0xFF) + 1)) / 256.0;
    video_box.video_view[0].a = 0;
    memcpy( &video_box.video_view[1].r, &video_box.video_view[0].r, sizeof(float)*3 );
    memcpy( &video_box.video_view[2].r, &video_box.video_view[0].r, sizeof(float)*3 );
    memcpy( &video_box.video_view[3].r, &video_box.video_view[0].r, sizeof(float)*3 );

    /* And render */
    gl_framebuffer_setup();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);
    glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
    glFlush();
    gl_framebuffer_cleanup();
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
    glPixelStorei( GL_PACK_ROW_LENGTH, 0 );
    return TRUE;
}

static gboolean video_gl_init();

/**
 * Minimal GL driver (assuming that the GL context is already set up externally)
 * This requires FBO support (since otherwise we have no way to get a render buffer)
 */
struct display_driver display_gl_driver = {
        "gl", N_("OpenGL driver"), video_gl_init, NULL,
        NULL, NULL, NULL,
        NULL, NULL, NULL, NULL,
        gl_load_frame_buffer, gl_display_render_buffer, gl_display_blank,
        NULL, gl_read_render_buffer, NULL, NULL
};

static gboolean video_gl_init()
{
     if( gl_fbo_is_supported() ) {
         display_gl_driver.capabilities.has_gl = TRUE;
         gl_fbo_init(&display_gl_driver);
         gl_vbo_init(&display_gl_driver);
         return TRUE;
     } else {
         return FALSE;
     }
}
