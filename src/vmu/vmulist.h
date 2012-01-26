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


#ifndef lxdream_vmulist_H
#define lxdream_vmulist_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "hook.h"
#include "vmu/vmuvol.h"

typedef enum { VMU_ADDED, VMU_MODIFIED, VMU_REMOVED } vmulist_change_type_t;
    
/* Hook for notification of list change events */
typedef gboolean (*vmulist_change_hook_t)(vmulist_change_type_t change, int rowidx, void *user_data);
DECLARE_HOOK(vmulist_change_hook, vmulist_change_hook_t);

vmu_volume_t vmulist_get_vmu(unsigned int index);

/** Retrieve a known vmu by name */
vmu_volume_t vmulist_get_vmu_by_name(const gchar *name);

/** Retrieve a vmu by filename. The filename/vmu will be added to the list if it's
 * not already in it.
 */
vmu_volume_t vmulist_get_vmu_by_filename(const gchar *name);

const char *vmulist_get_name(unsigned int index);

const char *vmulist_get_filename(unsigned int index);

const char *vmulist_get_volume_name( vmu_volume_t vol );



/** Mark a VMU as being attached.
 * @return FALSE if the VMU was already attached, otherwise TRUE
 */
gboolean vmulist_attach_vmu( vmu_volume_t vol, const gchar *where );

/** Mark a VMU as detached. */
void vmulist_detach_vmu( vmu_volume_t vol );

/** 
 * Create a new VMU at the given filename, and add it to the list 
 * @param filename to save the new VMU as
 * @param create_only if TRUE, the file must not already exist. If FALSE,
 *   the create will overwrite any existing file at that filename.
 * @return index of the VMU in the list, or -1 if the call failed.
 **/
int vmulist_create_vmu(const gchar *filename, gboolean create_only);

/** Add a VMU volume to the list. Returns the index of the added volume */
int vmulist_add_vmu(const gchar *filename, vmu_volume_t vol);

int vmulist_get_index_by_filename( const gchar *name );

/** Remove a VMU volume from the list */
void vmulist_remove_vmu(vmu_volume_t vol);

/** Initialize the list */
void vmulist_init(void);

/** Save all VMUs in the list (actually only ones which have been written to
 * some point)
 **/
void vmulist_save_all(void);

void vmulist_shutdown(void);

unsigned int vmulist_get_size(void);

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_vmulist_H */
