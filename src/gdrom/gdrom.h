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
#include "drivers/cdrom/defs.h"
#include <glib/glist.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gdrom_toc {
    uint32_t track[99];
    uint32_t first, last, leadout;
};

#define GDROM_TOC_SIZE (102*4) /* Size of GDROM TOC structure */
#define GDROM_SESSION_INFO_SIZE 6 /* Size of GDROM session info structure */
#define GDROM_SHORT_STATUS_SIZE 14 /* Size of GDROM short status structure */

typedef gboolean (*gdrom_disc_change_hook_t)( cdrom_disc_t new_disc, const gchar *new_disc_name, void *user_data );
DECLARE_HOOK(gdrom_disc_change_hook, gdrom_disc_change_hook_t);

typedef gboolean (*gdrom_drive_list_change_hook_t)( GList *drive_list, void *user_data );
DECLARE_HOOK(gdrom_drive_list_change_hook, gdrom_drive_list_change_hook_t);

/**
 * Open an image file
 */
cdrom_disc_t gdrom_image_open( const gchar *filename );

/**
 * Shortcut to open and mount an image file
 * @return true on success
 */
gboolean gdrom_mount_image( const gchar *filename, ERROR *err );

void gdrom_mount_disc( cdrom_disc_t disc );

void gdrom_unmount_disc( void );

gboolean gdrom_is_mounted( void );

cdrom_disc_t gdrom_get_current_disc();

const gchar *gdrom_get_current_disc_name();

const gchar *gdrom_get_current_disc_title();

/**
 * Find the track which should be checked for the
 * dreamcast bootstrap - this is the first data track on the last
 * session (where there are at least 2 sessions). If a boot track
 * cannot be found, returns NULL.
 */
cdrom_track_t gdrom_disc_get_boot_track( cdrom_disc_t disc );

/** 
 * Check if the disc contains valid media.
 * @return CDROM_ERROR_OK if disc is present, otherwise CDROM_ERROR_NODISC
 */
cdrom_error_t gdrom_check_media( );

/**
 * Retrieve the disc table of contents, and write it into the buffer in the 
 * format expected by the DC.
 * @param buf Buffer to receive the TOC data, which must be at least
 * GDROM_TOC_SIZE bytes long.
 * @return 0 on success, error code on failure (eg no disc)
 */
cdrom_error_t gdrom_read_toc( unsigned char *buf );

/**
 * Retrieve the short (6-byte) session info, and write it into the buffer.
 * @param session The session to read (numbered from 1), or 0 
 * @param buf Buffer to receive the session data, which must be at least
 * GDROM_SESSION_INFO_SIZE bytes long.
 * @return 0 on success, error code on failure.
 */
cdrom_error_t gdrom_read_session( int session, unsigned char *buf );

/**
 * Generate the position data as returned from a STATUS(1) packet. 
 * @param disc The disc to read
 * @param lba The current head position
 * @param buf The buffer to receive the position data, which must be at least
 * GDROM_SHORT_STATUS_SIZE bytes long.
 * @return 0 on success, error code on failure.
 */
cdrom_error_t gdrom_read_short_status( uint32_t lba, unsigned char *buf );

/**
 * Read sectors from the current disc.
 * @param lba Address of first sector to read
 * @param count Number of sectors to read
 * @param read_mode GDROM format read-mode
 * @param buf Buffer to receive read sectors
 * @param length If not null, will be written with the number of bytes read.
 * @return 0 on success, otherwise error code.
 */
cdrom_error_t gdrom_read_cd( cdrom_lba_t lba, cdrom_count_t count,
                             unsigned read_mode, unsigned char *buf, size_t *length );

cdrom_error_t gdrom_play_audio( cdrom_lba_t lba, cdrom_count_t count );

/**
 * Return the 1-byte status code for the disc (combination of IDE_DISC_* flags)
 */
int gdrom_get_drive_status( );

/**
 * Run GDROM time slice (if any)
 */
void gdrom_run_slice( uint32_t nanosecs );

#ifdef __cplusplus
}
#endif

#endif /* !lxdream_gdrom_H */
