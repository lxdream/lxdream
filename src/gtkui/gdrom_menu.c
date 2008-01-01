/**
 * $Id$
 *
 * Creates and manages the GD-Rom attachment menu.
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
#include <libgen.h>
#include <glib/gi18n.h>

#include "dream.h"
#include "dreamcast.h"
#include "config.h"
#include "gdrom/gdrom.h"
#include "gtkui/gtkui.h"

#define MAX_RECENT_ITEMS 5

static GList *gdrom_menu_list = NULL;
static gboolean gdrom_menu_adjusting = FALSE;
static GList *gdrom_device_list = NULL;
static GList *gdrom_recent_list = NULL;

void gdrom_menu_rebuild_all();


gint gdrom_menu_find_item( const gchar *name )
{
    gint posn = 0;
    GList *ptr;

    for( ptr = gdrom_device_list; ptr != NULL; ptr = g_list_next(ptr) ) {
	gchar *device = (gchar *)ptr->data;
	posn++;
	if( strcmp(device, name) == 0 ) {
	    return posn;
	}
    }
    posn++;
    for( ptr = gdrom_recent_list; ptr != NULL; ptr = g_list_next(ptr) ) {
	gchar *file = (gchar *)ptr->data;
	posn++;
	if( strcmp(file, name) == 0 ) {
	    return posn;
	}
    }
    return -1;
}

gint gdrom_menu_add_recent_item( const gchar *name )
{
    gdrom_recent_list = g_list_prepend( gdrom_recent_list, g_strdup(name) );
    if( g_list_length(gdrom_recent_list) > MAX_RECENT_ITEMS ) {
	GList *ptr = g_list_nth( gdrom_recent_list, MAX_RECENT_ITEMS );
	g_free( ptr->data );
	gdrom_recent_list = g_list_remove( gdrom_recent_list, ptr->data );
    }

    GList *ptr;
    int size = 0;
    for( ptr = gdrom_recent_list; ptr != NULL; ptr = g_list_next(ptr) ) {
	size += strlen( (gchar *)ptr->data ) + 1;
    }
    char buf[size];
    strcpy( buf, (gchar *)gdrom_recent_list->data );
    ptr = g_list_next(gdrom_recent_list);
    while( ptr != NULL ) {
	strcat( buf, ":" );
	strcat( buf, (gchar *)ptr->data );
	ptr = g_list_next(ptr);
    }
    lxdream_set_global_config_value( CONFIG_RECENT, buf );
    lxdream_save_config();

    return g_list_length( gdrom_device_list ) + 2; // menu posn of new item
}

void gdrom_menu_update_all()
{
    gdrom_disc_t disc = gdrom_get_current_disc();
    gint posn = 0;
    GList *ptr;

    gdrom_menu_adjusting = TRUE;

    if( disc != NULL ) {
	posn = gdrom_menu_find_item( disc->name );
	if( posn == -1 ) {
	    posn = gdrom_menu_add_recent_item( disc->name );
	    gdrom_menu_rebuild_all();
	}
    }

    for( ptr = gdrom_menu_list; ptr != NULL; ptr = g_list_next(ptr) ) {
	GtkWidget *menu = GTK_WIDGET(ptr->data);
	GList *children = gtk_container_get_children( GTK_CONTAINER(menu) );
	GList *item = g_list_nth( children, posn );
	assert( item != NULL );
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(item->data), TRUE );
	g_list_free(children);
    }    

    gdrom_menu_adjusting = FALSE;
}

void gdrom_menu_empty_callback( GtkWidget *widget, gpointer user_data )
{
    if( !gdrom_menu_adjusting ) {
	gdrom_unmount_disc();
	gdrom_menu_update_all();
	lxdream_set_global_config_value( CONFIG_GDROM, NULL );
	lxdream_save_config();
    }
}

gboolean gdrom_menu_open_file( const char *filename )
{
    gboolean result = FALSE;
    if( filename != NULL ) {
	result = gdrom_mount_image(filename);
    }
    if( result ) {
	gdrom_menu_update_all();
	lxdream_set_global_config_value( CONFIG_GDROM, filename );
	lxdream_save_config();
    }
    return result;
}

void gdrom_menu_open_image_callback( GtkWidget *widget, gpointer user_data )
{
    if( !gdrom_menu_adjusting ) {
	const gchar *dir = lxdream_get_config_value(CONFIG_DEFAULT_PATH);
	open_file_dialog( _("Open..."), gdrom_menu_open_file, NULL, NULL, dir );
    }
}


void gdrom_menu_open_specified_callback( GtkWidget *widget, gpointer user_data )
{
    if( !gdrom_menu_adjusting ) {
	gdrom_menu_open_file( (gchar *)user_data );
    }
}

void gdrom_menu_build( GtkWidget *menu ) 
{
    GSList *group = NULL;
    GtkWidget *empty = gtk_radio_menu_item_new_with_label( group, _("Empty") );
    group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM(empty) );
    g_signal_connect_after( empty, "activate", G_CALLBACK(gdrom_menu_empty_callback), NULL );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), empty );
    
    GList *ptr;
    for( ptr = gdrom_device_list; ptr != NULL; ptr = g_list_next(ptr) ) {
	gchar *name = (gchar *)ptr->data;
	GtkWidget *item = gtk_radio_menu_item_new_with_label( group, name);
	gtk_widget_set_name( item, name );
	group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM(item) );
	g_signal_connect_after( item, "activate", G_CALLBACK(gdrom_menu_open_specified_callback),
			  name );
	gtk_menu_shell_append( GTK_MENU_SHELL(menu), item );
    }

    if( gdrom_recent_list != NULL ) {
	gtk_menu_shell_append( GTK_MENU_SHELL(menu), gtk_separator_menu_item_new() );
	for( ptr = gdrom_recent_list; ptr != NULL; ptr = g_list_next(ptr) ) {
	    gchar *path = (gchar *)ptr->data;
	    gchar *name = basename(path);
	    GtkWidget *item = gtk_radio_menu_item_new_with_label( group, name );
	    gtk_widget_set_name( item, path );
	    group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM(item) );
	    g_signal_connect_after( item, "activate", G_CALLBACK(gdrom_menu_open_specified_callback),
				    path );
	    gtk_menu_shell_append( GTK_MENU_SHELL(menu), item );
	    
	}
    }
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), gtk_separator_menu_item_new() );
    GtkWidget *open = gtk_image_menu_item_new_with_label( _("Open image file...") );
    g_signal_connect_after( open, "activate", G_CALLBACK(gdrom_menu_open_image_callback), NULL );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), open );
    gtk_widget_show_all(menu);
}

GtkWidget *gdrom_menu_new()
{
    GtkWidget *menu = gtk_menu_new();
    gtk_menu_set_title( GTK_MENU(menu), _("GD-Rom Settings") );

    gdrom_menu_build(menu);

    gdrom_menu_list = g_list_append(gdrom_menu_list, menu);
    gtk_widget_show_all(menu);
    gdrom_menu_update_all();
    return menu;
}

void gdrom_menu_rebuild_all()
{
    GList *ptr;

    for( ptr = gdrom_menu_list; ptr != NULL; ptr = g_list_next(ptr) ) {
	GtkWidget *menu = GTK_WIDGET(ptr->data);
	GList *children = gtk_container_get_children( GTK_CONTAINER(menu) );
	GList *listptr;
	for( listptr = children; listptr != NULL; listptr = g_list_next(listptr) ) {
	    gtk_widget_destroy( GTK_WIDGET(listptr->data) );
	}
	g_list_free(children);
	gdrom_menu_build(menu);
    }
    gdrom_menu_update_all();
}

void gdrom_menu_init()
{
    const gchar *recent = lxdream_get_config_value( CONFIG_RECENT );
    gdrom_device_list = gdrom_get_native_devices();
    if( recent != NULL ) {
	gchar **list = g_strsplit(recent, ":", 5);
	int i;
	for( i=0; list[i] != NULL; i++ ) {
	    gdrom_recent_list = g_list_append( gdrom_recent_list, g_strdup(list[i]) );
	}
	g_strfreev(list);
    }
}
