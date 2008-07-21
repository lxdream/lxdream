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
#include "gettext.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_TYPE_NONE 0
#define CONFIG_TYPE_FILE 1
#define CONFIG_TYPE_PATH 2
#define CONFIG_TYPE_KEY 3

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
#define CONFIG_BOOTSTRAP 4
#define CONFIG_GDROM 5
#define CONFIG_RECENT 6
#define CONFIG_KEY_MAX CONFIG_RECENT

extern struct lxdream_config_group lxdream_config_root[];

/* Global config values */
const gchar *lxdream_get_config_value( int key );
const struct lxdream_config_entry * lxdream_get_config_entry( int key );

void lxdream_set_global_config_value( int key, const gchar *value );
void lxdream_set_config_value( lxdream_config_entry_t entry, const gchar *value );
gboolean lxdream_set_group_value( lxdream_config_group_t group, const gchar *key, const gchar *value );
void lxdream_copy_config_list( lxdream_config_entry_t dest, lxdream_config_entry_t src );

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

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_config_H */
