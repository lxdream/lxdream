/**
 * $Id$
 *
 * Various path definitions and helper functions
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

#ifndef lxdream_paths_H
#define lxdream_paths_H

/****************** System paths ****************/
/**
 * Location of the shared lxdreamrc (e.g. /usr/local/etc/lxdreamrc)
 */
const char *get_sysconf_path();

/**
 * Location of the message catalogs (e.g. /usr/local/share/locale)
 */
const char *get_locale_path();

/**
 * Location of the plugins, if any (e.g. /usr/local/lib/lxdream)
 */
const char *get_plugin_path();

/**
 * Location of the current user's data path (e.g. ~/.lxdream)
 */
const char *get_user_data_path();

/******************** Path helpers *****************/

/**
 * Escape a pathname if needed to prevent shell substitution.
 * @return a newly allocated string (or NULL if the input is NULL)
 */
gchar *get_escaped_path( const gchar *name );

/**
 * Expand a pathname according to standard shell substitution rules
 * (excluding command substitutions).
 * @return a newly allocated string (or NULL if the input is NULL)
 */
gchar *get_expanded_path( const gchar *name );

/**
 * Return an absolute path for the given input path, as a newly allocated
 * string. If the input path is already absolute, the returned string will
 * be identical to the input string.
 */
gchar *get_absolute_path( const gchar *path );

/**
 * Construct a filename in the same directory as the file at. That is,
 * if at is "/tmp/foo" and filename = "bar", the function returns 
 * "/tmp/bar".
 * @return a newly allocated string that must be released by the caller.
 */
gchar *get_filename_at( const gchar *at, const gchar *filename );


/********************* GUI Paths ***********************/
/* The following functions provide a cache for the most recently accessed
 * path for each config key (ie for GUI file open/save dialogs)
 */
/**
 * Get the path corresponding to the given global config key
 */
const gchar *get_gui_path(int key);

/**
 * Override the path for the given global config key, without changing the
 * underlying configuration.
 */
void set_gui_path( int key, const gchar *path );

/**
 * Notify the helper functions that the config paths have changed, in which
 * event they will revert to the config-specified versions.
 */
void reset_gui_paths();



#endif /* !lxdream_paths_H */
