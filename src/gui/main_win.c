/**
 * $Id: main_win.c,v 1.3 2007-10-13 03:58:31 nkeynes Exp $
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

#define SET_ACTION_ENABLED(win,name,b) gtk_action_set_sensitive( gtk_action_group_get_action( win->actions, name), b)
#define ENABLE_ACTION(win,name) SET_ACTION_ENABLED(win,name,TRUE)
#define DISABLE_ACTION(win,name) SET_ACTION_ENABLED(win,name,FALSE)

static const GtkActionEntry ui_actions[] = {
    { "FileMenu", NULL, "_File" },
    { "SettingsMenu", NULL, "_Settings" },
    { "HelpMenu", NULL, "_Help" },
    { "Mount", GTK_STOCK_CDROM, "_Mount...", "<control>O", "Mount a cdrom disc", G_CALLBACK(mount_action_callback) },
    { "Reset", GTK_STOCK_REFRESH, "_Reset", "<control>R", "Reset dreamcast", G_CALLBACK(reset_action_callback) },
    { "Pause", GTK_STOCK_MEDIA_PAUSE, "_Pause", NULL, "Pause dreamcast", G_CALLBACK(pause_action_callback) },
    { "Run", GTK_STOCK_MEDIA_PLAY, "Resume", NULL, "Resume", G_CALLBACK(resume_action_callback) },
    { "LoadState", GTK_STOCK_REVERT_TO_SAVED, "_Load state...", "F4", "Load an lxdream save state", G_CALLBACK(load_state_action_callback) },
    { "SaveState", GTK_STOCK_SAVE_AS, "_Save state...", "F3", "Create an lxdream save state", G_CALLBACK(save_state_action_callback) },
    { "Debugger", NULL, "_Debugger", NULL, "Open debugger window", G_CALLBACK(debugger_action_callback) },
    { "Exit", GTK_STOCK_QUIT, "E_xit", NULL, "Exit lxdream", G_CALLBACK(exit_action_callback) },
    { "AudioSettings", NULL, "_Audio...", NULL, "Configure audio output", G_CALLBACK(audio_settings_callback) },
    { "ControllerSettings", NULL, "_Controllers...", NULL, "Configure controllers", G_CALLBACK(controller_settings_callback) },
    { "NetworkSettings", NULL, "_Network...", NULL, "Configure network settings", G_CALLBACK(network_settings_callback) },
    { "VideoSettings", NULL, "_Video...", NULL, "Configure video output", G_CALLBACK(video_settings_callback) },
    { "About", GTK_STOCK_ABOUT, "_About...", NULL, "About lxdream", G_CALLBACK(about_action_callback) }
};
static const GtkToggleActionEntry ui_toggle_actions[] = {
    { "FullScreen", NULL, "_Full Screen", "F9", "Toggle full screen video", G_CALLBACK(fullscreen_toggle_callback), 0 },
};
    

static const char *ui_description =
    "<ui>"
    " <menubar name='MainMenu'>"
    "  <menu action='FileMenu'>"
    "   <menuitem action='Mount'/>"
    "   <separator/>"
    "   <menuitem action='Reset'/>"
    "   <menuitem action='Pause'/>"
    "   <menuitem action='Run'/>"
    "   <separator/>"
    "   <menuitem action='LoadState'/>"
    "   <menuitem action='SaveState'/>"
    "   <separator/>"
    "   <menuitem action='Exit'/>"
    "  </menu>"
    "  <menu action='SettingsMenu'>"
    "   <menuitem action='AudioSettings'/>"
    "   <menuitem action='ControllerSettings'/>"
    "   <menuitem action='NetworkSettings'/>"
    "   <menuitem action='VideoSettings'/>"
    "   <separator/>"
    "   <menuitem action='FullScreen'/>"
    "  </menu>"
    "  <menu action='HelpMenu'>"
    "   <menuitem action='About'/>"
    "  </menu>"
    " </menubar>"
    " <toolbar name='MainToolbar'>"
    "  <toolitem action='Mount'/>"
    "  <toolitem action='Reset'/>"
    "  <toolitem action='Pause'/>"
    "  <toolitem action='Run'/>"
    "  <separator/>"
    "  <toolitem action='LoadState'/>"
    "  <toolitem action='SaveState'/>"
    " </toolbar>"
    "</ui>";


struct main_window_info {
    GtkWidget *window;
    GtkWidget *menubar;
    GtkWidget *toolbar;
    GtkWidget *video;
    GtkWidget *statusbar;
    GtkActionGroup *actions;
};

main_window_t main_window_new( const gchar *title )
{
    GtkWidget *vbox;
    GtkUIManager *ui_manager;
    GtkAccelGroup *accel_group;
    GtkWidget *frame;
    GError *error = NULL;
    main_window_t win = g_malloc0( sizeof(struct main_window_info) );

    win->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title( GTK_WINDOW(win->window), title );

    win->actions = gtk_action_group_new("MenuActions");
    gtk_action_group_add_actions( win->actions, ui_actions, G_N_ELEMENTS(ui_actions), win->window );
    gtk_action_group_add_toggle_actions( win->actions, ui_toggle_actions, G_N_ELEMENTS(ui_toggle_actions), win->window );
    DISABLE_ACTION(win, "AudioSettings");
    DISABLE_ACTION(win, "NetworkSettings");
    DISABLE_ACTION(win, "VideoSettings");
    
    ui_manager = gtk_ui_manager_new();
    gtk_ui_manager_set_add_tearoffs(ui_manager, TRUE);
    gtk_ui_manager_insert_action_group( ui_manager, win->actions, 0 );

    if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_description, -1, &error)) {
	g_message ("building menus failed: %s", error->message);
	g_error_free (error);
	exit(1);
    }
    
    accel_group = gtk_ui_manager_get_accel_group (ui_manager);
    gtk_window_add_accel_group (GTK_WINDOW (win->window), accel_group);

    win->menubar = gtk_ui_manager_get_widget(ui_manager, "/MainMenu");
    win->toolbar = gtk_ui_manager_get_widget(ui_manager, "/MainToolbar");

    gtk_toolbar_set_style( GTK_TOOLBAR(win->toolbar), GTK_TOOLBAR_ICONS );

    win->video = gtk_drawing_area_new();
    GTK_WIDGET_SET_FLAGS(win->video, GTK_CAN_FOCUS|GTK_CAN_DEFAULT);
    gtk_widget_set_size_request( win->video, 640, 480 ); 
    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type( GTK_FRAME(frame), GTK_SHADOW_IN );
    gtk_container_add( GTK_CONTAINER(frame), win->video );

    win->statusbar = gtk_statusbar_new();

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add( GTK_CONTAINER(win->window), vbox );
    gtk_box_pack_start( GTK_BOX(vbox), win->menubar, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), win->toolbar, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), frame, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), win->statusbar, FALSE, FALSE, 0 );
    gtk_widget_show_all( win->window );
    gtk_widget_grab_focus( win->video );
    
    gtk_statusbar_push( GTK_STATUSBAR(win->statusbar), 1, "Stopped" );
    return win;
}

void main_window_set_running( main_window_t win, gboolean running )
{
    SET_ACTION_ENABLED( win, "Pause", running );
    SET_ACTION_ENABLED( win, "Run", !running );
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
