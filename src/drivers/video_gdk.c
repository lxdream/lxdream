/**
 * $Id$
 *
 * The PC side of the video support (responsible for actually displaying / 
 * rendering frames)
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
#include <assert.h>
#include <stdlib.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkwidget.h>
#include <GL/glu.h>
#include <GL/osmesa.h>
#include "lxdream.h"
#include "display.h"

#define MAX_PIXBUF 16

extern GtkWidget *gtk_video_drawable;
extern int video_width, video_height;

static render_buffer_t gdk_pixbuf_create_render_buffer( uint32_t width, uint32_t height );
static void gdk_pixbuf_destroy_render_buffer( render_buffer_t buffer );
static gboolean gdk_pixbuf_set_render_target( render_buffer_t buffer );
static void gdk_pixbuf_display_render_buffer( render_buffer_t buffer );
static void gdk_pixbuf_load_frame_buffer( frame_buffer_t frame, render_buffer_t buffer );
static void gdk_pixbuf_display_blank( uint32_t colour );
static gboolean gdk_pixbuf_read_render_buffer( unsigned char *target, render_buffer_t buffer, int rowstride, int format );

static void *pixbuf_array[MAX_PIXBUF];
unsigned int pixbuf_max = 0;
OSMesaContext osmesa_context = NULL;

void video_gdk_init_driver( display_driver_t driver )
{
    pixbuf_max = 0;
    driver->create_render_buffer = gdk_pixbuf_create_render_buffer;
    driver->destroy_render_buffer = gdk_pixbuf_destroy_render_buffer;
    driver->set_render_target = gdk_pixbuf_set_render_target;
    driver->display_render_buffer = gdk_pixbuf_display_render_buffer;
    driver->load_frame_buffer = gdk_pixbuf_load_frame_buffer;
    driver->display_blank = gdk_pixbuf_display_blank;
    driver->read_render_buffer = gdk_pixbuf_read_render_buffer;

    osmesa_context = OSMesaCreateContextExt( OSMESA_RGBA, 32, 0, 0, 0 );
    OSMesaMakeCurrent( osmesa_context, NULL, GL_UNSIGNED_BYTE, 640, 480 );
    pvr2_setup_gl_context();
}

int video_gdk_find_free()
{
    unsigned int i;
    for( i=0; i<pixbuf_max; i++ ) {
        if( pixbuf_array[i] == NULL ) {
            return i;
        }
    }
    if( i < MAX_PIXBUF ) {
        return pixbuf_max++;
    }
    return -1;
}

void video_gdk_shutdown()
{
    unsigned int i;
    for( i=0; i<pixbuf_max; i++ ) {
        if( pixbuf_array[i] != NULL ) {
            g_free(pixbuf_array[i]);
            pixbuf_array[i] = NULL;
        }
    }
    pixbuf_max = 0;
    OSMesaDestroyContext( osmesa_context );
}


static render_buffer_t gdk_pixbuf_create_render_buffer( uint32_t width, uint32_t height )
{
    render_buffer_t buf = g_malloc0(sizeof(struct render_buffer));
    gboolean alpha = FALSE;
    buf->width = width;
    buf->height = height;
    buf->buf_id = video_gdk_find_free();
    pixbuf_array[buf->buf_id] = g_malloc0( width * height * 4 );
    return buf;
}

static void gdk_pixbuf_destroy_render_buffer( render_buffer_t buffer )
{
    g_free(pixbuf_array[buffer->buf_id] );
    pixbuf_array[buffer->buf_id] = NULL;
    if( buffer->buf_id == (pixbuf_max-1) ) {
        pixbuf_max--;
    }
}

static void gdk_pixbuf_display_render_buffer( render_buffer_t buffer )
{
    glFinish();

    void *pb = pixbuf_array[buffer->buf_id];
    GdkGC *gc = gtk_video_drawable->style->fg_gc[GTK_STATE_NORMAL];
    GdkColor black = {0,0,0,0};

    assert(gc);

    int x1=0,y1=0,x2=video_width,y2=video_height;

    int ah = video_width * 0.75;


    if( ah > video_height ) {
        int w = (video_height/0.75);
        x1 = (video_width - w)/2;
        x2 -= x1;
        gdk_gc_set_foreground( gc, &black );
        gdk_gc_set_background( gc, &black );
        gdk_draw_rectangle( gtk_video_drawable->window, gc, TRUE, 0, 0, x1, video_height );
        gdk_draw_rectangle( gtk_video_drawable->window, gc, TRUE, x2, 0, video_width, video_height );
    } else if( ah < video_height ) {
        y1 = (video_height - ah)/2;
        y2 -= y1;
        gdk_gc_set_foreground( gc, &black );
        gdk_gc_set_background( gc, &black );
        gdk_draw_rectangle( gtk_video_drawable->window, gc, TRUE, 0, 0, video_width, y1 );
        gdk_draw_rectangle( gtk_video_drawable->window, gc, TRUE, 0, y2, video_width, video_height );
    }
    int w = x2-x1;
    int h = y2-y1;

    if( w != buffer->width || h != buffer->height ) {
        gdk_draw_rgb_32_image( gtk_video_drawable->window, gc, x1, y1, buffer->width, buffer->height, GDK_RGB_DITHER_NONE,
                pb, buffer->width*4 );
    } else {
        gdk_draw_rgb_32_image( gtk_video_drawable->window, gc, x1, y1, buffer->width, buffer->height, GDK_RGB_DITHER_NONE,
                pb, buffer->width*4 );
    }
}

static void gdk_pixbuf_display_blank( uint32_t colour )
{
    GdkGC *gc = gtk_video_drawable->style->fg_gc[GTK_STATE_NORMAL];
    GdkColor col = { };

    gdk_gc_set_foreground( gc, &col );
    gdk_gc_set_background( gc, &col );
    gdk_draw_rectangle( gtk_video_drawable->window, gc, TRUE, 0, 0, video_width, video_height );
}

static gboolean gdk_pixbuf_set_render_target( render_buffer_t buffer )
{
    glFinish();
    void *pb = pixbuf_array[buffer->buf_id];
    OSMesaMakeCurrent( osmesa_context, pb, GL_UNSIGNED_BYTE,
                       buffer->width, buffer->height );
    //OSMesaPixelStore( OSMESA_Y_UP, 0 );
    glViewport( 0, 0, buffer->width, buffer->height );
    glDrawBuffer(GL_FRONT);
    return TRUE;
}

static void gdk_pixbuf_load_frame_buffer( frame_buffer_t frame, render_buffer_t buffer )
{
    glFinish();
    void *pb = pixbuf_array[buffer->buf_id];
    OSMesaMakeCurrent( osmesa_context, pb, GL_UNSIGNED_BYTE,
                       buffer->width, buffer->height );
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

static gboolean gdk_pixbuf_read_render_buffer( unsigned char *target, render_buffer_t buffer, int rowstride, int format )
{
    glFinish();
    void *pb = pixbuf_array[buffer->buf_id];
    OSMesaMakeCurrent( osmesa_context, pb, GL_UNSIGNED_BYTE,
                       buffer->width, buffer->height );
    glReadBuffer( GL_FRONT );
    return gl_read_render_buffer( target, buffer, rowstride, format );

}
