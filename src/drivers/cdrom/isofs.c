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

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <glib.h>

#define LIBISOFS_WITHOUT_LIBBURN 1

#include "drivers/cdrom/cdrom.h"
#include "drivers/cdrom/isofs.h"

static int isofs_dummy_fn(IsoDataSource *src)
{
    return 1;
}

static int isofs_read_block(IsoDataSource *src, uint32_t lba, uint8_t *buffer)
{
    sector_source_t source = (sector_source_t)src->data;
    cdrom_error_t err = sector_source_read_sectors(source, lba, 1,
            CDROM_READ_MODE2_FORM1|CDROM_READ_DATA, buffer, NULL );
    if( err != CDROM_ERROR_OK ) {
        return ISO_DATA_SOURCE_FAILURE;
    }
    return 1;
}

static void isofs_release(IsoDataSource *src)
{
    sector_source_unref((sector_source_t)src->data);
}

static IsoDataSource *iso_data_source_new( sector_source_t source )
{
    IsoDataSource *src = g_malloc0(sizeof(IsoDataSource));
    src->refcount = 1;
    src->open = isofs_dummy_fn;
    src->close = isofs_dummy_fn;
    src->read_block = isofs_read_block;
    src->free_data = isofs_release;
    src->data = source;
    sector_source_ref(source);
    return src;
}

static void iso_error_convert( int status, ERROR *err )
{
    switch( status ) {
    case ISO_SUCCESS:
        err->code = LX_ERR_NONE;
        break;
    case ISO_OUT_OF_MEM:
        SET_ERROR( err, LX_ERR_NOMEM, "Unable to allocate memory for ISO filesystem" );
        break;
    case ISO_FILE_READ_ERROR:
        SET_ERROR( err, LX_ERR_FILE_IOERROR, "Read error" );
        break;
    default:
        SET_ERROR( err, LX_ERR_UNHANDLED, "Unknown error in ISO filesystem" );
        break;
    }
}

/**
 * Construct an isofs image from an existing sector source.
 */
IsoImage *iso_image_new_from_source( sector_source_t source, cdrom_lba_t start, ERROR *err )
{
    IsoImage *iso = NULL;
    IsoReadOpts *opts;
    IsoDataSource *src;

    int status = iso_image_new(NULL, &iso);
    if( status != 1 )
        return NULL;

    status = iso_read_opts_new(&opts,0);
    if( status != 1 ) {
        iso_image_unref( iso );
        return NULL;
    }

    iso_read_opts_set_start_block(opts, start);
    src = iso_data_source_new(source);
    status = iso_image_import(iso, src, opts, NULL);
    iso_data_source_unref(src);
    iso_read_opts_free(opts);
    if( status != 1 ) {
        iso_image_unref(iso);
        return NULL;
    }
    return iso;
}

IsoImage *iso_image_new_from_disc( cdrom_disc_t disc, cdrom_lba_t start_sector, ERROR *err )
{
    return iso_image_new_from_source( &disc->source, start_sector, err );
}

IsoImage *iso_image_new_from_track( cdrom_disc_t disc, cdrom_track_t track, ERROR *err )
{
    return iso_image_new_from_source( &disc->source, track->lba, err );
}


IsoImageFilesystem *iso_filesystem_new_from_source( sector_source_t source, cdrom_lba_t start, ERROR *err )
{
    IsoImageFilesystem *iso = NULL;
    IsoReadOpts *opts;
    IsoDataSource *src;

    int status = iso_read_opts_new(&opts,0);
    if( status != 1 ) {
        iso_error_convert(status, err);
        return NULL;
    }

    iso_read_opts_set_start_block(opts, start);
    src = iso_data_source_new(source);
    status = iso_image_filesystem_new(src, opts, 0x1FFFFF, &iso);
    iso_data_source_unref(src);
    iso_read_opts_free(opts);
    if( status != 1 ) {
        iso_error_convert(status, err);
        return NULL;
    }
    return iso;

}

IsoImageFilesystem *iso_filesystem_new_from_disc( cdrom_disc_t disc, cdrom_lba_t start_sector, ERROR *err )
{
    return iso_filesystem_new_from_source( &disc->source, start_sector, err );
}

IsoImageFilesystem *iso_filesystem_new_from_track( cdrom_disc_t disc, cdrom_track_t track, ERROR *err )
{
    return iso_filesystem_new_from_source( &disc->source, track->lba, err );
}


/**
 * Construct a sector source from a given IsoImage.
 */
sector_source_t iso_sector_source_new( IsoImage *image, sector_mode_t mode, cdrom_lba_t start_sector,
                                       const char *bootstrap, ERROR *err )
{
    assert( mode == SECTOR_MODE1 || mode == SECTOR_MODE2_FORM1 );

    IsoWriteOpts *opts;
    struct burn_source *burn;

    int status = iso_write_opts_new(&opts, 0);
    if( status != 1 ) {
        iso_error_convert(status, err);
        return NULL;
    }
    iso_write_opts_set_appendable(opts,0);
    iso_write_opts_set_ms_block(opts, start_sector);

    status = iso_image_create_burn_source(image, opts, &burn);
    iso_write_opts_free(opts);
    if( status != 1 ) {
        iso_error_convert(status, err);
        return NULL;
    }

    off_t size = burn->get_size(burn);
    sector_source_t source = tmpfile_sector_source_new(mode);
    if( source == NULL ) {
        burn->free_data(burn);
        free(burn);
        return NULL;
    }

    char buf[2048];
    cdrom_count_t expect = size/2048;
    cdrom_count_t count = 0;
    int fd = file_sector_source_get_fd(source);
    source->size = expect;
    lseek( fd, 0, SEEK_SET );
    write( fd, bootstrap, 32768 );
    for( cdrom_count_t count = 0; count < expect; count++ ) {
        if( burn->read == NULL ) {
            status = burn->read_xt(burn, buf, 2048);
        } else {
            status = burn->read(burn, buf, 2048);
        }
        if( status == 0 ) {
            /* EOF */
            break;
        } else if( status != 2048 ) {
            /* Error */
            sector_source_unref(source);
            source = NULL;
            break;
        }
        /* Discard first 16 sectors, replaced with the bootstrap */
        if( count >= (32768/2048) )
            write( fd, buf, 2048 );
    }
    burn->free_data(burn);
    free(burn);
    return source;
}

