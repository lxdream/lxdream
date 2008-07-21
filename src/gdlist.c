/**
 * $Id$
 *
 * GD-Rom list manager - maintains the list of recently accessed images and
 * available devices for the UI + config. 
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

#include <string.h>
#include <stdlib.h>
#include <glib/gstrfuncs.h>
#include <libgen.h>
#include "gettext.h"
#include "gdrom/gdrom.h"
#include "gdlist.h"
#include "lxdream.h"
#include "config.h"

#define MAX_RECENT_ITEMS 5

#define FIRST_RECENT_INDEX (gdrom_device_count+2)

DEFINE_HOOK(gdrom_list_change_hook, gdrom_list_change_hook_t);

static GList *gdrom_device_list = NULL;
static GList *gdrom_recent_list = NULL;
static unsigned int gdrom_device_count = 0, gdrom_recent_count = 0;

gint gdrom_list_find( const gchar *name )
{
    gint posn = 0;
    GList *ptr;

    for( ptr = gdrom_device_list; ptr != NULL; ptr = g_list_next(ptr) ) {
        gdrom_device_t device = (gdrom_device_t)ptr->data;
        posn++;
        if( strcmp(device->name, name) == 0 ) {
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

/**
 * Update the recent list in the lxdream config (but does not save)  
 */
void gdrom_list_update_config()
{
    GList *ptr;
    int size = 0;
    for( ptr = gdrom_recent_list; ptr != NULL; ptr = g_list_next(ptr) ) {
        size += strlen( (gchar *)ptr->data ) + 1;
    }
    char buf[size];
    strcpy( buf, (gchar *)gdrom_recent_list->data );
    for( ptr = g_list_next(gdrom_recent_list); ptr != NULL; ptr = g_list_next(ptr) ) {
        strcat( buf, ":" );
        strcat( buf, (gchar *)ptr->data );
    }
    lxdream_set_global_config_value( CONFIG_RECENT, buf );
}


void gdrom_list_add_recent_item( const gchar *name )
{
    gdrom_recent_list = g_list_prepend( gdrom_recent_list, g_strdup(name) );
    if( g_list_length(gdrom_recent_list) > MAX_RECENT_ITEMS ) {
        GList *ptr = g_list_nth( gdrom_recent_list, MAX_RECENT_ITEMS );
        g_free( ptr->data );
        gdrom_recent_list = g_list_remove( gdrom_recent_list, ptr->data );
    } else {
        gdrom_recent_count ++;
    }
    gdrom_list_update_config();
}

void gdrom_list_move_to_front( const gchar *name )
{
    GList *ptr;
    for( ptr = gdrom_recent_list; ptr != NULL; ptr = g_list_next(ptr) ) {
        gchar *file = (gchar *)ptr->data;
        if( strcmp(file, name) == 0 ) {
            gdrom_recent_list = g_list_delete_link( gdrom_recent_list, ptr );
            gdrom_recent_list = g_list_prepend( gdrom_recent_list, file );
            gdrom_list_update_config();
            return;
        }
    }
}

/**
 * Disc-changed callback from the GD-Rom driver. Updates the list accordingly.
 */
gboolean gdrom_list_disc_changed( gdrom_disc_t disc, const gchar *disc_name, void *user_data )
{
    gboolean list_changed = FALSE;
    int posn = 0;
    if( disc != NULL ) {
        posn = gdrom_list_find( disc_name );
        if( posn == -1 ) {
            gdrom_list_add_recent_item( disc_name );
            posn = FIRST_RECENT_INDEX;
            list_changed = TRUE;
        } else if( posn > FIRST_RECENT_INDEX ) {
            gdrom_list_move_to_front( disc_name );
            posn = FIRST_RECENT_INDEX;
            list_changed = TRUE;
        }
    }

    lxdream_set_global_config_value( CONFIG_GDROM, disc_name );
    lxdream_save_config();   

    CALL_HOOKS( gdrom_list_change_hook, list_changed, posn );
    return TRUE;
}

/**
 * Drives-changed callback from the host CD-Rom drivers. Probably not likely to
 * happen too often unless you're adding/removing external drives...
 */
void gdrom_list_drives_changed( GList *device_list )
{
}

/************ Public interface ***********/

void gdrom_list_init()
{
    const gchar *recent = lxdream_get_config_value( CONFIG_RECENT );
    register_gdrom_disc_change_hook( gdrom_list_disc_changed, NULL );
    gdrom_device_list = cdrom_get_native_devices();
    if( recent != NULL ) {
        gchar **list = g_strsplit(recent, ":", MAX_RECENT_ITEMS);
        int i;
        for( i=0; list[i] != NULL; i++ ) {
            gdrom_recent_list = g_list_append( gdrom_recent_list, g_strdup(list[i]) );
        }
        g_strfreev(list);
    }
    gdrom_device_count = g_list_length(gdrom_device_list);
    gdrom_recent_count = g_list_length(gdrom_recent_list);

    // Run the hooks in case anyone registered before the list was initialized
    CALL_HOOKS( gdrom_list_change_hook, TRUE, gdrom_list_get_selection() );
}

gboolean gdrom_list_set_selection( int posn )
{
    if( posn == 0 ) { // Always 'Empty'
        gdrom_unmount_disc();
        return TRUE;
    }

    if( posn <= gdrom_device_count ) {
        gdrom_device_t device = g_list_nth_data(gdrom_device_list, posn-1);
        return gdrom_mount_image(device->name);
    }

    posn -= FIRST_RECENT_INDEX;
    if( posn >= 0 && posn < gdrom_recent_count ) {
        gchar *entry = g_list_nth_data(gdrom_recent_list, posn);
        return gdrom_mount_image(entry);
    }

    return FALSE;
}

gint gdrom_list_get_selection( )
{
    const char *name = gdrom_get_current_disc_name();
    if( name == NULL ) {
        return 0;
    } else {
        return gdrom_list_find(name);
    }
}

int gdrom_list_size()
{
    return gdrom_device_count + gdrom_recent_count + 2;
}

const gchar *gdrom_list_get_display_name( int posn )
{
    if( posn == 0 ) {
        return _("Empty");
    }

    if( posn <= gdrom_device_count ) {
        gdrom_device_t device = g_list_nth_data(gdrom_device_list, posn-1);
        return device->device_name;
    }

    if( posn == gdrom_device_count + 1) {
        return "";
    }

    if( posn < 0 || posn > gdrom_list_size() ) {
        return NULL;
    }

    gchar *entry = g_list_nth_data(gdrom_recent_list, posn-FIRST_RECENT_INDEX);
    return basename(entry);
}

const gchar *gdrom_list_get_filename( int posn )
{
    if( posn == 0 ) {
        return _("Empty");
    }

    if( posn <= gdrom_device_count ) {
        gdrom_device_t device = g_list_nth_data(gdrom_device_list, posn-1);
        return device->name;
    }

    if( posn == gdrom_device_count + 1) {
        return "";
    }

    if( posn < 0 || posn > gdrom_list_size() ) {
        return NULL;
    }

    return g_list_nth_data(gdrom_recent_list, posn-FIRST_RECENT_INDEX);
}
