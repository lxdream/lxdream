/**
 * $Id$
 *
 * libisofs adapter
 *
 * Copyright (c) 2010 Nathan Keynes.
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

#ifndef cdrom_isofs_H
#define cdrom_isofs_H 1

#include <stdint.h>
#include <libisofs.h>
#include "drivers/cdrom/sector.h"

/**
 * Construct an IsoFilesystem from an existing sector source
 */
IsoImageFilesystem *iso_filesystem_new_from_source( sector_source_t track, cdrom_lba_t start, ERROR *err );
IsoImageFilesystem *iso_filesystem_new_from_disc( cdrom_disc_t disc, cdrom_lba_t start, ERROR *err );
IsoImageFilesystem *iso_filesystem_new_from_track( cdrom_disc_t disc, cdrom_track_t track, ERROR *err );


/**
 * Convenience function to read an entire IsoFileSource
 */
int iso_source_file_read_all( IsoFileSource *file, unsigned char *buf, size_t max_size );

/**
 * Construct an IsoImage image from an existing sector source, for use in
 * creating a modified image
 */
IsoImage *iso_image_new_from_source( sector_source_t track, cdrom_lba_t start, ERROR *err );

/**
 * Construct an IsoImage from a cdrom disc and sector position.
 * @return a new isofs_reader, or NULL on an error.
 */
IsoImage *iso_image_new_from_disc( cdrom_disc_t disc, cdrom_lba_t start_sector, ERROR *err );

IsoImage *iso_image_new_from_track( cdrom_disc_t disc, cdrom_track_t track, ERROR *err );

/**
 * Construct a sector source from a given IsoImage.
 */
sector_source_t iso_sector_source_new( IsoImage *image, sector_mode_t mode, cdrom_lba_t start_sector,
                                       const char *bootstrap, ERROR *err );


/** Prototypes for "Internal" Libisofs functions */
int iso_mem_stream_new(unsigned char *buf, size_t size, IsoStream **stream);

#endif /* !cdrom_isofs_H */
