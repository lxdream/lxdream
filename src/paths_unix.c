/**
 * $Id$
 *
 * Wrappers for system-dependent functions (mainly path differences)
 *
 * Copyright (c) 2008 Nathan Keynes.
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

#include "lxdream.h"
#include "config.h"

const char *get_sysconf_path()
{
    return PACKAGE_CONF_DIR;
}

const char *get_locale_path()
{
    return PACKAGE_LOCALE_DIR;
}

const char *get_plugin_path()
{
    return PACKAGE_PLUGIN_DIR;
}

static char *user_data_path = NULL;

const char *get_user_data_path()
{
    if( user_data_path == NULL ) {
        char *home = getenv("HOME");
        user_data_path = g_strdup_printf( "%s/.lxdream", home );
    }
    return user_data_path;
}

void set_user_data_path( const char *p ) 
{
    g_free(user_data_path);
    user_data_path = g_strdup(p);
}

const char *get_user_home_path()
{
    return getenv("HOME");
}