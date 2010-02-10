/**
 * $Id$
 *
 * ISO9660 filesystem reading support
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


#ifndef cdrom_isoread_H
#define cdrom_isoread_H 1

#include "drivers/cdrom/defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct isofs_reader_dir *isofs_reader_dir_t;

typedef struct isofs_reader_dirent {
    const char *name;
    size_t size;
    gboolean is_dir;

    cdrom_lba_t start_lba;
    size_t xa_size;
    unsigned interleave_gap;
    unsigned interleave_size;
    isofs_reader_dir_t subdir;
} *isofs_reader_dirent_t;

/**
 * ISO9600 filesystem reader.
 */
typedef struct isofs_reader *isofs_reader_t;

/**
 * Construct an isofs reader from an existing sector source. On error, returns
 * NULL.
 */
isofs_reader_t isofs_reader_new_from_source( sector_source_t track, ERROR *err );

/**
 * Construct an isofs from a cdrom disc and sector position.
 * @return a new isofs_reader, or NULL on an error (and sets err).
 */
isofs_reader_t isofs_reader_new_from_disc( cdrom_disc_t disc, cdrom_lba_t start_sector, ERROR *err );

isofs_reader_t isofs_reader_new_from_track( cdrom_disc_t disc, cdrom_track_t track, ERROR *err );

/**
 * Destroy an isofs reader.
 */
void isofs_reader_destroy( isofs_reader_t reader );

/**
 * Read 0 or more 2048-byte sectors from the filesystem.
 */
cdrom_error_t isofs_reader_read_sectors( isofs_reader_t iso, cdrom_lba_t sector, cdrom_count_t count,
                                         unsigned char *buf );


/**
 * Search the filesystem for the specific fully-qualified file.
 * @return FALSE if the file could not be found, otherwise TRUE and the iterator
 * is updated to point to the requested file.
 */
isofs_reader_dirent_t isofs_reader_get_file( isofs_reader_t iso, const char *filename );

cdrom_error_t isofs_reader_read_file( isofs_reader_t iso, isofs_reader_dirent_t file,
                                      size_t offset, size_t byte_count, unsigned char *buf );

/**
 * Print an isofs directory to the given stream (mostly for debugging purposes)
 */
void isofs_reader_print_dir( FILE *f, isofs_reader_dir_t dir );

isofs_reader_dir_t isofs_reader_get_root_dir( isofs_reader_t iso );

#ifdef __cplusplus
}
#endif

#endif /* !cdrom_isoread_H */
