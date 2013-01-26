/**
 * $Id$
 *
 * VMU management - maintains a list of all known VMUs
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

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include "vmulist.h"
#include "config.h"

DEFINE_HOOK(vmulist_change_hook, vmulist_change_hook_t);

typedef struct vmulist_entry {
    const gchar *filename;
    vmu_volume_t vol;
    int attach_count;
} *vmulist_entry_t;

/** 
 * Doubly-linked list of vmulist_entry_t maintained in sorted order by display name.
 * Could be augmented with a hashtable if it gets too long
 */
static GList *vmu_list;

#define ENTRY(it) ((vmulist_entry_t)(it)->data)
#define VOLUME(it) (ENTRY(it)->vol)
#define DISPLAY_NAME(it) vmulist_display_name(ENTRY(it))

static const char *vmulist_display_name(vmulist_entry_t ent)
{
    if( ent->filename == NULL ) {
        return NULL;
    }
    const char *s = strrchr(ent->filename, '/' );
    if( s == NULL || *(s+1) == '\0' ) {
        return ent->filename;
    } else {
        return s+1;
    }
}

static void vmulist_update_config( void ) 
{
    GList *temp = NULL, *it;
    
    for( it = vmu_list; it != NULL; it = g_list_next(it) ) {
        vmulist_entry_t entry = ENTRY(it);
        temp = g_list_append( temp, (char *)entry->filename );
    }
    lxdream_set_global_config_list_value( CONFIG_VMU, temp );
    g_list_free( temp );
}

static vmulist_entry_t vmulist_get_entry_by_name( const gchar *name )
{
    GList *it;
    for( it = vmu_list; it != NULL; it = g_list_next(it) ) {
        const gchar *vmu_name = DISPLAY_NAME(it);
        if( name == NULL ? vmu_name == NULL : vmu_name != NULL && strcmp( vmu_name, name ) == 0 ) {
            return ENTRY(it);
        }
    }
    return NULL; // not found
}

static vmulist_entry_t vmulist_get_entry_by_filename( const gchar *name )
{
    GList *it;
    for( it = vmu_list; it != NULL; it = g_list_next(it) ) {
        const gchar *vmu_name = ENTRY(it)->filename;
        if( name == NULL ? vmu_name == NULL : vmu_name != NULL && strcmp( vmu_name, name ) == 0 ) {
            return ENTRY(it);
        }
    }
    return NULL; // not found
}

static vmulist_entry_t vmulist_get_entry_by_volume( vmu_volume_t vol )
{
    GList *it;
    for( it = vmu_list; it != NULL; it = g_list_next(it) ) {
        if( VOLUME(it) == vol ) {
            return ENTRY(it);
        }
    }
    return NULL; // not found
}

static vmulist_entry_t vmulist_get_entry_by_index( unsigned int index )
{
    return (vmulist_entry_t)g_list_nth_data(vmu_list,index);
}

static gint vmulist_display_name_compare( gconstpointer a, gconstpointer b )
{
    const char *aname = vmulist_display_name((vmulist_entry_t)a);    
    const char *bname = vmulist_display_name((vmulist_entry_t)b);
    if( aname == bname )
        return 0;
    if( aname == NULL ) 
        return -1;
    if( bname == NULL )
        return 1;
    return strcmp(aname,bname);
}

/** 
 * Add a new entry into the list, maintaining the sorted order.
 * If the filename is already in the list, it is updated instead.
 */ 
static vmulist_entry_t vmulist_add_entry( const gchar *filename, vmu_volume_t vol )
{
    vmulist_entry_t entry = vmulist_get_entry_by_filename(filename);
    if( entry == NULL ) {
        entry = g_malloc( sizeof(struct vmulist_entry) );
        entry->filename = g_strdup(filename);
        entry->vol = vol;
        vmu_list = g_list_insert_sorted(vmu_list, entry, vmulist_display_name_compare );
        vmulist_update_config();

        CALL_HOOKS( vmulist_change_hook, VMU_ADDED, g_list_index(vmu_list,entry) );
    } else {
        if( entry->vol != vol && entry->vol != NULL )
            vmu_volume_destroy( entry->vol );
        entry->vol = vol;
        /* NOTE: at the moment this can't require a resort, but if we allow
         * user-editable display names it will
         */ 
    }
    entry->attach_count = 0;
    
    return entry;
}

static void vmulist_remove_entry( vmulist_entry_t entry )
{
    int idx = g_list_index(vmu_list, entry);
    vmu_list = g_list_remove( vmu_list, entry );
    g_free( (char *)entry->filename );
    g_free( entry );
    vmulist_update_config();
    CALL_HOOKS( vmulist_change_hook, VMU_REMOVED, idx );
}

