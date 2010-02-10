/**
 * $Id$
 *
 * low-level 'block device' for input to gdrom discs.
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

#include <sys/stat.h>
#include <glib/gmem.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "drivers/cdrom/sector.h"
#include "drivers/cdrom/cdrom.h"
#include "drivers/cdrom/ecc.h"

#define CHECK_READ(dev,lba,count) \
    if( !IS_SECTOR_SOURCE(dev) ) { \
        return CDROM_ERROR_NODISC; \
    } else if( (dev)->size != 0 && ((lba) >= (dev)->size || (lba+block_count) > (dev)->size) ) { \
        return CDROM_ERROR_BADREAD; \
    }

/* Default read mode for each sector mode */
const uint32_t cdrom_sector_read_mode[] = { 0,
        CDROM_READ_CDDA|CDROM_READ_DATA, CDROM_READ_MODE1|CDROM_READ_DATA,
        CDROM_READ_MODE2|CDROM_READ_DATA, CDROM_READ_MODE2_FORM1|CDROM_READ_DATA,
        CDROM_READ_MODE2_FORM1|CDROM_READ_DATA,
        CDROM_READ_MODE2|CDROM_READ_DATA|CDROM_READ_SUBHEADER|CDROM_READ_ECC,
        CDROM_READ_RAW, CDROM_READ_RAW };

/* Block size for each sector mode */
const uint32_t cdrom_sector_size[] = { 0, 2352, 2048, 2336, 2048, 2324, 2336, 2352, 2352 };

const char *cdrom_sector_mode_names[] = { "Unknown", "Audio", "Mode 1", "Mode 2", "Mode 2 Form 1", "Mode 2 Form 2",
        "Mode 2 semiraw", "XA Raw", "Non-XA Raw" };


/********************* Public functions *************************/
cdrom_error_t sector_source_read( sector_source_t device, cdrom_lba_t lba, cdrom_count_t block_count, unsigned char *buf )
{
    CHECK_READ(device,lba,block_count);
    return device->read_blocks(device, lba, block_count, buf);
}

cdrom_error_t sector_source_read_sectors( sector_source_t device, cdrom_lba_t lba, cdrom_count_t block_count, cdrom_read_mode_t mode,
                                          unsigned char *buf, size_t *length )
{
    CHECK_READ(device,lba,block_count);
    return device->read_sectors(device, lba, block_count, mode, buf, length);
}

void sector_source_ref( sector_source_t device )
{
    assert( IS_SECTOR_SOURCE(device) );
    device->ref_count++;
}

void sector_source_unref( sector_source_t device )
{
    if( device == NULL )
        return;
    assert( IS_SECTOR_SOURCE(device) );
    if( device->ref_count > 0 )
        device->ref_count--;
    if( device->ref_count == 0 )
        device->destroy(device);
}

void sector_source_release( sector_source_t device )
{
    assert( IS_SECTOR_SOURCE(device) );
    if( device->ref_count == 0 )
        device->destroy(device);
}

/************************** Sector mangling ***************************/
/*
 * Private functions used to pack/unpack sectors, determine mode, and
 * evaluate sector reads.
 */

/** Basic data sector header structure */
struct cdrom_sector_header {
    uint8_t sync[12];
    uint8_t msf[3];
    uint8_t mode;
    uint8_t subhead[8]; // Mode-2 XA sectors only
};

static const uint8_t cdrom_sync_data[12] = { 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0 };

/* Field combinations that are legal for mode 1 or mode 2 (formless) reads */
static const uint8_t legal_nonxa_fields[] =
{ TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE,
  TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE,
  TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE,
  FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE };

/* Field combinations that are legal for mode 2 form 1 or form 2 reads */
static const uint8_t legal_xa_fields[] =
{ TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE,
  TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE,
  TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE,
  FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, TRUE, TRUE };

/**
 * Position per sector mode of each of the fields
 *   sync, header, subheader, data, ecc.
 *
 */
static const uint32_t sector_field_positions[][6] = {
        { 0, 0, 0, 0, 0, 0 },    /* Unknown */
        { 0, 0, 0, 0, 2352, 2352 }, /* CDDA */
        { 0, 12, 16, 16, 2064, 2352 }, /* Mode 1 */
        { 0, 12, 16, 16, 2352, 2352 }, /* Mode 2 formless */
        { 0, 12, 16, 24, 2072, 2352 }, /* Mode 2 form 1 */
        { 0, 12, 16, 24, 2352, 2352 }}; /* Mode 2 form 2 */



