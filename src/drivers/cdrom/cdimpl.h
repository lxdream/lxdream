/**
 * $Id$
 *
 * Copyright (c) 2009 Nathan Keynes.
 *
 * Internal CD-ROM implementation header
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

#ifndef cdrom_cdimpl_H
#define cdrom_cdimpl_H 1

#include "drivers/cdrom/cdrom.h"
#include "drivers/cdrom/drive.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Disc factory implementation, to construct cdrom_disc_t objects from a file
 * (if possible).
 */
typedef struct cdrom_disc_factory {
    /* Human-readable name for the discs constructed by the factory */
    const char *display_name;
    /* Default file extension for supported discs */
    const char *extension;

    /* Test if the given file is facially valid for the factory
     * (that is, it passes all file magic etc checks)
     */
    gboolean (*is_valid_file)(FILE *f);

    /* Perform any additional initialization needed for the disc type
     * (run after the default initialization). May be NULL if no init
     * is needed
     * @return TRUE on success, FALSE on error.
     */ 
    gboolean (*init)(cdrom_disc_t disc, ERROR *err);
    
    /* Read the table of contents from the given file, and update the disc
     * accordingly. On error, set the err message and return FALSE.
     */
    gboolean (*read_toc)(cdrom_disc_t disc, ERROR *err);

} *cdrom_disc_factory_t;

/**
 * Low-level SCSI transport provided to the main SCSI/MMC driver. When used
 * this will be set as the disc->impl_data field.
 * Note: For symmetry there should be a packet_write variant, but we don't
 * currently need it for anything. YAGNI, etc.
 */
typedef struct cdrom_scsi_transport {
        /* Execute a read command (ie a command that returns a block of data in
         * response, not necessarily a CD read).
         * @param disc The disc to execute the command
         * @param cmd  The 12-byte command packet
         * @param buf  The buffer to receive the read results
         * @param length On entry, the size of buf. Modified on exit to the number
         *        of bytes actually read.
         * @return PKT_ERR_OK on success, otherwise the host error code.
         */
        cdrom_error_t (*packet_read)( struct cdrom_disc *disc,
                                      char *cmd, unsigned char *buf,
                                      unsigned int *length );

        /* Execute a generic command that does not write or return any data.
         * (eg play audio).
         * @param scsi The disc to execute the command
         * @param cmd  The 12-byte command packet
         * @return PKT_ERR_OK on success, otherwise the host error code.
         */
        cdrom_error_t (*packet_cmd)( struct cdrom_disc *disc,
                                     char *cmd );

        /* Return TRUE if the media has changed since the last call, otherwise
         * FALSE. This method is used to implement the disc-level check_status
         * and should have no side-effects.
         */
        gboolean (*media_changed)( struct cdrom_disc *disc );
} *cdrom_scsi_transport_t;

#define SCSI_TRANSPORT(disc)  ((cdrom_scsi_transport_t)disc->impl_data)

/**
 * Initialize a previously allocated cdrom_disc_t.
 */
cdrom_disc_t cdrom_disc_init( cdrom_disc_t disc, const char *filename );

/**
 * Allocate and initialize a new cdrom_disc_t with the defaults for image files
 */
cdrom_disc_t cdrom_disc_new( const char *name, ERROR *err );

/**
 * Read the table of contents from a scsi disc.
 */
gboolean cdrom_disc_scsi_read_toc( cdrom_disc_t disc, ERROR *err );

/**
 * Allocate and initialize a new cdrom_disc_t using a scsi transport.
 */
cdrom_disc_t cdrom_disc_scsi_new( const char *name, cdrom_scsi_transport_t transport, ERROR *err );

/**
 * Allocate and initialize a new cdrom_disc_t using a scsi transport and an
 * open file
 */
cdrom_disc_t cdrom_disc_scsi_new_file( FILE *f, const char *filename, cdrom_scsi_transport_t transport, ERROR *err );


void cdrom_disc_scsi_init( cdrom_disc_t disc, cdrom_scsi_transport_t scsi );

/**
 * Compute derived values for the TOC where they have not already been set
 *   - Determine disc leadout from end of the last track
 *   - Set the disc type to the based on the track types present.
 */
void cdrom_disc_finalize_toc( cdrom_disc_t disc );

/**
 * Clear all TOC values in preparation for replacing with a new TOC
 */
void cdrom_disc_clear_toc( cdrom_disc_t disc );

/**
 * Re-read the table of contents of the disc
 */
gboolean cdrom_disc_read_toc( cdrom_disc_t disc, ERROR *err );

/**
 * track source for a host CD-ROM device, for use by host implementations
 */
sector_source_t track_sector_source_new( cdrom_disc_t disc, sector_mode_t mode, cdrom_lba_t lba, cdrom_count_t count );

/**
 * Get the base file used by the cdrom, or NULL if there is no such file.
 */
FILE *cdrom_disc_get_base_file( cdrom_disc_t disc );

#define cdrom_disc_get_base_fd(disc) fileno(cdrom_disc_get_base_file(disc))

/**
 * Default disc destructor method
 */
void default_cdrom_disc_destroy( sector_source_t device );

/******************** Physical drive support *********************/

/**
 * Add a physical drive to the list.
 * @return the new cdrom_drive_t entry. If the drive was already in the list,
 * returns the existing entry instead and does not add a new one.
 */
cdrom_drive_t cdrom_drive_add( const char *name, const char *display_name, cdrom_drive_open_fn_t open_fn );

/**
 * Remove a physical drive from the list, specified by name.
 * @return TRUE if the drive was removed, FALSE if the drive was not in the list.
 */
gboolean cdrom_drive_remove( const char *name );

/**
 * Clear the cdrom drive list.
 */
void cdrom_drive_remove_all();

/************************* MMC support ***************************/

/**
 * Parse a standard MMC format-2 TOC into the disc structure.
 */
void mmc_parse_toc2( cdrom_disc_t disc, unsigned char *buf );

/**
 * Read a standard MMC inquiry response, returning a newly allocated string
 * of the form "<vendor> <product> <revision>"
 */
const char *mmc_parse_inquiry( unsigned char *buf );



#ifdef __cplusplus
}
#endif

#endif /* !cdrom_cdimpl_H */
