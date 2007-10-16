/**
 * $Id: gtkcb.c,v 1.3 2007-10-16 12:36:29 nkeynes Exp $
 *
 * Action callbacks from the main window
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

#include "dream.h"
#include "dreamcast.h"
#include "gdrom/gdrom.h"
#include "gui/gtkui.h"

typedef gboolean (*file_callback_t)( const gchar *filename );

static gboolean dreamcast_paused = FALSE;

void dreamcast_pause()
{
    if( dreamcast_is_running() ) {
	dreamcast_paused = TRUE;
	dreamcast_stop();
    }
}

void dreamcast_unpause()
{
    if( dreamcast_paused ) {
	dreamcast_paused = FALSE;
	if( !dreamcast_is_running() ) {
	    dreamcast_run();
	}
    }
}


void open_file_callback(GtkWidget *btn, gint result, gpointer user_data) {
    GtkFileChooser *file = GTK_FILE_CHOOSER(user_data);
    if( result == GTK_RESPONSE_ACCEPT ) {
	gchar *filename =gtk_file_chooser_get_filename(
						       GTK_FILE_CHOOSER(file) );
	file_callback_t action = (file_callback_t)gtk_object_get_data( GTK_OBJECT(file), "file_action" );
	gtk_widget_destroy(GTK_WIDGET(file));
	action( filename );
	g_free(filename);
    } else {
	gtk_widget_destroy(GTK_WIDGET(file));
    }
    dreamcast_unpause();
}

static void add_file_pattern( GtkFileChooser *chooser, char *pattern, char *patname )
{
    if( pattern != NULL ) {
	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_add_pattern( filter, pattern );
	gtk_file_filter_set_name( filter, patname );
	gtk_file_chooser_add_filter( chooser, filter );
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name( filter, "All files" );
	gtk_file_filter_add_pattern( filter, "*" );
	gtk_file_chooser_add_filter( chooser, filter );
    }
}

void open_file_dialog( char *title, file_callback_t action, char *pattern, char *patname,
		       gchar const *initial_dir )
{
    GtkWidget *file;
    dreamcast_pause();
    file = gtk_file_chooser_dialog_new( title, NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL );
    add_file_pattern( GTK_FILE_CHOOSER(file), pattern, patname );
    g_signal_connect( GTK_OBJECT(file), "response", 
		      GTK_SIGNAL_FUNC(open_file_callback), file );
    gtk_object_set_data( GTK_OBJECT(file), "file_action", action );
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(file), initial_dir );
    gtk_window_set_modal( GTK_WINDOW(file), TRUE );
    gtk_widget_show( file );
}

void save_file_dialog( char *title, file_callback_t action, char *pattern, char *patname,
		       gchar const *initial_dir )
{
    GtkWidget *file;
    dreamcast_pause();
    file = gtk_file_chooser_dialog_new( title, NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					NULL );
    add_file_pattern( GTK_FILE_CHOOSER(file), pattern, patname );
    g_signal_connect( GTK_OBJECT(file), "response", 
		      GTK_SIGNAL_FUNC(open_file_callback), file );
    gtk_object_set_data( GTK_OBJECT(file), "file_action", action );
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(file), initial_dir );
    gtk_window_set_modal( GTK_WINDOW(file), TRUE );
    gtk_widget_show( file );
}

void mount_action_callback( GtkAction *action, gpointer user_data)
{
    const gchar *dir = dreamcast_get_config_value(CONFIG_DEFAULT_PATH);
    open_file_dialog( "Open...", gdrom_mount_image, NULL, NULL, dir );
}
void reset_action_callback( GtkAction *action, gpointer user_data)
{
    dreamcast_reset();
}

void pause_action_callback( GtkAction *action, gpointer user_data)
{
    dreamcast_stop();
}

void resume_action_callback( GtkAction *action, gpointer user_data)
{
    dreamcast_run();
}

void load_state_action_callback( GtkAction *action, gpointer user_data)
{
    const gchar *dir = dreamcast_get_config_value(CONFIG_SAVE_PATH);
    open_file_dialog( "Load state...", dreamcast_load_state, "*.dst", "lxDream Save State (*.dst)", dir );
}
void save_state_action_callback( GtkAction *action, gpointer user_data)
{
    const gchar *dir = dreamcast_get_config_value(CONFIG_SAVE_PATH);
    save_file_dialog( "Save state...", dreamcast_save_state, "*.dst", "lxDream Save State (*.dst)", dir );
}
void about_action_callback( GtkAction *action, gpointer user_data)
{
    
    GtkWidget *dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG,
				      "name", APP_NAME, 
                                     "version", APP_VERSION,
			             "copyright", "(C) 2003-2007 Nathan Keynes",
                                     NULL);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
}

void exit_action_callback( GtkAction *action, gpointer user_data)
{
    exit(0);
}

void debugger_action_callback( GtkAction *action, gpointer user_data)
{
    gtk_gui_show_debugger();
}

void path_settings_callback( GtkAction *action, gpointer user_data)
{
}

void audio_settings_callback( GtkAction *action, gpointer user_data)
{
}

void controller_settings_callback( GtkAction *action, gpointer user_data)
{
    controller_dialog_run( NULL );
}

void network_settings_callback( GtkAction *action, gpointer user_data)
{
}

void video_settings_callback( GtkAction *action, gpointer user_data)
{
}

void fullscreen_toggle_callback( GtkToggleAction *action, gpointer user_data)
{
}
