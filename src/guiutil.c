/**
 * $Id$
 *
 * GUI helper functions that aren't specific to any particular implementation.
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

#include <unistd.h>
#include <glib/gstrfuncs.h>
#include <glib/gutils.h>

#include "gui.h"
#include "config.h"

static gchar *gui_paths[CONFIG_KEY_MAX];

gchar *get_absolute_path( const gchar *in_path )
{
    char tmp[PATH_MAX];
    if( in_path == NULL ) {
        return NULL;
    }
    if( in_path[0] == '/' || in_path[0] == 0 ) {
        return g_strdup(in_path);
    } else {
        getcwd(tmp, sizeof(tmp));
        return g_strdup_printf("%s%c%s", tmp, G_DIR_SEPARATOR, in_path);
    }
}

const gchar *gui_get_configurable_path( int key )
{
    if( gui_paths[key] == NULL ) {
        gui_paths[key] = lxdream_get_global_config_path_value(key);
        /* If no path defined, go with the current working directory */
        if( gui_paths[key] == NULL ) {
            gui_paths[key] = get_absolute_path(".");
        }
    }
    return gui_paths[key];
}

void gui_set_configurable_path( int key, const gchar *path )
{
    g_free(gui_paths[key]);
    gui_paths[key] = g_strdup(path);
}

void gui_config_paths_changed()
{
    int i;
    for( i=0; i < CONFIG_KEY_MAX; i++ ) {
        g_free(gui_paths[i]);
        gui_paths[i] = NULL;
    }
}
