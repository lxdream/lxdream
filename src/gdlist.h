/**
 * $Id$
 *
 * GD-Rom list manager - maintains the list of recently accessed images and
 * available devices for the UI + config. 
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

#ifndef lxdream_gdlist_H
#define lxdream_gdlist_H 1

#include "hook.h"

typedef gboolean (*gdrom_list_change_hook_t)(gboolean list_changed, int selection, void *user_data);
DECLARE_HOOK(gdrom_list_change_hook, gdrom_list_change_hook_t);


/**
 * Initialize the gdrom list (registers with the gdrom driver, creates the 
 * initial lists, etc). Must be called exactly once before using the lists.
 */
void gdrom_list_init(void);

/**
 * Return the index of the currently selected GD-Rom item. If there is no disc
 * currently mounted, returns 0. 
 */ 
int gdrom_list_get_selection(void);

/**
 * Return the number of items in the list, including separators.
 */
int gdrom_list_size(void);

/**
 * Return the display name of the item at the specified index. If the
 * item is a separator, returns the empty string. If the index is out
 * of bounds, returns NULL. 
 * The list will currently follow the following structure:
 *   "Empty" (localised)
 *   Any CD/DVD drives attached to the system
 *   "" (empty string) - separator item
 *   An LRU list of disc image files (note without directory components).
 */
const gchar *gdrom_list_get_display_name(int index);

/**
 * Change the current gdrom selection to the selected index. This will mount the
 * appropriate drive/image where necessary.
 * @return TRUE if the selection was updated, FALSE if the position was invalid. 
 */
gboolean gdrom_list_set_selection(int posn);

#endif /* lxdream_gdlist_H */