/**
 * Return CDROM_ERROR_OK if the given read mode + sector modes are compatible,
 * otherwise either CDROM_ERROR_BADREADMODE or CDROM_ERROR_BADFIELD. Raw sector modes
 * will return BADREADMODE, as it's impossible to tell.
 *
 * @param track_mode one of the CDROM_MODE* constants
 * @param read_mode the full read mode
 */
static cdrom_error_t is_legal_read( sector_mode_t sector_mode, cdrom_read_mode_t read_mode )
{
    int read_sector_type = CDROM_READ_TYPE(read_mode);
    int read_sector_fields = CDROM_READ_FIELDS(read_mode);

    /* Check the sector type is consistent */
    switch( read_sector_type ) {
    case CDROM_READ_ANY: break;
    case CDROM_READ_CDDA:
        if( sector_mode != SECTOR_CDDA )
            return CDROM_ERROR_BADREADMODE;
        break;
    case CDROM_READ_MODE1:
    case CDROM_READ_MODE2_FORM1:
        if( sector_mode != SECTOR_MODE1 && sector_mode != SECTOR_MODE2_FORM1 )
            return CDROM_ERROR_BADREADMODE;
        break;
    case CDROM_READ_MODE2_FORM2:
        if( sector_mode != SECTOR_MODE2_FORM2 )
            return CDROM_ERROR_BADREADMODE;
        break;
    case CDROM_READ_MODE2:
        if( sector_mode != SECTOR_MODE2_FORMLESS )
            return CDROM_ERROR_BADREADMODE;
        break;
    default: /* Illegal read mode */
        return CDROM_ERROR_BADFIELD;
    }

    /* Check the fields requested are sane per MMC (non-contiguous regions prohibited) */
    switch( sector_mode ) {
    case SECTOR_CDDA:
        return CDROM_ERROR_OK; /* Everything is OK */
    case SECTOR_MODE2_FORM1:
    case SECTOR_MODE2_FORM2:
        if( !legal_xa_fields[read_sector_fields>>11] )
            return CDROM_ERROR_BADFIELD;
        else
            return CDROM_ERROR_OK;
    case SECTOR_MODE1:
    case SECTOR_MODE2_FORMLESS:
        if( !legal_nonxa_fields[read_sector_fields>>11] )
            return CDROM_ERROR_BADFIELD;
        else
            return CDROM_ERROR_OK;
    default:
        return CDROM_ERROR_BADFIELD;
    }
}

static sector_mode_t identify_sector( sector_mode_t raw_mode, unsigned char *buf )
{
    struct cdrom_sector_header *header = (struct cdrom_sector_header *)buf;

    switch( raw_mode ) {
    case SECTOR_SEMIRAW_MODE2: /* XA sectors */
    case SECTOR_RAW_XA:
        switch( header->mode ) {
        case 1: return SECTOR_MODE1;
        case 2: return ((header->subhead[2] & 0x20) == 0 ) ? SECTOR_MODE2_FORM1 : SECTOR_MODE2_FORM2;
        default: return SECTOR_UNKNOWN;
        }
    case SECTOR_RAW_NONXA:
        switch( header->mode ) {
        case 1: return SECTOR_MODE1;
        case 2: return SECTOR_MODE2_FORMLESS;
        default: return SECTOR_UNKNOWN;
        }
    default:
        return raw_mode;
    }
}

/**
 * Read a single raw sector from the device. Generate sync, ECC/EDC data etc where
 * necessary.
 */
static cdrom_error_t read_raw_sector( sector_source_t device, cdrom_lba_t lba, unsigned char *buf )
{
    cdrom_error_t err;

    switch( device->mode ) {
    case SECTOR_RAW_XA:
    case SECTOR_RAW_NONXA:
        return device->read_blocks(device, lba, 1, buf);
    case SECTOR_SEMIRAW_MODE2:
        memcpy( buf, cdrom_sync_data, 12 );
        cd_build_address(buf, SECTOR_MODE2_FORMLESS, lba);
        return device->read_blocks(device, lba, 1, &buf[16]);
    case SECTOR_MODE1:
    case SECTOR_MODE2_FORMLESS:
        err = device->read_blocks(device, lba, 1, &buf[16]);
        if( err == CDROM_ERROR_OK ) {
            do_encode_L2( buf, device->mode, lba );
        }
        return err;
    case SECTOR_MODE2_FORM1:
        *((uint32_t *)(buf+16)) = *((uint32_t *)(buf+20)) = 0;
        err = device->read_blocks(device, lba, 1, &buf[24]);
        if( err == CDROM_ERROR_OK ) {
            do_encode_L2( buf, device->mode, lba );
        }
        return err;
    case SECTOR_MODE2_FORM2:
        *((uint32_t *)(buf+16)) = *((uint32_t *)(buf+20)) = 0x00200000;
        err = device->read_blocks(device, lba, 1, &buf[24]);
        if( err == CDROM_ERROR_OK ) {
            do_encode_L2( buf, device->mode, lba );
        }
        return err;
    default:
        abort();
    }
}

