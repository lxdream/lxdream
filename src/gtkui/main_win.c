/**
 * $Id$
 *
 * Define the main (emu) GTK window, along with its menubars,
 * toolbars, etc.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xutil.h>

#include "dream.h"
#include "gtkui/gtkui.h"
#include "drivers/video_glx.h"


struct main_window_info {
    GtkWidget *window;
    GtkWidget *video;
    GtkWidget *menubar;
    GtkWidget *toolbar;
    GtkWidget *statusbar;
    GtkActionGroup *actions;
};

static gboolean on_main_window_deleted( GtkWidget *widget, GdkEvent event, gpointer user_data )
{
    exit(0);
}

static void on_main_window_state_changed( GtkWidget *widget, GdkEventWindowState *state, 
					  gpointer userdata )
{
    main_window_t win = (main_window_t)userdata;
    if( state->changed_mask & GDK_WINDOW_STATE_FULLSCREEN ) {
	gboolean fs = (state->new_window_state & GDK_WINDOW_STATE_FULLSCREEN);
	GtkWidget *frame = gtk_widget_get_parent(win->video);
	if( frame->style == NULL ) {
	    gtk_widget_set_style( frame, gtk_style_new() );
	}
	if( fs ) {
	    gtk_widget_hide( win->menubar );
	    gtk_widget_hide( win->toolbar );
	    gtk_widget_hide( win->statusbar );
	    
	    frame->style->xthickness = 0;
	    frame->style->ythickness = 0;
	} else {
	    frame->style->xthickness = 2;
	    frame->style->ythickness = 2;
	    gtk_widget_show( win->menubar );
	    gtk_widget_show( win->toolbar );
	    gtk_widget_show( win->statusbar );
	}
	gtk_widget_queue_draw( win->window );
    }
}

main_window_t main_window_new( const gchar *title, GtkWidget *menubar, GtkWidget *toolbar,
			       GtkAccelGroup *accel_group )
{
    GtkWidget *vbox;
    GtkWidget *frame;
    main_window_t win = g_malloc0( sizeof(struct main_window_info) );

    win->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    win->menubar = menubar;
    win->toolbar = toolbar;
    gtk_window_set_title( GTK_WINDOW(win->window), title );
    gtk_window_add_accel_group (GTK_WINDOW (win->window), accel_group);

    gtk_toolbar_set_style( GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS );
    
    Display *display = gdk_x11_display_get_xdisplay( gtk_widget_get_display(win->window));
    Screen *screen = gdk_x11_screen_get_xscreen( gtk_widget_get_screen(win->window));
    int screen_no = XScreenNumberOfScreen(screen);
    video_glx_init(display, screen_no);

    XVisualInfo *visual = video_glx_get_visual();
    GdkVisual *gdkvis = gdk_x11_screen_lookup_visual( gtk_widget_get_screen(win->window), visual->visualid );
    GdkColormap *colormap = gdk_colormap_new( gdkvis, FALSE );
    win->video = gtk_drawing_area_new();
    gtk_widget_set_colormap( win->video, colormap );
    GTK_WIDGET_SET_FLAGS(win->video, GTK_CAN_FOCUS|GTK_CAN_DEFAULT);
    gtk_widget_set_size_request( win->video, 640, 480 ); 
    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type( GTK_FRAME(frame), GTK_SHADOW_IN );
    gtk_container_add( GTK_CONTAINER(frame), win->video );

    win->statusbar = gtk_statusbar_new();

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add( GTK_CONTAINER(win->window), vbox );
    gtk_box_pack_start( GTK_BOX(vbox), menubar, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), toolbar, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), frame, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), win->statusbar, FALSE, FALSE, 0 );
    gtk_widget_show_all( win->window );
    gtk_widget_grab_focus( win->video );
    
    gtk_statusbar_push( GTK_STATUSBAR(win->statusbar), 1, "Stopped" );
    g_signal_connect( win->window, "delete_event", 
		      G_CALLBACK(on_main_window_deleted), win );
    g_signal_connect( win->window, "window-state-event",
		      G_CALLBACK(on_main_window_state_changed), win );
    return win;
}

void main_window_set_running( main_window_t win, gboolean running )
{
    gtk_gui_enable_action( "Pause", running );
    gtk_gui_enable_action( "Run", !running && dreamcast_can_run() );
    gtk_statusbar_pop( GTK_STATUSBAR(win->statusbar), 1 );
    gtk_statusbar_push( GTK_STATUSBAR(win->statusbar), 1, running ? "Running" : "Stopped" );
}

void main_window_set_framerate( main_window_t win, float rate )
{


}

void main_window_set_speed( main_window_t win, double speed )
{
    char buf[32];

    snprintf( buf, 32, "Running (%2.4f%%)", speed );
    gtk_statusbar_pop( GTK_STATUSBAR(win->statusbar), 1 );
    gtk_statusbar_push( GTK_STATUSBAR(win->statusbar), 1, buf );
    

}

GtkWidget *main_window_get_renderarea( main_window_t win )
{
    return win->video;
}

GtkWindow *main_window_get_frame( main_window_t win )
{
    return GTK_WINDOW(win->window);
}

void main_window_set_fullscreen( main_window_t win, gboolean fullscreen )
{
    if( fullscreen ) {
	gtk_window_fullscreen( GTK_WINDOW(win->window) );
    } else {
	gtk_window_unfullscreen( GTK_WINDOW(win->window) );
    }
}
