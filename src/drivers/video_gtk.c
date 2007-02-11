/**
 * $Id: video_gtk.c,v 1.10 2007-02-11 10:09:32 nkeynes Exp $
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

gboolean video_gtk_init();
void video_gtk_shutdown();
uint16_t video_gtk_resolve_keysym( const gchar *keysym );

struct display_driver display_gtk_driver = { "gtk", video_gtk_init, NULL,
					     video_gtk_resolve_keysym,
					     NULL, NULL, NULL, NULL, NULL, NULL, NULL };
					     

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

gboolean video_gtk_init()
{
    video_win = GTK_WINDOW(gtk_window_new( GTK_WINDOW_TOPLEVEL ));
    gtk_window_set_title( video_win, APP_NAME " - Emulation Window" );
    gtk_window_set_policy( video_win, FALSE, FALSE, FALSE );
    
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
    video_img = gdk_image_new( GDK_IMAGE_FASTEST, gdk_visual_get_system(),
                               video_width, video_height );
    gtk_image_set_from_image( GTK_IMAGE(video_area), video_img, NULL );

    gtk_window_set_default_size( video_win, video_width, video_height );

    video_glx_init( gdk_x11_display_get_xdisplay( gtk_widget_get_display(GTK_WIDGET(video_win))),
		    gdk_x11_screen_get_xscreen( gtk_widget_get_screen(GTK_WIDGET(video_win))),
		    GDK_WINDOW_XWINDOW( GTK_WIDGET(video_win)->window ),
		    video_width, video_height, &display_gtk_driver );
    return TRUE;
}

void video_gtk_shutdown()
{


}