static cdrom_error_t extract_sector_fields( unsigned char *raw_sector, sector_mode_t mode, int fields, unsigned char *buf, size_t *length )
{
    int start=-1,end=0;
    int i;

    for( i=0; i<5; i++ ) {
        if( fields & (0x8000>>i) ) {
            if( start == -1 )
                start = sector_field_positions[mode][i];
            else if( end != sector_field_positions[mode][i] )
                return CDROM_ERROR_BADFIELD;
            end = sector_field_positions[mode][i+1];
        }
    }
    if( start == -1 ) {
        *length = 0;
    } else {
        memcpy( buf, &raw_sector[start], end-start );
        *length = end-start;
    }
    return CDROM_ERROR_OK;
}

cdrom_error_t sector_extract_from_raw( unsigned char *raw_sector, cdrom_read_mode_t mode, unsigned char *buf, size_t *length )
{
    sector_mode_t sector_mode = identify_sector( SECTOR_RAW_XA, raw_sector );
    if( sector_mode == SECTOR_UNKNOWN )
        return CDROM_ERROR_BADREAD;
    cdrom_error_t status = is_legal_read( sector_mode, mode );
    if( status != CDROM_ERROR_OK )
        return status;
    return extract_sector_fields( raw_sector, sector_mode, CDROM_READ_FIELDS(mode), buf, length );
}

/**
 * This is horribly complicated by the need to handle mapping between all possible
 * sector modes + read modes, but fortunately most sources can just supply
 * a single block type and not care about the details here.
 */
cdrom_error_t default_sector_source_read_sectors( sector_source_t device,
        cdrom_lba_t lba, cdrom_count_t block_count, cdrom_read_mode_t mode,
        unsigned char *buf, size_t *length )
{
    unsigned char tmp[CDROM_MAX_SECTOR_SIZE];
    int read_sector_type = CDROM_READ_TYPE(mode);
    int read_sector_fields = CDROM_READ_FIELDS(mode);
    int i;
    size_t len = 0;
    cdrom_error_t err;

    CHECK_READ(device, lba, count);

    switch(device->mode) {
    case SECTOR_CDDA:
        if( read_sector_type != CDROM_READ_ANY && read_sector_type != CDROM_READ_CDDA )
            return CDROM_ERROR_BADREADMODE;
        if( read_sector_fields != 0 ) {
            len = block_count * CDROM_MAX_SECTOR_SIZE;
            device->read_blocks( device, lba, block_count, buf );
        }
        break;
    case SECTOR_RAW_XA:
    case SECTOR_RAW_NONXA:
    case SECTOR_SEMIRAW_MODE2:
        /* We (may) have to break the raw sector up into requested fields.
         * Process sectors one at a time
         */
        for( i=0; i<block_count; i++ ) {
            size_t tmplen;
            err = read_raw_sector( device, lba+i, tmp );
            if( err != CDROM_ERROR_OK )
                return err;
            err = sector_extract_from_raw( tmp, mode, &buf[len], &tmplen );
            if( err != CDROM_ERROR_OK )
                return err;
            len += tmplen;
        }
        break;
    default: /* Data-only blocks */
        err = is_legal_read( device->mode, mode );
        if( err != CDROM_ERROR_OK )
            return err;
        if( read_sector_fields == 0 ) { /* Read nothing */
            if( length != NULL )
                *length = 0;
            return CDROM_ERROR_OK;
        } else if( read_sector_fields == CDROM_READ_DATA ) {
            /* Data-only */
            if( length != NULL )
                *length = block_count * CDROM_SECTOR_SIZE(device->mode);
            return device->read_blocks( device, lba, block_count, buf );
        } else if( read_sector_fields == CDROM_READ_RAW ) {
            for( i=0; i<block_count; i++ ) {
                err = read_raw_sector( device, lba+i, &buf[2352*i] );
                if( err != CDROM_ERROR_OK )
                    return err;
            }
            len = block_count * CDROM_MAX_SECTOR_SIZE;
        } else {
            for( i=0; i<block_count; i++ ) {
                size_t tmplen;
                err = read_raw_sector( device, lba+i, tmp );
                if( err != CDROM_ERROR_OK )
                    return err;
                err = extract_sector_fields( tmp, device->mode, read_sector_fields, &buf[len], &tmplen );
                if( err != CDROM_ERROR_OK )
                    return err;
                len += tmplen;
            }
        }
    }
    if( length != NULL )
        *length = len;
    return CDROM_ERROR_OK;

}

