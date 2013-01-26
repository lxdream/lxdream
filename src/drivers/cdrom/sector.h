/**
 * $Id$
 *
 * low-level 'block device' for input to cdrom discs.
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

#ifndef cdrom_sector_H
#define cdrom_sector_H 1

#include <stdio.h>
#include <glib.h>
#include "drivers/cdrom/defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************** Sector Source ******************************/
#define SECTOR_SOURCE_TAG 0x42444556
typedef struct sector_source *sector_source_t;
typedef enum {
    NULL_SECTOR_SOURCE,
    FILE_SECTOR_SOURCE,
    MEM_SECTOR_SOURCE,
    DISC_SECTOR_SOURCE,
    TRACK_SECTOR_SOURCE
} sector_source_type_t;

typedef cdrom_error_t (*sector_source_read_fn_t)(sector_source_t, cdrom_lba_t, cdrom_count_t, unsigned char *outbuf);
typedef cdrom_error_t (*sector_source_read_sectors_fn_t)(sector_source_t, cdrom_lba_t, cdrom_count_t, cdrom_read_mode_t mode,
        unsigned char *outbuf, size_t *length);
typedef void (*sector_source_destroy_fn_t)(sector_source_t);

/**
 * A 'sector source' is a read-only random-access data source that supports
 * reads of arbitrary blocks within their capacity. A block device has a
 * defined mode, block size, and block count.
 *
 * Source are ref-counted, and automatically destroyed when the reference
 * count reaches 0.
 */
struct sector_source {
    uint32_t tag;       /* sector source tag */
    uint32_t ref_count; /* Reference count. Initialized to 0 */
    sector_source_type_t type;

    sector_mode_t mode; /* Implies sector size. */
    cdrom_count_t size; /* Block count */

    /**
     * Read blocks from the device using the native block size.
     * @param buf Buffer to receive the blocks
     * @param block First block to transfer (numbered from 0)
     * @param block_count number of blocks to transfer.
     * @return 0 on success, otherwise an error code.
     */
    sector_source_read_fn_t read_blocks;

    /**
     * Read sectors from the device using the specified read mode, performing any
     * necessary conversions.
     */
    sector_source_read_sectors_fn_t read_sectors;

    /**
     * Release all resources and memory used by the device (note should never
     * be called directly
     */
    sector_source_destroy_fn_t destroy;

};

/**
 * Block device that always returns zeros.
 */
sector_source_t null_sector_source_new( sector_mode_t mode, cdrom_count_t size );

#define FILE_SECTOR_FULL_FILE ((cdrom_count_t)-1)

/**
 * File reader. Last block is 0-padded.
 */
sector_source_t file_sector_source_new_filename( const gchar *filename, sector_mode_t mode,
                                                 uint32_t offset, cdrom_count_t sector_count );
sector_source_t file_sector_source_new( FILE *f, sector_mode_t mode, uint32_t offset, cdrom_count_t sector_count,
                                                gboolean closeOnDestroy );
sector_source_t file_sector_source_new_full( FILE *f, sector_mode_t mode, gboolean closeOnDestroy );

/**
 * Temp-file creator - initially empty. Creates a file in the system temp dir,
 * unlinked on destruction or program exit.
 */
sector_source_t tmpfile_sector_source_new(  sector_mode_t mode );

/**
 * Construct a file source that shares its file descriptor with another
 * file source.
 */
sector_source_t file_sector_source_new_source( sector_source_t ref, sector_mode_t mode, uint32_t offset,
                                               cdrom_count_t sector_count );

/**
 * Change the value of the source's closeOnDestroy flag
 */
void file_sector_source_set_close_on_destroy( sector_source_t ref, gboolean closeOnDestroy );

/**
 * Retrieve the source's underlying FILE
 */
FILE *file_sector_source_get_file( sector_source_t ref );

/**
 * Retrieve the source's underlying file descriptor
 */
int file_sector_source_get_fd( sector_source_t ref );

/** Construct a memory source with the given mode and size */
sector_source_t mem_sector_source_new( sector_mode_t mode, cdrom_count_t size );

/**
 * Construct a memory source using the supplied buffer for data.
 * @param buffer The buffer to read from, which must be at least size * sector_size in length
 * @param mode The sector mode of the data in the buffer, which cannot be SECTOR_UNKNOWN
 * @param size Number of sectors in the buffer
 * @param freeOnDestroy If true, the source owns the buffer and will release it when the
 *   source is destroyed.
 */
sector_source_t mem_sector_source_new_buffer( unsigned char *buffer, sector_mode_t mode, cdrom_count_t size,
                                       gboolean freeOnDestroy );

/**
 * Retrieve the underlying buffer for a memory source
 */
unsigned char *mem_sector_source_get_buffer( sector_source_t source );

/**
 * Increment the reference count for a block device.
 */
void sector_source_ref( sector_source_t device );

/**
 * Unreference a block device. If decremented to 0, the device will be
 * destroyed.
 */
void sector_source_unref( sector_source_t device );

/**
 * Release an unbound block device. If the ref count is 0, the device is
 * destroyed. Otherwise the function has no effect.
 */
void sector_source_release( sector_source_t device );

cdrom_error_t sector_source_read( sector_source_t device, cdrom_lba_t lba, cdrom_count_t block_count, unsigned char *buf );

cdrom_error_t sector_source_read_sectors( sector_source_t device, cdrom_lba_t lba, cdrom_count_t block_count,
                                          cdrom_read_mode_t mode, unsigned char *buf, size_t *length );

/***** Internals for sector source implementations *****/

/**
 * Initialize a new (pre-allocated) sector source
 */
sector_source_t sector_source_init( sector_source_t device, sector_source_type_t type, sector_mode_t mode, cdrom_count_t size,
                        sector_source_read_fn_t readfn, sector_source_destroy_fn_t destroyfn );

/**
 * Default sector source destructor method
 */
void default_sector_source_destroy( sector_source_t device );


/**
 * Extract the necessary fields from a single raw sector for the given read mode.
 * @param raw_sector input raw 2352 byte sector
 * @param mode sector mode and field specification flags
 * @param buf output buffer for sector data
 * @param length output length of sector written to buf
 * @return CDROM_ERROR_OK on success, otherwise an appropriate error code.
 */
cdrom_error_t sector_extract_from_raw( unsigned char *raw_sector, cdrom_read_mode_t mode, unsigned char *buf, size_t *length );

/**
 * Test if the given pointer is a valid sector source
 */
#define IS_SECTOR_SOURCE(dev) ((dev) != NULL && ((sector_source_t)(dev))->tag == SECTOR_SOURCE_TAG)

 /**
  * Test if the given pointer is a valid sector source of the given type
  */
#define IS_SECTOR_SOURCE_TYPE(dev,id) (IS_SECTOR_SOURCE(dev) && ((sector_source_t)(dev))->type == id)



#ifdef __cplusplus
}
#endif

#endif /* !cdrom_sector_H */
