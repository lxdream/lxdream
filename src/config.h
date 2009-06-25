/**
 * $Id$
 *
 * User configuration support
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

#ifndef lxdream_config_H
#define lxdream_config_H 1

#include <glib/gtypes.h>
#include <glib/glist.h>
#include "gettext.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_TYPE_NONE 0
#define CONFIG_TYPE_FILE 1
#define CONFIG_TYPE_PATH 2
#define CONFIG_TYPE_KEY 3
#define CONFIG_TYPE_FILELIST 4
#define CONFIG_TYPE_INTEGER 5

#define DEFAULT_CONFIG_FILENAME "lxdreamrc"

typedef struct lxdream_config_entry {
    const gchar *key;
    const gchar *label; // i18n 
    const int type;
    const gchar *default_value;
    gchar *value;
} *lxdream_config_entry_t;

typedef struct lxdream_config_group {
    const gchar *key;
    struct lxdream_config_entry *params;
} *lxdream_config_group_t;

#define CONFIG_BIOS_PATH 0
#define CONFIG_FLASH_PATH 1
#define CONFIG_DEFAULT_PATH 2
#define CONFIG_SAVE_PATH 3
#define CONFIG_VMU_PATH 4
#define CONFIG_BOOTSTRAP 5
#define CONFIG_GDROM 6
#define CONFIG_RECENT 7
#define CONFIG_VMU 8
#define CONFIG_QUICK_STATE 9
#define CONFIG_KEY_MAX CONFIG_QUICK_STATE

extern struct lxdream_config_group lxdream_config_root[];

/* Global config values */
const gchar *lxdream_get_global_config_value( int key );
const struct lxdream_config_entry * lxdream_get_global_config_entry( int key );
void lxdream_set_global_config_value( int key, const gchar *value );

void lxdream_register_config_group( const gchar *key, lxdream_config_entry_t group ); 
void lxdream_set_config_value( lxdream_config_entry_t entry, const gchar *value );
gboolean lxdream_set_group_value( lxdream_config_group_t group, const gchar *key, const gchar *value );
void lxdream_copy_config_list( lxdream_config_entry_t dest, lxdream_config_entry_t src );

/**
 * Return a fully expanded path value for a key - this performs substitutions
 * for ~ and simple shell variables ($VAR and ${VAR})
 * 
 * The returned string is newly allocated and must be freed by the caller
 */
gchar *lxdream_get_global_config_path_value( int key );

/**
 * Set a path value for a key, escaping if necessary to protect against 
 * shell-substitution on the round trip.
 * 
 * @return the resultant config value.
 */
const gchar *lxdream_set_global_config_path_value( int key, const gchar *value );

/**
 * Construct a list of strings for the given config key - The caller is 
 * responsible for freeing the list and its values.
 */
GList *lxdream_get_global_config_list_value( int key );

/**
 * Set a config key based on a list of strings. 
 */
void lxdream_set_global_config_list_value( int key, const GList *list );
/**
 * Search the standard locations for the configuration file:
 *   $HOME/.lxdreamrc
 *   $CWD/lxdreamrc
 *   $SYSCONF_DIR/lxdreamrc
 * @return TRUE if the file was found, otherwise FALSE.
 */
gboolean lxdream_find_config( );

/**
 * Set the configuration file filename to the supplied string.
 * The string is copied internally (ie can be released by the
 * caller).
 */
void lxdream_set_config_filename( const gchar *filename );

/**
 * Load the configuration from the previously determined filename.
 */
gboolean lxdream_load_config( );

/**
 * Update the configuration
 */
gboolean lxdream_save_config( );

/**
 * Make the user configuration directories if they don't already exist.
 */ 
void lxdream_make_config_dir( );

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

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_config_H */