/************************ Base implementation *************************/

/**
 * Default destroy implementation - clears the tag and frees memory.
 */
void default_sector_source_destroy( sector_source_t device )
{
    assert( device != NULL && device->ref_count == 0 );
    device->tag = 0;
    g_free( device );
}

sector_source_t sector_source_init( sector_source_t device, sector_source_type_t type, sector_mode_t mode, cdrom_count_t size,
                        sector_source_read_fn_t readfn, sector_source_destroy_fn_t destroyfn )
{
    device->tag = SECTOR_SOURCE_TAG;
    device->ref_count = 0;
    device->type = type;
    device->mode = mode;
    device->size = size;
    device->read_blocks = readfn;
    device->read_sectors = default_sector_source_read_sectors;
    if( destroyfn == NULL )
        device->destroy = default_sector_source_destroy;
    else
        device->destroy = destroyfn;
    return device;
}

/************************ Null device implementation *************************/
cdrom_error_t null_sector_source_read( sector_source_t device, cdrom_lba_t lba, cdrom_count_t block_count, unsigned char *buf )
{
    memset( buf, 0,  block_count*CDROM_SECTOR_SIZE(device->mode) );
    return CDROM_ERROR_OK;
}

sector_source_t null_sector_source_new( sector_mode_t mode, cdrom_count_t size )
{
    return sector_source_init( g_malloc(sizeof(struct sector_source)), NULL_SECTOR_SOURCE, mode, size,
            null_sector_source_read, default_sector_source_destroy );
}

/************************ File device implementation *************************/
typedef struct file_sector_source {
    struct sector_source dev;
    FILE *file;
    uint32_t offset; /* offset in file where source begins */
    sector_source_t ref; /* Parent source reference */
    gboolean closeOnDestroy;
} *file_sector_source_t;

void file_sector_source_destroy( sector_source_t dev )
{
    assert( IS_SECTOR_SOURCE_TYPE(dev,FILE_SECTOR_SOURCE) );
    file_sector_source_t fdev = (file_sector_source_t)dev;

    if( fdev->closeOnDestroy && fdev->file != NULL ) {
        fclose( fdev->file );
    }
    sector_source_unref( fdev->ref );
    fdev->file = NULL;
    default_sector_source_destroy(dev);
}

cdrom_error_t file_sector_source_read( sector_source_t dev, cdrom_lba_t lba, cdrom_count_t block_count, unsigned char *buf )
{
    assert( IS_SECTOR_SOURCE_TYPE(dev,FILE_SECTOR_SOURCE) );
    file_sector_source_t fdev = (file_sector_source_t)dev;

    uint32_t off = fdev->offset + lba * CDROM_SECTOR_SIZE(dev->mode);
    uint32_t size = block_count * CDROM_SECTOR_SIZE(dev->mode);
    fseek( fdev->file, off, SEEK_SET );

    size_t len = fread( buf, 1, size, fdev->file );
    if( len == -1 ) {
        return CDROM_ERROR_READERROR;
    } else if( len < size ) {
        /* zero-fill */
        memset( buf + len, 0, size-len );
    }
    return CDROM_ERROR_OK;
}

sector_source_t file_sector_source_new( FILE *f, sector_mode_t mode, uint32_t offset,
                                        cdrom_count_t sector_count, gboolean closeOnDestroy )
{
    if( sector_count == FILE_SECTOR_FULL_FILE ) {
        unsigned int sector_size = CDROM_SECTOR_SIZE(mode);
        if( sector_size == 0 )
            sector_size = 2048;
        struct stat st;

        if( f == NULL || fstat( fileno(f), &st ) != 0 ) {
            /* can't stat file? */
            return NULL;
        }

        sector_count = (st.st_size + sector_size-1) / sector_size;
    }

    file_sector_source_t dev = g_malloc(sizeof(struct file_sector_source));
    dev->file = f;
    dev->offset = offset;
    dev->closeOnDestroy = closeOnDestroy;
    dev->ref = NULL;
    return sector_source_init( &dev->dev, FILE_SECTOR_SOURCE, mode,  sector_count, file_sector_source_read, file_sector_source_destroy );
}