static unsigned int vmulist_get_index( vmulist_entry_t entry )
{
    return g_list_index( vmu_list, entry );
}

int vmulist_add_vmu( const gchar *filename, vmu_volume_t vol )
{
    vmulist_entry_t entry = vmulist_add_entry( filename, vol );
    return vmulist_get_index(entry);
}

void vmulist_remove_vmu( vmu_volume_t vol )
{
    vmulist_entry_t entry = vmulist_get_entry_by_volume(vol);
    if( entry != NULL ) {
        vmulist_remove_entry(entry);
    }
}

const char *vmulist_get_name( unsigned int idx )
{
    vmulist_entry_t entry = vmulist_get_entry_by_index(idx);
    if( entry != NULL ) {
        return vmulist_display_name(entry);
    }
    return NULL;
}

const char *vmulist_get_filename( unsigned int idx )
{
    vmulist_entry_t entry = vmulist_get_entry_by_index(idx);
    if( entry != NULL ) {
        return entry->filename;
    }
    return NULL;
}

const char *vmulist_get_volume_name( vmu_volume_t vol )
{
    vmulist_entry_t entry = vmulist_get_entry_by_volume(vol);
    if( entry != NULL ) {
        return entry->filename;
    } 
    return NULL;
}

vmu_volume_t vmulist_get_vmu( unsigned int idx )
{
    vmulist_entry_t entry = vmulist_get_entry_by_index(idx);
    if( entry != NULL ) {
        if( entry->vol == NULL ) {
            entry->vol = vmu_volume_load(entry->filename);
        }
        return entry->vol;
    }
    return NULL;
}

vmu_volume_t vmulist_get_vmu_by_name( const gchar *name )
{
    vmulist_entry_t entry = vmulist_get_entry_by_name(name);
    if( entry != NULL ) {
        if( entry->vol == NULL ) {
            entry->vol = vmu_volume_load(entry->filename);
        }
        return entry->vol;
    }
    return NULL;
}

vmu_volume_t vmulist_get_vmu_by_filename( const gchar *name )
{
    vmulist_entry_t entry = vmulist_get_entry_by_filename(name);
    if( entry != NULL ) {
        if( entry->vol == NULL ) {
            entry->vol = vmu_volume_load(entry->filename);
        }
        return entry->vol;
    } else {
        vmu_volume_t vol = vmu_volume_load( name );
        vmulist_add_entry( name, vol );
        return vol;
    }
}

int vmulist_get_index_by_filename( const gchar *name )
{
    vmulist_entry_t entry = vmulist_get_entry_by_filename(name);
    if( entry != NULL ) {
        return g_list_index( vmu_list, entry );
    }
    return -1;
}


int vmulist_create_vmu( const gchar *filename, gboolean create_only )
{
    vmu_volume_t vol = vmu_volume_new_default(filename);

    if( vmu_volume_save( filename, vol, create_only ) ) {
        return vmulist_add_vmu( filename, vol );
    } else {
        vmu_volume_destroy(vol);
    }
    return -1;
}

gboolean vmulist_attach_vmu( vmu_volume_t vol, const gchar *where )
{
    vmulist_entry_t entry = vmulist_get_entry_by_volume(vol);
    if( entry == NULL ) {
        return FALSE;
    }
    entry->attach_count++;
    return TRUE;
}

void vmulist_detach_vmu( vmu_volume_t vol )
{
    vmulist_entry_t entry = vmulist_get_entry_by_volume(vol);
    if( entry != NULL && entry->attach_count > 0 ) {
        entry->attach_count--;
    }
}

unsigned int vmulist_get_size(void)
{
    return g_list_length(vmu_list);
}

void vmulist_init( void )
{
    GList *filenames = lxdream_get_global_config_list_value( CONFIG_VMU );
    GList *ptr;
    for( ptr = filenames; ptr != NULL; ptr = g_list_next(ptr) ) {
        vmulist_add_entry( (gchar *)ptr->data, NULL );
        g_free( ptr->data );
    }
    g_list_free( filenames );
}

void vmulist_save_all( void )
{
    GList *it;
    for( it = vmu_list; it != NULL; it = g_list_next(it) ) {
        vmulist_entry_t entry = ENTRY(it);
        if( entry->vol != NULL && vmu_volume_is_dirty(entry->vol) ) {
            vmu_volume_save(entry->filename, entry->vol, FALSE);
        }
    }
}

void vmulist_shutdown( void )
{
    vmulist_save_all();
}
