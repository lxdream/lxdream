/**
 * $Id: main_win.c,v 1.6 2007-10-21 05:21:35 nkeynes Exp $
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

#include "dream.h"
#include "gui/gtkui.h"


struct main_window_info {
    GtkWidget *window;
    GtkWidget *video;
    GtkWidget *statusbar;
    GtkActionGroup *actions;
};

main_window_t main_window_new( const gchar *title, GtkWidget *menubar, GtkWidget *toolbar,
			       GtkAccelGroup *accel_group )
{
    GtkWidget *vbox;
    GtkWidget *frame;
    main_window_t win = g_malloc0( sizeof(struct main_window_info) );

    win->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title( GTK_WINDOW(win->window), title );
    gtk_window_add_accel_group (GTK_WINDOW (win->window), accel_group);

    gtk_toolbar_set_style( GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS );

    win->video = gtk_drawing_area_new();
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
    return win;
}

void main_window_set_running( main_window_t win, gboolean running )
{
    gtk_gui_enable_action( "Pause", running );
    gtk_gui_enable_action( "Run", !running );
    gtk_statusbar_pop( GTK_STATUSBAR(win->statusbar), 1 );
    gtk_statusbar_push( GTK_STATUSBAR(win->statusbar), 1, running ? "Running" : "Stopped" );
}

void main_window_set_framerate( main_window_t win, float rate )
{


}

void main_window_set_speed( main_window_t win, double speed )
{
    char buf[32];

    snprintf( buf, 32, "Running (%2.4f%)", speed );
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
