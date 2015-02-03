/**
 * $Id$
 *
 * Host CD/DVD drive support.
 *
 * This module supplies functions to enumerate the physical drives in the
 * host system, and open them as a cdrom disc.
 *
 * Note that cdrom_disc_t objects bound to a physical drive may update their
 * TOC at any time, including setting disc_type to CDROM_DISC_NONE (to indicate
 * no media present).
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

#ifndef cdrom_drive_H
#define cdrom_drive_H 1

#include <glib.h>
#include "hook.h"
#include "drivers/cdrom/defs.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct cdrom_drive *cdrom_drive_t;

typedef cdrom_disc_t (*cdrom_drive_open_fn_t)(cdrom_drive_t, ERROR *);

/**
 * A cdrom_device is a placeholder for a physical CD/DVD drive in the host
 * system.
 */
struct cdrom_drive {
    /**
     * System name for the device
     */
    const char *name;
    /**
     * Human-readable name of the device - normally the device's vendor
     * and product name as returned by an Inquiry request.
     */
    const char *display_name;

    /**
     * Implementation specific function to open the drive, returning a new
     * cdrom_disc_t.
     */
    cdrom_drive_open_fn_t open;
};

typedef gboolean (*cdrom_drive_list_change_hook_t)( GList *drive_list, void *user_data );
DECLARE_HOOK(cdrom_drive_list_change_hook, cdrom_drive_list_change_hook_t);


/**
 * Native CD-ROM API - provided by drivers/cd_*.c
 *
 * A device name is either a system special file (most unixes) or a url of the
 * form dvd://<identifier> or cd://<identifier>, where <identifier> is a system
 * defined string that uniquely identifies a particular device.
 */

/**
 * Return a list of cdrom_drive_t defining all CD/DVD drives in the host system.
 */
GList *cdrom_drive_get_list();

/**
 *
 */
cdrom_drive_t cdrom_drive_find( const char *name );

/**
 * Open a cdrom_drive_t previously obtained from the system.
 *
 * @return NULL on failure, otherwise a valid cdrom_disc_t that can be mounted.
 */
cdrom_disc_t cdrom_drive_open( cdrom_drive_t drive, ERROR *err );

/**
 * Scan the system for physical host CD-ROM devices (Platform-specific implementation)
 */
void cdrom_drive_scan();

#ifdef __cplusplus
}
#endif

#endif /* !cdrom_drive_H */
