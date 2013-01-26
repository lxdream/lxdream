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
#include <stdlib.h>
#include <glib.h>

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
    char result[PATH_MAX];

    char *d, *e;
    const char *s;
    d = result;
    e = result+sizeof(result)-1;
    s = input;
    gboolean inDQstring = FALSE;

    if( input == NULL )
        return NULL;

    while( *s ) {
        if( d == e ) {
            return g_strdup(input); /* expansion too long */
        }
        char c = *s++;
        if( c == '$' ) {
            if( *s == '{' ) {
                s++;
                const char *q = s;
                while( *q != '}' ) {
                    if( ! *q ) {
                        return g_strdup(input); /* unterminated variable */
                    }
                    q++;
                }
                char *tmp = g_strndup(s, (q-s));
                s = q+1;
                char *value = getenv(tmp);
                g_free(tmp);
                if( value != NULL ) {
                    int len = strlen(value);
                    if( d + len > e )
                        return g_strdup(input);
                    strcpy(d, value);
                    d+=len;
                } /* Else, empty string */
            } else {
                const char *q = s;
                while( isalnum(*q) || *q == '_' ) {
                    q++;
                }
                if( q == s ) {
                    *d++ = '$';
                } else {
                    char *tmp = g_strndup(s,q-s);
                    s = q;
                    char *value = getenv(tmp);
                    g_free(tmp);
                    if( value != NULL ) {
                        int len = strlen(value);
                        if( d + len > e )
                            return g_strdup(input);
                        strcpy(d, value);
                        d += len;
                    }
                }
            }
        } else if( c == '\\' ) {
            c = *s++;
            if( c ) {
                *d++ = c;
            } else {
                *d++ = '\\';
            }
        } else if( c == '\"' ) {
            /* Unescaped double-quotes start a DQ string. Although we treat the
             * string as if it were double-quoted for most purposes anyway, so
             * this has little effect.
             */
            inDQstring = !inDQstring;
        } else {
            *d++ = c;
        }
    }
    *d = '\0';
    if( inDQstring ) {
        WARN( "Unterminated double-quoted string '%s'", input );
    }
    return g_strdup(result);
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
