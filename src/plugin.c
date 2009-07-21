/**
 * $Id$
 *
 * Plugin loader code
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

#include <sys/stat.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>
#include <glib/gmem.h>
#include <glib/gstrfuncs.h>
#include "plugin.h"
#include "lxpaths.h"

#ifdef APPLE_BUILD
#define SOEXT ".dylib"
#else
#define SOEXT ".so"
#endif

/** Dummy plugin used as a plugin directory marker */
#define DUMMY_PLUGIN ("lxdream_dummy" SOEXT) 

const char *plugin_type_string[] = { "undefined", "audio driver", "input driver" };

int main(int argc, char *argv[]);
static const char *exec_path = NULL;

/**
 * Return the full path to the main binary
 */
static const char *get_exec_path()
{
    if( exec_path == NULL ) {
        Dl_info dli;

        /* Use dladdr for this, since it should be available for any platform
         * that we can support plugins on at all.
         */
        if( dladdr( main, &dli) ) {
            gchar *path = g_strdup( dli.dli_fname );
            char *i = strrchr( path, '/' );
            if( i > path ) {
                *i = '\0';
                exec_path = path;
            } else {
                g_free(path);
            }
        }
    }
    return exec_path;
}
    
static gboolean plugin_load( const gchar *plugin_path )
{
    void *so = dlopen(plugin_path, RTLD_NOW|RTLD_LOCAL);
    if( so == NULL ) {
        WARN("Failed to load plugin '%s': %s", plugin_path, dlerror());
        return FALSE;
    }
    
    struct plugin_struct *plugin = (struct plugin_struct *)dlsym(so,"lxdream_plugin_entry");
    if( plugin == NULL ) {
        WARN("Failed to load plugin: '%s': Not an lxdream plugin", plugin_path);
        dlclose(so);
        return FALSE;
    }

    if( strcmp(lxdream_short_version, plugin->version) != 0 ) {
        WARN("Failed to load plugin: '%s': Incompatible version (%s)", plugin_path, plugin->version);
        dlclose(so);
        return FALSE;
    }

    if( plugin->type == PLUGIN_NONE ) {
        /* 'dummy' plugin - we don't actually want to load it */
        dlclose(so);
        return FALSE;
    }
    
    if( plugin->type < PLUGIN_MIN_TYPE || plugin->type > PLUGIN_MAX_TYPE ) {
        WARN("Failed to load plugin: '%s': Unrecognized plugin type (%d)", plugin_path, plugin->type );
        dlclose(so);
        return FALSE;
    }

    if( plugin->register_plugin() == FALSE ) {
        WARN("Failed to load plugin: '%s': Initialization failed", plugin_path);
        dlclose(so);
        return FALSE;
    }
    INFO("Loaded %s '%s'", plugin_type_string[plugin->type], plugin->name);
    return TRUE;
}

static gboolean has_plugins( const gchar *path )
{
    struct stat st;
    
    gchar *dummy_name = g_strdup_printf( "%s/%s", path, DUMMY_PLUGIN );
    if( stat( dummy_name, &st ) == 0 ) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/**
 * Scan the plugin dir and load all valid plugins.
 */
static int plugin_load_all( const gchar *plugin_dir )
{
    int plugin_count = 0;
    struct dirent *ent;
    
    DIR *dir = opendir(plugin_dir);
    if( dir == NULL ) {
        WARN( "Unable to open plugin directory '%s'", plugin_dir );
        return 0;
    }
    
    while( (ent = readdir(dir)) != NULL ) {
        const char *ext = strrchr(ent->d_name, '.');
        if( ext != NULL && strcasecmp(SOEXT,ext) == 0 ) {
            char *libname = g_strdup_printf( "%s/%s", plugin_dir,ent->d_name );
            if( plugin_load( libname ) ) {
                plugin_count++;
            }
            g_free(libname);
        }
    }
    return plugin_count;
}

int plugin_init()
{
    const char *path = get_exec_path();
    if( path == NULL || !has_plugins(path) ) {
        path = get_plugin_path();
    }
    
    INFO( "Plugin directory: %s", path );
    return plugin_load_all( path );
}
