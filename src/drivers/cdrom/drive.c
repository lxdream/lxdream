/**
 * $Id$
 *
 * Host CD/DVD drive support.
 *
 * Copyright (c) 2009 Nathan Keynes.
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

#include <stdlib.h>
#include <glib/gstrfuncs.h>
#include <glib/gmem.h>
#include "drivers/cdrom/drive.h"
#include "drivers/cdrom/cdimpl.h"

static GList *cdrom_drive_list;

static cdrom_drive_t cdrom_drive_new( const char *name, const char *display_name, cdrom_drive_open_fn_t open_fn )
{
    cdrom_drive_t drive = g_malloc0( sizeof(struct cdrom_drive) );
    drive->name = g_strdup(name);
    drive->display_name = g_strdup(display_name);
    drive->open = open_fn;
    return drive;
}

static void cdrom_drive_destroy( cdrom_drive_t drive )
{
    g_free( (char *)drive->name );
    drive->name = NULL;
    g_free( (char *)drive->display_name );
    drive->display_name = NULL;
    g_free( drive );
}

GList *cdrom_drive_get_list()
{
    return cdrom_drive_list;
}

cdrom_drive_t cdrom_drive_add( const char *name, const char *display_name, cdrom_drive_open_fn_t open_fn )
{
    for( GList *ptr = cdrom_drive_list; ptr != NULL; ptr = ptr->next ) {
        cdrom_drive_t it = (cdrom_drive_t)ptr->data;
        if( strcmp(it->name, name) == 0 ) {
            return it;
        }
    }

    cdrom_drive_t new_drive = cdrom_drive_new(name,display_name,open_fn);
    cdrom_drive_list = g_list_append( cdrom_drive_list, new_drive );
    return new_drive;
}

gboolean cdrom_drive_remove( const char *name )
{
    for( GList *ptr = cdrom_drive_list; ptr != NULL; ptr = ptr->next ) {
        cdrom_drive_t it = (cdrom_drive_t)ptr->data;
        if( strcmp(it->name, name) == 0 ) {
            cdrom_drive_list = g_list_delete_link( cdrom_drive_list, ptr );
            cdrom_drive_destroy(it);
            return TRUE;
        }
    }
    return FALSE;
}

void cdrom_drive_remove_all()
{
    for( GList *ptr = cdrom_drive_list; ptr != NULL; ptr = ptr->next ) {
        cdrom_drive_destroy( (cdrom_drive_t)ptr->data );
    }
    g_list_free(cdrom_drive_list);
    cdrom_drive_list = NULL;
}

cdrom_drive_t cdrom_drive_get_index( unsigned int index )
{
    return (cdrom_drive_t)g_list_nth_data(cdrom_drive_list, index);
}

cdrom_disc_t cdrom_drive_open( cdrom_drive_t drive, ERROR *err )
{
    return drive->open(drive, err);
}

cdrom_drive_t cdrom_drive_find( const char *name )
{
    const char *id = name;

    /* If we have no drives, just return NULL without looking, to save time */
    if( cdrom_drive_list == NULL )
        return NULL;

    /* Check for a url-style name */
    const char *lizard_lips = strstr( name, "://" );
    if( lizard_lips != NULL ) {
        id = lizard_lips + 3;
        int method_len = (lizard_lips-name);
        if( method_len > 8 )
            return NULL;

        char method[method_len + 1];
        memcpy( method, name, method_len );
        method[method_len] = '\0';

        if( strcasecmp( method, "file" ) != 0 &&
            strcasecmp( method, "dvd" ) != 0 &&
            strcasecmp( method, "cd" ) != 0 &&
            strcasecmp( method, "cdrom" ) ) {
            /* Anything else we don't try to recognize */
            return NULL;
        }

        if( *id == '\0' ) {
            /* Accept eg  'dvd://' as meaning 'the first cd/dvd device */
            return cdrom_drive_list->data;
        }

        char *endp = NULL;
        unsigned long index = strtoul( id, &endp, 10 );
        if( endp != NULL && *endp == '\0' ) {
            /* Accept eg 'dvd://2' as meaning 'the second cd/dvd device */
            return cdrom_drive_get_index(index);
        }

        /* Otherwise it must be a drive identifier, so treat it as if it didn't
         * have the url prefix. (fallthrough)
         */
    }

    for( GList *ptr = cdrom_drive_list; ptr != NULL; ptr = ptr->next ) {
        cdrom_drive_t drive = (cdrom_drive_t)ptr->data;
        if( strcmp(drive->name, id) == 0 )
            return drive;
    }

    return NULL;
}
