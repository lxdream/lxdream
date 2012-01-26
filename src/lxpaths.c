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

#include <ctype.h>
#include <unistd.h>
#include <wordexp.h>
#include <glib/gstrfuncs.h>
#include <glib/gutils.h>

#include "gui.h"
#include "config.h"

static gchar *gui_paths[CONFIG_KEY_MAX];

/**
 * Test if we need to escape a path to prevent substitution mangling. 
 * @return TRUE if the input value contains any character that doesn't
 * match [a-zA-Z0-9._@%/] (this will escape slightly more than it needs to,
 * but is safe)
 */
static gboolean path_needs_escaping( const gchar *value )
{
   const gchar *p = value;
   while( *p ) {
       if( !isalnum(*p) && *p != '.' && *p != '_' &&
               *p != '@' && *p != '%' && *p != '/' ) {
           return TRUE;
       }
       p++;
   }
   return FALSE;
}

gchar *get_escaped_path( const gchar *value )
{
    if( value != NULL && path_needs_escaping(value) ) {
        /* Escape with "", and backslash the remaining characters:
         *   \ " $ `
         */
        char buf[strlen(value)*2+3];  
        const char *s = value;
        char *p = buf;
        *p++ = '\"';
        while( *s ) {
            if( *s == '\\' || *s == '"' || *s == '$' || *s == '`' ) {
                *p++ = '\\';
            }
            *p++ = *s++;
        }
        *p++ = '\"';
        *p = '\0';
        return g_strdup(buf);
    } else {
        return g_strdup(value);
    }
}

gchar *get_expanded_path( const gchar *input )
{
    wordexp_t we;
    if( input == NULL ) {
        return NULL;
    }
    memset(&we,0,sizeof(we));
    int result = wordexp(input, &we, WRDE_NOCMD);
    if( result != 0 || we.we_wordc == 0 ) {
        /* On failure, return the original input unchanged */
        return g_strdup(input);
    } else {
        /* On success, concatenate all 'words' together into a single 
         * space-separated string
         */
        int length = we.we_wordc, i;
        gchar *result, *p;
        
        for( i=0; i<we.we_wordc; i++ ) {
            length += strlen(we.we_wordv[i]);
        }
        p = result = g_malloc(length);
        for( i=0; i<we.we_wordc; i++ ) {
            if( i != 0 )
                *p++ = ' ';
            strcpy( p, we.we_wordv[i] );
            p += strlen(p);
        }
        wordfree(&we);
        return result;
    }        
}

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

gchar *get_filename_at( const gchar *at, const gchar *filename )
{
    char tmp[PATH_MAX];
    char *p = strrchr( at, '/' );
    if( p == NULL ) {
        /* No path at all, so just return filename */
        return g_strdup(filename);
    } else {
        int off = p-at;
        return g_strdup_printf("%.*s%c%s", off, at, G_DIR_SEPARATOR, filename );
    }
}

const gchar *get_gui_path( int key )
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

void set_gui_path( int key, const gchar *path )
{
    g_free(gui_paths[key]);
    gui_paths[key] = g_strdup(path);
}

void reset_gui_paths()
{
    int i;
    for( i=0; i < CONFIG_KEY_MAX; i++ ) {
        g_free(gui_paths[i]);
        gui_paths[i] = NULL;
    }
}
