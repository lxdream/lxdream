/**
 * $Id$
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
#include "config.h"
#include "dreamcast.h"
#include "gdrom/gdrom.h"
#include "gtkui/gtkui.h"
#include "pvr2/pvr2.h"
#include "loader.h"


static void add_file_pattern( GtkFileChooser *chooser, const char *pattern, const char *patname )
{
    if( pattern != NULL ) {
        GtkFileFilter *filter = gtk_file_filter_new();
        gtk_file_filter_add_pattern( filter, pattern );
        gtk_file_filter_set_name( filter, patname );
        gtk_file_chooser_add_filter( chooser, filter );
        filter = gtk_file_filter_new();
        gtk_file_filter_set_name( filter, _("All files") );
        gtk_file_filter_add_pattern( filter, "*" );
        gtk_file_chooser_add_filter( chooser, filter );
    }
}

void open_file_dialog( const char *title, file_callback_t action, const char *pattern, const char *patname,
                       const gchar *initial_dir )
{
    GtkWidget *file;
    gchar *initial_path = get_absolute_path(initial_dir);
    file = gtk_file_chooser_dialog_new( title, NULL,
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
            NULL );
    add_file_pattern( GTK_FILE_CHOOSER(file), pattern, patname );
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(file), initial_path );
    gtk_window_set_modal( GTK_WINDOW(file), TRUE );
    gtk_dialog_set_default_response( GTK_DIALOG(file), GTK_RESPONSE_ACCEPT );
    int result = gtk_dialog_run( GTK_DIALOG(file) );
    if( result == GTK_RESPONSE_ACCEPT ) {
        gchar *filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(file) );
        action( filename );
    }
    gtk_widget_destroy(file);
    g_free(initial_path);
}

void save_file_dialog( const char *title, file_callback_t action, const char *pattern, const char *patname,
                       const gchar *initial_dir )
{
    GtkWidget *file;
    gchar *initial_path = get_absolute_path(initial_dir);
    file = gtk_file_chooser_dialog_new( title, NULL,
            GTK_FILE_CHOOSER_ACTION_SAVE,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
            NULL );
    add_file_pattern( GTK_FILE_CHOOSER(file), pattern, patname );
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(file), initial_path );
    gtk_window_set_modal( GTK_WINDOW(file), TRUE );
    gtk_dialog_set_default_response( GTK_DIALOG(file), GTK_RESPONSE_ACCEPT );
    int result = gtk_dialog_run( GTK_DIALOG(file) );
    if( result == GTK_RESPONSE_ACCEPT ) {
        gchar *filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(file) );
        action( filename );
    }
    gtk_widget_destroy(file);
    g_free(initial_path);
}

void mount_action_callback( GtkAction *action, gpointer user_data)
{
    const gchar *dir = lxdream_get_config_value(CONFIG_DEFAULT_PATH);
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

void load_binary_action_callback( GtkAction *action, gpointer user_data)
{
    const gchar *dir = lxdream_get_config_value(CONFIG_DEFAULT_PATH);
    open_file_dialog( "Open Binary...", file_load_magic, NULL, NULL, dir );
}

void load_state_preview_callback( GtkFileChooser *chooser, gpointer user_data )
{
    GtkWidget *preview = GTK_WIDGET(user_data);
    gchar *filename = gtk_file_chooser_get_preview_filename(chooser);

    frame_buffer_t data = dreamcast_load_preview(filename);
    if( data != NULL ) {
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_frame_buffer(data);
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, 320, 240,
                GDK_INTERP_BILINEAR);
        g_object_unref(pixbuf);
        gtk_image_set_from_pixbuf( GTK_IMAGE(preview), scaled );
        g_object_unref(scaled);
        gtk_widget_show(preview);
    } else {
        gtk_widget_hide(preview);
    }
}

void load_state_action_callback( GtkAction *action, gpointer user_data)
{
    GtkWidget *file, *preview, *frame, *align;
    GtkRequisition size;
    const gchar *dir = lxdream_get_config_value(CONFIG_SAVE_PATH);
    gchar *path = get_absolute_path(dir);
    file = gtk_file_chooser_dialog_new( _("Load state..."), NULL,
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
            NULL );
    add_file_pattern( GTK_FILE_CHOOSER(file), "*.dst", _("lxDream Save State (*.dst)") );
    gtk_object_set_data( GTK_OBJECT(file), "file_action", action );

    preview = gtk_image_new( );

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type( GTK_FRAME(frame), GTK_SHADOW_IN );
    gtk_container_add( GTK_CONTAINER(frame), preview );
    gtk_widget_show(frame);
    gtk_widget_size_request(frame, &size);
    gtk_widget_set_size_request(frame, size.width + 320, size.height + 240);
    align = gtk_alignment_new(0.5, 0.5, 0, 0 );
    gtk_container_add( GTK_CONTAINER(align), frame );
    gtk_widget_show( align );
    gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(file), align);
    g_signal_connect( file, "update-preview", G_CALLBACK(load_state_preview_callback),
                      preview );
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(file), path );
    gtk_window_set_modal( GTK_WINDOW(file), TRUE );
    int result = gtk_dialog_run( GTK_DIALOG(file) );
    if( result == GTK_RESPONSE_ACCEPT ) {
        gchar *filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(file) );
        dreamcast_load_state( filename );
    }
    gtk_widget_destroy(file);
    g_free(path);
}

void save_state_action_callback( GtkAction *action, gpointer user_data)
{
    const gchar *dir = lxdream_get_config_value(CONFIG_SAVE_PATH);
    save_file_dialog( "Save state...", dreamcast_save_state, "*.dst", _("lxDream Save State (*.dst)"), dir );
}
void about_action_callback( GtkAction *action, gpointer user_data)
{

    GtkWidget *dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG,
            "name", APP_NAME, 
            "version", lxdream_full_version,
            "copyright", lxdream_copyright,
            NULL);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

}

void exit_action_callback( GtkAction *action, gpointer user_data)
{
    dreamcast_shutdown();
    exit(0);
}

void path_settings_callback( GtkAction *action, gpointer user_data)
{
    path_dialog_run();
}

void audio_settings_callback( GtkAction *action, gpointer user_data)
{
}

void maple_settings_callback( GtkAction *action, gpointer user_data)
{
    maple_dialog_run( );
}

void network_settings_callback( GtkAction *action, gpointer user_data)
{
}

void video_settings_callback( GtkAction *action, gpointer user_data)
{
}

void fullscreen_toggle_callback( GtkToggleAction *action, gpointer user_data)
{
    main_window_set_fullscreen(gtk_gui_get_main(), gtk_toggle_action_get_active(action));
}

void debugger_action_callback( GtkAction *action, gpointer user_data)
{
    gtk_gui_show_debugger();
}

void debug_memory_action_callback( GtkAction *action, gpointer user_data)
{
    gchar *title = g_strdup_printf( "%s :: %s", lxdream_package_name, _("Memory dump") );
    dump_window_new( title );
    g_free(title);
}

void debug_mmio_action_callback( GtkAction *action, gpointer user_data)
{
    gtk_gui_show_mmio();
}

void save_scene_action_callback( GtkAction *action, gpointer user_data)
{
    const gchar *dir = lxdream_get_config_value(CONFIG_SAVE_PATH);
    save_file_dialog( _("Save next scene..."), pvr2_save_next_scene, "*.dsc", _("lxdream scene file (*.dsc)"), dir );
}

int debug_window_get_selected_row( debug_window_t data );

void debug_step_action_callback( GtkAction *action, gpointer user_data)
{
    debug_window_single_step(gtk_gui_get_debugger());
}

void debug_runto_action_callback( GtkAction *action, gpointer user_data)
{
    debug_window_t debug = gtk_gui_get_debugger();
    int selected_row = debug_window_get_selected_row(debug);
    if( selected_row == -1 ) {
        WARN( _("No address selected, so can't run to it"), NULL );
    } else {
        debug_window_set_oneshot_breakpoint( debug, selected_row );
        dreamcast_run();
    }
}

void debug_breakpoint_action_callback( GtkAction *action, gpointer user_data)
{
    debug_window_t debug = gtk_gui_get_debugger();
    int selected_row = debug_window_get_selected_row(debug);
    if( selected_row != -1 ) {
        debug_window_toggle_breakpoint( debug, selected_row );
    }
}
