/**
 * $Id$
 *
 * General OS X IOKit support (primarily for cdrom support)
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

#ifndef lxdream_osx_iokit_H
#define lxdream_osx_iokit_H 1

#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include "lxdream.h"
#include "hook.h"

/**
 * CD-ROM drive visitor. Returns FALSE to continue iterating, TRUE if the desired CD-ROM
 * has been found. In the latter case, the io_object is returned from find_cdrom_device
 * (and not freed)
 */ 
typedef gboolean (*find_drive_callback_t)( io_object_t object, char *vendor, char *product,
        char *iopath, void *user_data );

/**
 * Search for a CD or DVD drive (instance of IODVDServices or IOCompactDiscServices).
 * The callback will be called repeatedly until either it returns TRUE, or all drives
 * have been iterated over.
 * 
 * @return an IO registry entry for the matched drive, or 0 if no drives matched.
 * 
 * Note: Use of IOCompactDiscServices is somewhat tentative since I don't have a Mac
 * with a CD-Rom drive.
 */ 
io_object_t find_cdrom_drive( find_drive_callback_t callback, void *user_data );

typedef struct osx_cdrom_drive *osx_cdrom_drive_t;

/**
 * Construct an osx_cdrom_drive_t on the given device specification.
 * @return a new osx_cdrom_drive_t, or NULL if the device name was invalid.
 */

osx_cdrom_drive_t osx_cdrom_open_drive( const char *devname );

typedef void (*media_changed_callback_t)( osx_cdrom_drive_t drive, gboolean disc_present, void *user_data ); 

/**
 * Set the media changed callback for the drive. (NULL == no callback)
 */
void osx_cdrom_set_media_changed_callback( osx_cdrom_drive_t drive, 
                                           media_changed_callback_t callback, 
                                           void *user_data );

/**
 * Return a file handle for the cdrom drive (actually for the media).
 * @return an open file handle, or -1 if there was no media present or
 * the media could not be opened.
 */
int osx_cdrom_get_media_handle( osx_cdrom_drive_t drive );

void osx_cdrom_release_media_handle( osx_cdrom_drive_t drive );

/** Close on osx_cdrom_drive_t and release all associated resources.
 */
void osx_cdrom_close_drive( osx_cdrom_drive_t drive );

/**
 * Install the notifications and handlers needed by the IOKit support layer.
 * Must be called before trying to use any of the functions above.
 */
gboolean osx_register_iokit_notifications();
/**
 * Uninstall the notifications and handlers in the IOKit support layer
 */
void osx_unregister_iokit_notifications();

#endif /* !lxdream_osx_iokit_H */
