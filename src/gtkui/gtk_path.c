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
#include <gtk/gtk.h>

#include "lxdream.h"
#include "dreamcast.h"
#include "config.h"
#include "lxpaths.h"
#include "gtkui/gtkui.h"

static GtkWidget *path_entry[CONFIG_KEY_MAX];

static gboolean path_file_button_clicked( GtkWidget *button, gpointer user_data )
{
    GtkWidget *entry = GTK_WIDGET(user_data);
    GtkWidget *file = gtk_file_chooser_dialog_new( _("Select file"), NULL,
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
            NULL );
    gchar *filename = get_expanded_path(gtk_entry_get_text(GTK_ENTRY(entry)));
    gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(file), filename );
    gtk_window_set_modal( GTK_WINDOW(file), TRUE );
    gtk_widget_show_all( file );
    gint result = gtk_dialog_run(GTK_DIALOG(file));
    g_free(filename);
    if( result == GTK_RESPONSE_ACCEPT ) {
        filename = get_escaped_path(gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(file) ));
        gtk_entry_set_text(GTK_ENTRY(entry), filename);
        g_free(filename);
    }
    gtk_widget_destroy(file);
    return TRUE;
}

static gboolean path_dir_button_clicked( GtkWidget *button, gpointer user_data )
{
    GtkWidget *entry = GTK_WIDGET(user_data);
    GtkWidget *file = gtk_file_chooser_dialog_new( _("Select file"), NULL,
            GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
            NULL );
    gchar *filename = get_expanded_path(gtk_entry_get_text(GTK_ENTRY(entry)));
    gtk_file_chooser_set_action( GTK_FILE_CHOOSER(file), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER );
    gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(file), filename );
    gtk_window_set_modal( GTK_WINDOW(file), TRUE );
    gtk_widget_show_all( file );
    gint result = gtk_dialog_run(GTK_DIALOG(file));
    g_free(filename);
    if( result == GTK_RESPONSE_ACCEPT ) {
        filename = get_escaped_path(gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(file) ));
        gtk_entry_set_text(GTK_ENTRY(entry), filename);
        g_free(filename);
    }
    gtk_widget_destroy(file);
    return TRUE;
}

GtkWidget *path_panel_new(void)
{
    int i, y=0;
    GtkWidget *table = gtk_table_new( CONFIG_KEY_MAX, 3, FALSE );
    for( i=0; i<CONFIG_KEY_MAX; i++ ) {
        const struct lxdream_config_entry *entry = lxdream_get_global_config_entry(i);
        if( entry->label != NULL ) {
            GtkWidget *text = path_entry[i] = gtk_entry_new();
            GtkWidget *button = gtk_button_new();
            gtk_table_attach( GTK_TABLE(table), gtk_label_new(Q_(entry->label)), 0, 1, y, y+1,
                              GTK_SHRINK, GTK_SHRINK, 0, 0);
            gtk_entry_set_text( GTK_ENTRY(text), lxdream_get_global_config_value(i) );
            gtk_entry_set_width_chars( GTK_ENTRY(text), 48 );
            gtk_table_attach_defaults( GTK_TABLE(table), text, 1, 2, y, y+1 );
            gtk_table_attach( GTK_TABLE(table), button, 2, 3, y, y+1, GTK_SHRINK, GTK_SHRINK, 0, 0 );
            if( entry->type == CONFIG_TYPE_FILE ) {
                GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_FILE, GTK_ICON_SIZE_MENU);
                gtk_button_set_image( GTK_BUTTON(button), image );
                g_signal_connect( button, "clicked", G_CALLBACK(path_file_button_clicked), text );
            } else {
                GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU);
                gtk_button_set_image( GTK_BUTTON(button), image );
                g_signal_connect( button, "clicked", G_CALLBACK(path_dir_button_clicked), text );
            }
            y++;
        }
    }
    gtk_table_resize( GTK_TABLE(table), y, 3 );
    return table;

}

void path_panel_done( GtkWidget *panel, gboolean isOK )
{
    if( isOK ) {
        int i;
        for(i=0; i<CONFIG_KEY_MAX; i++ ) {
            if( path_entry[i] != NULL ) {
                const char *filename = gtk_entry_get_text( GTK_ENTRY(path_entry[i]) );
                lxdream_set_global_config_value( i, filename );
            }
        }

        lxdream_save_config();
        dreamcast_config_changed();
        gtk_gui_update();
    }
}

void path_dialog_run( void )
{
    gtk_gui_run_property_dialog( _("Path Settings"), path_panel_new(), path_panel_done );
}
