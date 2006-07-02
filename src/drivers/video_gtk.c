/**
 * $Id: video_gtk.c,v 1.8 2006-07-02 04:59:00 nkeynes Exp $
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

#include <gnome.h>
#include <gdk/gdkx.h>
#include <stdint.h>
#include "dream.h"
#include "display.h"
#include "drivers/video_x11.h"

GdkImage *video_img = NULL;
GtkWindow *video_win = NULL;
GtkWidget *video_area = NULL;
uint32_t video_width = 640;
uint32_t video_height = 480;
uint32_t video_frame_count = 0;

uint16_t video_gtk_resolve_keysym( const gchar *keysym );
gboolean video_gtk_set_output_format( uint32_t width, uint32_t height,  
				      int colour_format );
gboolean video_gtk_set_render_format( uint32_t width, uint32_t height,  
				      int colour_format, gboolean texture );
gboolean video_gtk_display_frame( video_buffer_t frame );
gboolean video_gtk_blank( uint32_t rgb );

struct display_driver display_gtk_driver = { "gtk", 
					   NULL,
					   NULL,
					   video_gtk_resolve_keysym,
					   video_gtk_set_output_format,
					   video_gtk_set_render_format,
					   video_gtk_display_frame,
					   video_gtk_blank,
					   video_glx_swap_buffers };

gboolean video_gtk_keydown_callback(GtkWidget       *widget,
				     GdkEventKey     *event,
				     gpointer         user_data)
{
    input_event_keydown( event->keyval );
}

uint16_t video_gtk_resolve_keysym( const gchar *keysym )
{
    int val = gdk_keyval_from_name( keysym );
    if( val == GDK_VoidSymbol )
	return 0;
    return (uint16_t)val;
}

gboolean video_gtk_keyup_callback(GtkWidget       *widget,
				  GdkEventKey     *event,
				  gpointer         user_data)
{
    input_event_keyup( event->keyval );
}

gboolean video_gtk_set_output_format( uint32_t width, uint32_t height,  
				      int colour_format )
{
    video_width = width;
    video_height = height;
    if( video_win == NULL ) {
	video_win = GTK_WINDOW(gtk_window_new( GTK_WINDOW_TOPLEVEL ));
	gtk_window_set_title( video_win, APP_NAME " - Emulation Window" );
	gtk_window_set_policy( video_win, FALSE, FALSE, FALSE );
	gtk_window_set_default_size( video_win, width, height );
    
	g_signal_connect( video_win, "key_press_event", 
			  G_CALLBACK(video_gtk_keydown_callback), NULL );
	g_signal_connect( video_win, "key_release_event", 
			  G_CALLBACK(video_gtk_keyup_callback), NULL );
	gtk_widget_add_events( GTK_WIDGET(video_win), 
			       GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
			       GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK );
	video_area = gtk_image_new();
	gtk_widget_show( GTK_WIDGET(video_area) );
	gtk_container_add( GTK_CONTAINER(video_win), GTK_WIDGET(video_area) );
	gtk_widget_show( GTK_WIDGET(video_win) );
	video_x11_set_display( gdk_x11_display_get_xdisplay( gtk_widget_get_display(video_area)),
			       gdk_x11_screen_get_xscreen( gtk_widget_get_screen(video_area)),
			       GDK_WINDOW_XWINDOW( GTK_WIDGET(video_win)->window ) );
			       
    }
    gtk_window_set_default_size( video_win, width, height );
    video_img = gdk_image_new( GDK_IMAGE_FASTEST, gdk_visual_get_system(),
			       width, height );
    gtk_image_set_from_image( GTK_IMAGE(video_area), video_img, NULL );
    /* Note old image is auto de-refed */
    return TRUE;
}


/**
 * Fill the entire frame with the specified colour (00RRGGBB)
 */
gboolean video_gtk_blank( uint32_t colour ) 
{
    video_glx_blank( video_width, video_height, colour );
}

gboolean video_gtk_display_frame( video_buffer_t frame ) 
{
    uint32_t bytes_per_line, x, y;
    char *src = frame->data;
    char *dest = video_img->mem;

    return video_glx_display_frame( frame );

    switch( frame->colour_format ) {
    case COLFMT_ARGB1555:
	for( y=0; y < frame->vres; y++ ) {
	    uint16_t *p = (uint16_t *)src;
	    for( x=0; x < frame->hres; x++ ) {
		uint16_t pixel = *p++;
		*dest++ = (pixel & 0x1F) << 3;
		*dest++ = (pixel & 0x3E0) >> 2;
		*dest++ = (pixel & 0x7C00) >> 7;
		*dest++ = 0;
	    }
	    src += frame->rowstride;
	}
	break;
    case COLFMT_RGB565:
	for( y=0; y < frame->vres; y++ ) {
	    uint16_t *p = (uint16_t *)src;
	    for( x=0; x < frame->hres; x++ ) {
		uint16_t pixel = *p++;
		*dest++ = (pixel & 0x1F) << 3;
		*dest++ = (pixel & 0x7E0) >> 3;
		*dest++ = (pixel & 0xF800) >> 8;
		*dest++ = 0;
	    }
	    src += frame->rowstride;
	}
	break;
    case COLFMT_RGB888:
	for( y=0; y< frame->vres; y++ ) {
	    char *p = src;
	    for( x=0; x < frame->hres; x++ ) {
		*dest++ = *p++;
		*dest++ = *p++;
		*dest++ = *p++;
		*dest++ = 0;
	    }
	    src += frame->rowstride;
	}
	break;
    case COLFMT_ARGB8888:
	bytes_per_line = frame->hres << 2;
	if( bytes_per_line == frame->rowstride ) {
	    /* A little bit faster */
	    memcpy( dest, src, bytes_per_line * frame->vres );
	} else {
	    for( y=0; y< frame->vres; y++ ) {
		memcpy( dest, src, bytes_per_line );
		src += frame->rowstride;
		dest += bytes_per_line;
	    }
	}
	break;
    }
    gtk_widget_queue_draw( video_area );
    return TRUE;
}

gboolean video_gtk_set_render_format( uint32_t width, uint32_t height,  
				      int colour_format, gboolean texture )
{
    return video_glx_set_render_format( 0, 0, width, height );
}
