/**
 * $Id: video_gtk.c,v 1.15 2007-10-13 04:01:02 nkeynes Exp $
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
#include "gui/gtkui.h"

static GtkWidget *video_win = NULL;
int video_width = 640;
int video_height = 480;

gboolean video_gtk_init();
void video_gtk_shutdown();
uint16_t video_gtk_resolve_keysym( const gchar *keysym );

struct display_driver display_gtk_driver = { "gtk", video_gtk_init, video_gtk_shutdown,
					     video_gtk_resolve_keysym,
					     NULL, NULL, NULL, NULL, NULL, NULL, NULL };
					     

gboolean video_gtk_keydown_callback(GtkWidget       *widget,
				     GdkEventKey     *event,
				     gpointer         user_data)
{
    input_event_keydown( event->keyval );
    return TRUE;
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
    return TRUE;
}

gboolean video_gtk_expose_callback(GtkWidget *widget, GdkEventExpose *event, gpointer data )
{
    gl_redisplay_last();
    return TRUE;
}

gboolean video_gtk_resize_callback(GtkWidget *widget, GdkEventConfigure *event, gpointer data )
{
    video_width = event->width;
    video_height = event->height;
    gl_redisplay_last();
    return TRUE;
}

gboolean video_gtk_init()
{
    video_win = gtk_gui_get_renderarea();

    g_signal_connect( video_win, "key_press_event", 
		      G_CALLBACK(video_gtk_keydown_callback), NULL );
    g_signal_connect( video_win, "key_release_event", 
		      G_CALLBACK(video_gtk_keyup_callback), NULL );
    g_signal_connect( video_win, "expose_event",
		      G_CALLBACK(video_gtk_expose_callback), NULL );
    g_signal_connect( video_win, "configure_event",
		      G_CALLBACK(video_gtk_resize_callback), NULL );
    gtk_widget_add_events( video_win, 
			   GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
			   GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK );
    gtk_widget_set_double_buffered( video_win, FALSE );
    video_width = video_win->allocation.width;
    video_height = video_win->allocation.height;
    return video_glx_init( gdk_x11_display_get_xdisplay( gtk_widget_get_display(GTK_WIDGET(video_win))),
			   gdk_x11_screen_get_xscreen( gtk_widget_get_screen(GTK_WIDGET(video_win))),
			   GDK_WINDOW_XWINDOW( GTK_WIDGET(video_win)->window ),
			   video_width, video_height, &display_gtk_driver );
}

void video_gtk_shutdown()
{
    video_glx_shutdown();
    gtk_widget_destroy( GTK_WIDGET(video_win) );

}

