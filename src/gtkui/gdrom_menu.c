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
#include "dream.h"
#include "dreamcast.h"
#include "config.h"
#include "gdlist.h"
#include "gdrom/gdrom.h"
#include "gtkui/gtkui.h"

static gboolean gdrom_menu_adjusting = FALSE;

static void gdrom_menu_open_image_callback( GtkWidget *widget, gpointer user_data )
{
    if( !gdrom_menu_adjusting ) {
        const gchar *dir = lxdream_get_config_value(CONFIG_DEFAULT_PATH);
        open_file_dialog( _("Open..."), gdrom_mount_image, NULL, NULL, dir );
    }
}

void gdrom_menu_item_callback( GtkWidget *widget, gpointer user_data )
{
    if( !gdrom_menu_adjusting ) {
        gdrom_list_set_selection( GPOINTER_TO_INT(user_data) );
    }
}

void gdrom_menu_build( GtkWidget *menu ) 
{
    unsigned int i, len;
    GSList *group = NULL;

    len = gdrom_list_size();
    for( i=0; i < len; i++ ) {
        const gchar *entry = gdrom_list_get_display_name(i);
        if( entry[0] == '\0' ) { // Empty string = separator
            gtk_menu_shell_append( GTK_MENU_SHELL(menu), gtk_separator_menu_item_new() );
        } else {
            GtkWidget *item = gtk_radio_menu_item_new_with_label( group, entry );
            group = gtk_radio_menu_item_get_group( GTK_RADIO_MENU_ITEM(item) );
            g_signal_connect_after( item, "activate", G_CALLBACK(gdrom_menu_item_callback), GINT_TO_POINTER(i) );
            gtk_menu_shell_append( GTK_MENU_SHELL(menu), item );
        }
    }

    gtk_menu_shell_append( GTK_MENU_SHELL(menu), gtk_separator_menu_item_new() );
    GtkWidget *open = gtk_image_menu_item_new_with_label( _("Open image file...") );
    g_signal_connect_after( open, "activate", G_CALLBACK(gdrom_menu_open_image_callback), NULL );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), open );
    gtk_widget_show_all(menu);
}

void gdrom_menu_rebuild( GtkWidget *menu )
{
    GList *children = gtk_container_get_children( GTK_CONTAINER(menu) );
    GList *listptr;
    for( listptr = children; listptr != NULL; listptr = g_list_next(listptr) ) {
        gtk_widget_destroy( GTK_WIDGET(listptr->data) );
    }
    g_list_free(children);
    gdrom_menu_build(menu);
}

gboolean gdrom_menu_update( gboolean list_changed, int selection, void *user_data )
{
    gdrom_menu_adjusting = TRUE;
    GtkWidget *menu = GTK_WIDGET(user_data);

    if( list_changed ) {
        gdrom_menu_rebuild(menu);
    }

    GList *children = gtk_container_get_children( GTK_CONTAINER(menu) );
    GList *item = g_list_nth( children, selection );
    assert( item != NULL );
    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(item->data), TRUE );
    g_list_free(children);

    gdrom_menu_adjusting = FALSE;
    return TRUE;
}

GtkWidget *gdrom_menu_new()
{
    GtkWidget *menu = gtk_menu_new();
    gtk_menu_set_title( GTK_MENU(menu), _("GD-Rom Settings") );

    gdrom_menu_build(menu);
    register_gdrom_list_change_hook(gdrom_menu_update, menu);
    gdrom_menu_update( FALSE, gdrom_list_get_selection(), menu );
    gtk_widget_show_all(menu);

    return menu;
}
