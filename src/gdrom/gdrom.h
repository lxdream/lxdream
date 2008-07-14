/**
 * $Id$
 *
 * This file defines the public structures and functions exported by the 
 * GD-Rom subsystem
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

#ifndef lxdream_gdrom_H
#define lxdream_gdrom_H 1

#include "lxdream.h"
#include "hook.h"
#include <glib/glist.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t gdrom_error_t;


struct gdrom_device {
    char *name;  // internal name
    char *device_name; // Human-readable device name
};

typedef struct gdrom_device *gdrom_device_t;

typedef struct gdrom_disc *gdrom_disc_t;

typedef gboolean (*gdrom_disc_change_hook_t)( gdrom_disc_t new_disc, const gchar *new_disc_name, void *user_data );
DECLARE_HOOK(gdrom_disc_change_hook, gdrom_disc_change_hook_t);

typedef gboolean (*gdrom_drive_list_change_hook_t)( GList *drive_list, void *user_data );
DECLARE_HOOK(gdrom_drive_list_change_hook, gdrom_drive_list_change_hook_t);

/**
 * Open an image file
 */
gdrom_disc_t gdrom_image_open( const gchar *filename );

/**
 * Dump image info
 */
void gdrom_image_dump_info( gdrom_disc_t d );


/**
 * Shortcut to open and mount an image file
 * @return true on success
 */
gboolean gdrom_mount_image( const gchar *filename );

void gdrom_mount_disc( gdrom_disc_t disc );

void gdrom_unmount_disc( void );

gboolean gdrom_is_mounted( void );

gdrom_disc_t gdrom_get_current_disc();

const gchar *gdrom_get_current_disc_name();

uint32_t gdrom_read_sectors( uint32_t sector, uint32_t sector_count,
                             int mode, unsigned char *buf, uint32_t *length );


/**
 * Retrieve the disc table of contents, and write it into the buffer in the 
 * format expected by the DC.
 * @return 0 on success, error code on failure (eg no disc mounted)
 */
gdrom_error_t gdrom_get_toc( unsigned char *buf );

/**
 * Retrieve the short (6-byte) session info, and write it into the buffer.
 * @return 0 on success, error code on failure.
 */
gdrom_error_t gdrom_get_info( unsigned char *buf, int session );

uint8_t gdrom_get_track_no_by_lba( uint32_t lba );


/**
 * Native CD-ROM API - provided by drivers/cd_*.c
 *
 * A device name is either a system special file (most unixes) or a url of the
 * form dvd://<identifier> or cd://<identifier>, where <identifier> is a system
 * defined string that uniquely identifies a particular device.
 */

/**
 * Return a list of gdrom_device_t defining all CD/DVD drives in the host system.
 */
GList *cdrom_get_native_devices();

/**
 * Open a native device given a device name and url method. Eg, for the url dvd://1
 * this function will be invoked with method = "dvd" and name = "1"
 * 
 * @return NULL on failure, otherwise a valid gdrom_disc_t that can be mounted.
 */
gdrom_disc_t cdrom_open_device( const gchar *method, const gchar *name );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_gdrom_H */