sector_source_t file_sector_source_new_full( FILE *f, sector_mode_t mode, gboolean closeOnDestroy )
{
    return file_sector_source_new( f, mode, 0, FILE_SECTOR_FULL_FILE, closeOnDestroy );
}

sector_source_t file_sector_source_new_filename( const gchar *filename, sector_mode_t mode, uint32_t offset,
                                                 cdrom_count_t sector_count )
{
    int fd = open( filename, O_RDONLY|O_NONBLOCK );
    if( fd == -1 ) {
        return NULL;
    }
    FILE *f = fdopen( fd , "ro" );
    if( f == NULL ) {
        close(fd);
        return NULL;
    } else {
        return file_sector_source_new( f, mode, offset, sector_count, TRUE );
    }
}

sector_source_t file_sector_source_new_source( sector_source_t ref, sector_mode_t mode, uint32_t offset,
                                               cdrom_count_t sector_count )
{
    assert( IS_SECTOR_SOURCE_TYPE(ref,FILE_SECTOR_SOURCE) );
    file_sector_source_t fref = (file_sector_source_t)ref;

    sector_source_t source = file_sector_source_new( fref->file, mode, offset, sector_count, FALSE );
    ((file_sector_source_t)source)->ref = ref;
    sector_source_ref(ref);
    return source;
}

FILE *file_sector_source_get_file( sector_source_t ref )
{
    assert( IS_SECTOR_SOURCE_TYPE(ref,FILE_SECTOR_SOURCE) );
    file_sector_source_t fref = (file_sector_source_t)ref;
    return fref->file;
}

int file_sector_source_get_fd( sector_source_t ref )
{
    return fileno(file_sector_source_get_file(ref));
}

void file_sector_source_set_close_on_destroy( sector_source_t ref, gboolean closeOnDestroy )
{
    assert( IS_SECTOR_SOURCE_TYPE(ref,FILE_SECTOR_SOURCE) );
    file_sector_source_t fref = (file_sector_source_t)ref;
    fref->closeOnDestroy = closeOnDestroy;
}

/************************ Track device implementation *************************/
typedef struct track_sector_source {
    struct sector_source dev;
    cdrom_disc_t disc;
    uint32_t start_lba;
} *track_sector_source_t;

cdrom_error_t track_sector_source_read_blocks( sector_source_t dev, cdrom_lba_t lba, cdrom_count_t count,
                                               unsigned char *out )
{
    size_t length;
    assert( IS_SECTOR_SOURCE_TYPE(dev,TRACK_SECTOR_SOURCE) );
    assert( dev->mode != SECTOR_UNKNOWN );
    track_sector_source_t tdev = (track_sector_source_t)dev;
    return cdrom_disc_read_sectors( tdev->disc, lba + tdev->start_lba, count, CDROM_SECTOR_READ_MODE(dev->mode), out, &length );
}

cdrom_error_t track_sector_source_read_sectors( sector_source_t dev, cdrom_lba_t lba, cdrom_count_t count,
                                                cdrom_read_mode_t mode, unsigned char *out, size_t *length )
{
    assert( IS_SECTOR_SOURCE_TYPE(dev,TRACK_SECTOR_SOURCE) );
    track_sector_source_t tdev = (track_sector_source_t)dev;

    return cdrom_disc_read_sectors( tdev->disc, lba + tdev->start_lba, count, mode, out, length );
}

void track_sector_source_destroy( sector_source_t dev )
{
    assert( IS_SECTOR_SOURCE_TYPE(dev,TRACK_SECTOR_SOURCE) );
    track_sector_source_t tdev = (track_sector_source_t)dev;
    sector_source_unref( &tdev->disc->source );
    default_sector_source_destroy(dev);
}

sector_source_t track_sector_source_new( cdrom_disc_t disc, sector_mode_t mode, cdrom_lba_t lba, cdrom_count_t count )
{
    if( disc == NULL ) {
        return NULL;
    }
    track_sector_source_t dev = g_malloc(sizeof(struct track_sector_source));
    dev->disc = disc;
    dev->start_lba = lba;
    sector_source_init( &dev->dev, TRACK_SECTOR_SOURCE, mode, count,
                        track_sector_source_read_blocks, track_sector_source_destroy );
    dev->dev.read_sectors = track_sector_source_read_sectors;
    sector_source_ref( &disc->source );
    return &dev->dev;
}
