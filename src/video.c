/**
 * $Id: video.c,v 1.3 2006-01-03 12:21:45 nkeynes Exp $
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
#include <stdint.h>

GdkImage *img;
GtkWindow *video_win;
GtkWidget *video_area;
char *video_data;
uint32_t video_width = 640;
uint32_t video_height = 480;
uint32_t video_frame_count = 0;

void video_open( void )
{
    img = gdk_image_new( GDK_IMAGE_FASTEST, gdk_visual_get_system(),
                         640, 480 );
    video_win = GTK_WINDOW(gtk_window_new( GTK_WINDOW_TOPLEVEL ));
    video_area = gtk_image_new_from_image(img, NULL);
    gtk_widget_show( video_area );
    gtk_container_add( GTK_CONTAINER(video_win), video_area );
    video_data = img->mem;
    
    gtk_window_set_title( video_win, "DreamOn! - Emulation Window" );
    gtk_window_set_policy( video_win, FALSE, FALSE, FALSE );
    gtk_window_set_default_size( video_win, 640, 480 );
    
    gtk_widget_show( GTK_WIDGET(video_win) );
}

/**
 * Fill the entire frame with the specified colour (00RRGGBB)
 */
void video_fill( uint32_t colour ) 
{
    char *p = video_data;
    int i;
    for( i=0; i<video_width*video_height; i++ ) {
	*p++ = (colour>>16) & 0xFF;
	*p++ = (colour>>8) & 0xFF;
	*p++ = (colour) & 0xFF;
    }
}

void video_update_frame( void )
{
    video_frame_count++;
    gtk_widget_queue_draw( video_area );
}

void video_update_size( int hres, int vres, int colmode )
{
    /* do something intelligent */
}
