/**
 * $Id: path_dlg.c,v 1.5 2007-11-10 04:45:29 nkeynes Exp $
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

#include "dream.h"
#include "dreamcast.h"
#include "config.h"
#include "gui/gtkui.h"

static const gchar *path_label[] = { N_("Bios rom"), N_("Flash rom"), N_("Default disc path"), 
				     N_("Save state path"), N_("Bootstrap IP.BIN") };
static const int path_id[] = { CONFIG_BIOS_PATH, CONFIG_FLASH_PATH, CONFIG_DEFAULT_PATH,
			       CONFIG_SAVE_PATH, CONFIG_BOOTSTRAP };
static GtkFileChooserAction path_action[] = {
    GTK_FILE_CHOOSER_ACTION_OPEN,
    GTK_FILE_CHOOSER_ACTION_OPEN,
    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
    GTK_FILE_CHOOSER_ACTION_OPEN };

static GtkWidget *path_entry[5];

static gboolean path_file_button_clicked( GtkWidget *button, gpointer user_data )
{
    GtkWidget *entry = GTK_WIDGET(user_data);
    GtkWidget *file = gtk_file_chooser_dialog_new( _("Select file"), NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL );
    const gchar *filename = gtk_entry_get_text(GTK_ENTRY(entry));
    gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(file), filename );
    gtk_window_set_modal( GTK_WINDOW(file), TRUE );
    gtk_widget_show_all( file );
    gint result = gtk_dialog_run(GTK_DIALOG(file));
    if( result == GTK_RESPONSE_ACCEPT ) {
	filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(file) );
	gtk_entry_set_text(GTK_ENTRY(entry), filename);
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
    const gchar *filename = gtk_entry_get_text(GTK_ENTRY(entry));
    gtk_file_chooser_set_action( GTK_FILE_CHOOSER(file), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER );
    gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(file), filename );
    gtk_window_set_modal( GTK_WINDOW(file), TRUE );
    gtk_widget_show_all( file );
    gint result = gtk_dialog_run(GTK_DIALOG(file));
    if( result == GTK_RESPONSE_ACCEPT ) {
	filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(file) );
	gtk_entry_set_text(GTK_ENTRY(entry), filename);
    }
    gtk_widget_destroy(file);
    return TRUE;
}

GtkWidget *path_panel_new(void)
{
    GtkWidget *table = gtk_table_new( 5, 3, FALSE );
    int i;
    for( i=0; i<5; i++ ) {
	GtkWidget *text = path_entry[i] = gtk_entry_new();
	GtkWidget *button = gtk_button_new();
	gtk_table_attach( GTK_TABLE(table), gtk_label_new(Q_(path_label[i])), 0, 1, i, i+1,
			  GTK_SHRINK, GTK_SHRINK, 0, 0);
	gtk_entry_set_text( GTK_ENTRY(text), lxdream_get_config_value(path_id[i]) );
	gtk_entry_set_width_chars( GTK_ENTRY(text), 48 );
	gtk_table_attach_defaults( GTK_TABLE(table), text, 1, 2, i, i+1 );
	gtk_table_attach( GTK_TABLE(table), button, 2, 3, i, i+1, GTK_SHRINK, GTK_SHRINK, 0, 0 );
	if( path_action[i] == GTK_FILE_CHOOSER_ACTION_OPEN ) {
	    GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
	    gtk_button_set_image( GTK_BUTTON(button), image );
	    g_signal_connect( button, "clicked", G_CALLBACK(path_file_button_clicked), text );
	} else {
	    GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
	    gtk_button_set_image( GTK_BUTTON(button), image );
	    g_signal_connect( button, "clicked", G_CALLBACK(path_dir_button_clicked), text );
	}
    }
    return table;

}

void path_panel_done( GtkWidget *panel, gboolean isOK )
{
    if( isOK ) {
	int i;
	for(i=0; i<5; i++ ) {
	    const char *filename = gtk_entry_get_text( GTK_ENTRY(path_entry[i]) );
	    lxdream_set_global_config_value( path_id[i], filename );
	}
	
	lxdream_save_config();
	dreamcast_config_changed();
    }
}

void path_dialog_run( void )
{
    gtk_gui_run_property_dialog( _("Path Settings"), path_panel_new(), path_panel_done );
}
