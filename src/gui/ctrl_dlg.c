/**
 * $Id: ctrl_dlg.c,v 1.2 2007-10-17 11:26:45 nkeynes Exp $
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
#include "gui/gtkui.h"
#include "maple/maple.h"

#define MAX_DEVICES 4

typedef struct maple_slot_data {
    maple_device_t old_device;
    maple_device_t new_device;
    GtkWidget *button;
    GtkWidget *combo;
} *maple_slot_data_t;

static struct maple_slot_data maple_data[MAX_DEVICES];


gboolean controller_properties_activated( GtkButton *button, gpointer user_data )
{
    maple_slot_data_t data = (maple_slot_data_t)user_data;
}

gboolean controller_device_changed( GtkComboBox *combo, gpointer user_data )
{
    maple_slot_data_t data = (maple_slot_data_t)user_data;
    int active = gtk_combo_box_get_active(combo);
    gtk_widget_set_sensitive(data->button, active != 0);
    if( active != 0 ) {
	gchar *devname = gtk_combo_box_get_active_text(combo);
	const maple_device_class_t devclz = maple_get_device_class(devname);
	assert(devclz != NULL);
	if( data->new_device != NULL ) {
	    if( data->new_device->device_class != devclz ) {
		data->new_device->destroy(data->new_device);
		data->new_device = maple_new_device(devname);
	    }
	} else {
	    data->new_device = maple_new_device(devname);
	}
    } else {
	if( data->new_device != NULL && data->new_device != data->old_device ) {
	    data->new_device->destroy(data->new_device);
	}
	data->new_device = NULL;
    }
}

void controller_commit_changes( )
{
    int i;
    for( i=0; i<MAX_DEVICES; i++ ) {
	if( maple_data[i].new_device != maple_data[i].old_device ) {
	    if( maple_data[i].old_device != NULL ) {
		maple_detach_device(i,0);
	    }
	    if( maple_data[i].new_device != NULL ) {
		maple_attach_device(maple_data[i].new_device, i, 0 );
	    }
	}
    }
    lxdream_save_config();
}

void controller_cancel_changes( )
{
    int i;
    for( i=0; i<MAX_DEVICES; i++ ) {
	if( maple_data[i].new_device != NULL && 
	    maple_data[i].new_device != maple_data[i].old_device ) {
	    maple_data[i].new_device->destroy(maple_data[i].new_device);
	}
    }
}

GtkWidget *controller_panel_new()
{
    GtkWidget *table = gtk_table_new(4, 3, TRUE);
    GtkTreeIter iter;
    int i,j;
    const struct maple_device_class **devices = maple_get_device_classes();

    for( i=0; i< MAX_DEVICES; i++ ) {
	char buf[12];
	GtkWidget *combo, *button;
	int active = 0;
	maple_device_t device = maple_get_device(i,0);
	sprintf( buf, "Slot %d.", i );
	gtk_table_attach_defaults( GTK_TABLE(table), gtk_label_new(buf), 0, 1, i, i+1 );
	combo = gtk_combo_box_new_text();
	gtk_combo_box_append_text( GTK_COMBO_BOX(combo), "<empty>" );
	for( j=0; devices[j] != NULL; j++ ) {
	    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), devices[j]->name);
	    if( device != NULL && device->device_class == devices[j] ) {
		active = j+1;
	    }
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
	gtk_table_attach_defaults( GTK_TABLE(table), combo, 1, 2, i, i+1 );
	button = gtk_button_new_from_stock( GTK_STOCK_PROPERTIES );
	gtk_widget_set_sensitive(button, active != 0);
	gtk_table_attach_defaults( GTK_TABLE(table), button, 2, 3, i, i+1 );

	maple_data[i].old_device = device;
	maple_data[i].new_device = device;
	maple_data[i].combo = combo;
	maple_data[i].button = button;
	g_signal_connect( button, "clicked", 
			  G_CALLBACK( controller_properties_activated ), &maple_data[i] );
	g_signal_connect( combo, "changed", 
			  G_CALLBACK( controller_device_changed ), &maple_data[i] );

    }
    return table;
}

void controller_dialog_run( GtkWindow *parent )
{
    gint result = gtk_gui_run_property_dialog( "Controller Settings", controller_panel_new() );
    if( result == GTK_RESPONSE_ACCEPT ) {
	controller_commit_changes();
    } else {
	controller_cancel_changes();
    }
}
